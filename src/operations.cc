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

#include <dirent.h>
#include <glog/logging.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "fuse.h"
#include "posix_extras.h"

namespace scoville {

namespace {

// Pointer to the directory underlying the mount point.
File* root_;

File* FileInfoFile(fuse_file_info* const file_info) noexcept {
  return reinterpret_cast<File*>(file_info->fh);
}

Directory* FileInfoDirectory(fuse_file_info* const file_info) noexcept {
  return reinterpret_cast<Directory*>(file_info->fh);
}

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

template <typename T>
int OpenResource(const char* const path, const int flags, const mode_t mode,
                 uint64_t* const handle) noexcept {
  try {
    std::unique_ptr<T> t(new T(
        std::strcmp(path, "/") == 0
            ?
            // They're asking to open the mount point.
            *root_
            :
            // Trim the leading slash so OpenAt will treat it relative to root_.
            root_->OpenAt(path + 1, flags, mode)));

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

int Open(const char* const path, fuse_file_info* const file_info) noexcept {
  return OpenResource<File>(path, file_info->flags, 0777, &file_info->fh);
}

int Create(const char* const path, const mode_t mode,
           fuse_file_info* const file_info) noexcept {
  try {
    if (std::strcmp(path, "/") == 0) {
      // They're asking to create the mount point.  Huh?
      return -EEXIST;
    }

    return OpenResource<File>(path,
                              file_info->flags | O_CREAT | O_WRONLY | O_EXCL,
                              mode, &file_info->fh);
  } catch (const std::system_error& e) {
    return -e.code().value();
  } catch (...) {
    LOG(ERROR) << "create: caught unexpected value";
    return -ENOTRECOVERABLE;
  }
}

int Release(const char*, fuse_file_info* const file_info) noexcept {
  File* const file = FileInfoFile(file_info);
  delete file;
  return 0;
}

int Opendir(const char* const path, fuse_file_info* const file_info) noexcept {
  return OpenResource<Directory>(path, O_DIRECTORY, 0777, &file_info->fh);
}

int Readdir(const char*, void* const buffer, fuse_fill_dir_t filler,
            const off_t offset, fuse_file_info* const file_info) noexcept {
  try {
    Directory* const directory = FileInfoDirectory(file_info);

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
  try {
    Directory* const directory = FileInfoDirectory(file_info);
    delete directory;
    return 0;
  } catch (...) {
    LOG(ERROR) << "releasedir: caught unexpected value";
    return -ENOTRECOVERABLE;
  }
}

}  // namespace

fuse_operations FuseOperations(File* const root) {
  root_ = root;

  fuse_operations result;

  result.flag_nullpath_ok = true;
  result.flag_nopath = true;
  result.flag_utime_omit_ok = true;

  result.init = &Initialize;
  result.destroy = &Destroy;

  result.getattr = &Getattr;

  result.open = &Open;
  result.create = &Create;
  result.release = &Release;

  result.opendir = &Opendir;
  result.readdir = &Readdir;
  result.releasedir = &Releasedir;

  return result;
}

}  // namespace scoville
