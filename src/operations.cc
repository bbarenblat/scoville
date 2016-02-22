// Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
// Copyright (C) 2011  Sebastian Pipping <sebastian@pipping.org>
// Copyright (C) 2016  Benjamin Barenblat <benjamin@barenblat.name>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.

#include "operations.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <experimental/optional>
#include <memory>
#include <new>
#include <system_error>
#include <type_traits>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <glog/logging.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "fuse.h"
#include "posix_extras.h"

namespace scoville {

namespace {

// Pointer to the directory underlying the mount point.
File* root_;

mode_t DirectoryTypeToFileType(const unsigned char type) noexcept {
  return static_cast<mode_t>(DTTOIF(type));
}

void* Initialize(fuse_conn_info*) noexcept { return nullptr; }

void Destroy(void*) noexcept {}

int Getattr(const char* const path, struct stat* output) noexcept {
  try {
    if (path[0] == '\0') {
      LOG(ERROR) << "getattr: called on path not starting with /";
      return -ENOENT;
    }

    if (std::strcmp(path, "/") == 0) {
      // They're asking for information about the mount point.
      *output = root_->Stat();
      return 0;
    }

    // Trim the leading slash so LinkStatAt will treat it relative to root_.
    LOG(INFO) << "getattr: trimming leading slash";
    *output = root_->LinkStatAt(path + 1);
    return 0;
  } catch (const std::system_error& e) {
    return -e.code().value();
  } catch (...) {
    LOG(ERROR) << "getattr: caught unexpected value";
    return -ENOTRECOVERABLE;
  }
}

int Fgetattr(const char*, struct stat* const output,
             struct fuse_file_info* const file_info) noexcept {
  try {
    *output = reinterpret_cast<File*>(file_info->fh)->Stat();
    return 0;
  } catch (const std::system_error& e) {
    return -e.code().value();
  } catch (...) {
    LOG(ERROR) << "getattr: caught unexpected value";
    return -ENOTRECOVERABLE;
  }
}

template <typename T>
int OpenResource(const char* const path, const int flags,
                 uint64_t* const handle) noexcept {
  try {
    std::unique_ptr<T> t(new T(
        std::strcmp(path, "/") == 0
            ?
            // They're asking to open the mount point.
            *root_
            :
            // Trim the leading slash so OpenAt will treat it relative to root_.
            root_->OpenAt(path + 1, flags)));

    static_assert(sizeof(*handle) == sizeof(std::uintptr_t),
                  "FUSE file handles are a different size than pointers");
    *handle = reinterpret_cast<std::uintptr_t>(t.release());
    return 0;
  } catch (const std::bad_alloc&) {
    return -ENOMEM;
  } catch (const std::system_error& e) {
    return -e.code().value();
  } catch (...) {
    LOG(ERROR) << "caught unexpected value";
    return -ENOTRECOVERABLE;
  }
}

template <typename T>
int ReleaseResource(const uint64_t handle) noexcept {
  delete reinterpret_cast<T*>(handle);
  return 0;
}

int Mknod(const char* const c_path, const mode_t mode,
          const dev_t dev) noexcept {
  try {
    if (std::strcmp(path, "/") == 0) {
      // They're asking to create the mount point.  Huh?
      return -EEXIST;
    }

    // Trim the leading slash so OpenAt will treat it relative to root_.
    root_->MkNod(path + 1, mode, dev);
    return 0;
  } catch (const std::system_error& e) {
    return -e.code().value();
  } catch (...) {
    LOG(ERROR) << "mknod: caught unexpected value";
    return -ENOTRECOVERABLE;
  }
}

int Open(const char* const path, fuse_file_info* const file_info) noexcept {
  return OpenResource<File>(path, file_info->flags, &file_info->fh);
}

int Read(const char*, char* const buffer, const size_t bytes,
         const off_t offset, fuse_file_info* const file_info) noexcept {
  LOG(INFO) << "read with offset " << offset;
  try {
    auto* const file = reinterpret_cast<File*>(file_info->fh);
    const std::vector<std::uint8_t> read = file->Read(offset, bytes);
    std::memcpy(buffer, read.data(), read.size());
    return static_cast<int>(read.size());
  } catch (const std::system_error& e) {
    return -e.code().value();
  } catch (...) {
    LOG(ERROR) << "read: caught unexpected value";
    return -ENOTRECOVERABLE;
  }
}

int Utimens(const char* const path, const timespec times[2]) noexcept {
  try {
    root_->UTimeNs(
        std::strcmp(path, "/") == 0
            ?
            // Update the times on the mount point.
            "."
            :
            // Trim the leading slash so UTimeNs will treat it relative to
            // root_.
            path + 1,
        times[0], times[1]);
    return 0;
  } catch (const std::system_error& e) {
    return -e.code().value();
  } catch (...) {
    LOG(ERROR) << "utimens: caught unexpected value";
    return -ENOTRECOVERABLE;
  }
}

int Release(const char*, fuse_file_info* const file_info) noexcept {
  return ReleaseResource<File>(file_info->fh);
}

int Unlink(const char* c_path) noexcept {
  try {
    if (std::strcmp(path, "/") == 0) {
      // Removing the root is probably a bad idea.
      return -EPERM;
    }

    // Trim the leading slash so UnlinkAt will treat it relative to root_.
    root_->UnlinkAt(path + 1);
    return 0;
  } catch (const std::system_error& e) {
    return -e.code().value();
  } catch (...) {
    LOG(ERROR) << "unlink: caught unexpected value";
    return -ENOTRECOVERABLE;
  }
}

int Opendir(const char* const path, fuse_file_info* const file_info) noexcept {
  return OpenResource<Directory>(path, O_DIRECTORY, &file_info->fh);
}

int Readdir(const char*, void* const buffer, fuse_fill_dir_t filler,
            const off_t offset, fuse_file_info* const file_info) noexcept {
  try {
    auto* const directory = reinterpret_cast<Directory*>(file_info->fh);

    static_assert(std::is_same<off_t, long>(),
                  "off_t is not convertible with long");
    if (offset != directory->offset()) {
      directory->Seek(offset);
    }

    for (std::experimental::optional<dirent> entry = directory->ReadOne();
         entry; entry = directory->ReadOne()) {
      struct stat stats;
      std::memset(&stats, 0, sizeof(stats));
      stats.st_ino = entry->d_ino;
      stats.st_mode = DirectoryTypeToFileType(entry->d_type);
      const off_t next_offset = directory->offset();
      if (filler(buffer, entry->d_name, &stats, next_offset)) {
        break;
      }
    }
  } catch (const std::system_error& e) {
    return -e.code().value();
  } catch (...) {
    LOG(ERROR) << "readdir: caught unexpected value";
    return -ENOTRECOVERABLE;
  }

  return 0;
}

int Releasedir(const char*, fuse_file_info* const file_info) noexcept {
  return ReleaseResource<Directory>(file_info->fh);
}

}  // namespace

fuse_operations FuseOperations(File* const root) {
  root_ = root;

  fuse_operations result;
  std::memset(&result, 0, sizeof(result));

  result.flag_nullpath_ok = true;
  result.flag_nopath = true;
  result.flag_utime_omit_ok = true;

  result.init = &Initialize;
  result.destroy = &Destroy;

  result.getattr = &Getattr;
  result.fgetattr = &Fgetattr;

  result.mknod = &Mknod;
  result.open = &Open;
  result.read = &Read;
  result.utimens = &Utimens;
  result.release = &Release;
  result.unlink = &Unlink;

  result.opendir = &Opendir;
  result.readdir = &Readdir;
  result.releasedir = &Releasedir;

  return result;
}

}  // namespace scoville
