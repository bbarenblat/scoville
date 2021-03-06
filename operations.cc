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
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <glog/logging.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <time.h>

#include "encoding.h"
#include "fuse.h"
#include "posix_extras.h"

namespace scoville {

namespace {

// Pointer to the directory underlying the mount point.
File* root_;

mode_t DirectoryTypeToFileType(const unsigned char type) {
  return static_cast<mode_t>(DTTOIF(type));
}

std::string MakeRelative(const std::string& path) {
  if (path.at(0) != '/') {
    throw std::system_error(ENOENT, std::system_category());
  }
  return path.substr(1);
}

void* Initialize(fuse_conn_info*) noexcept { return nullptr; }

void Destroy(void*) noexcept {}

int Statfs(const char* const c_path, struct statvfs* const output) {
  const std::string path(Encode(c_path));
  if (path == "/") {
    *output = root_->StatVFs();
  } else {
    *output =
        root_->OpenAt(MakeRelative(path).c_str(), O_RDONLY | O_PATH).StatVFs();
  }
  return 0;
}

int Getattr(const char* const c_path, struct stat* output) {
  const std::string path(Encode(c_path));
  if (path == "/") {
    *output = root_->Stat();
  } else {
    *output = root_->LinkStatAt(MakeRelative(path).c_str());
  }
  return 0;
}

int Fgetattr(const char*, struct stat* const output,
             struct fuse_file_info* const file_info) {
  // This reinterpret_cast violates type aliasing rules, so a compiler may
  // invoke undefined behavior if *output is ever dereferenced.  However, we
  // compile with -fno-strict-aliasing, so this should be safe.
  *output = reinterpret_cast<File*>(file_info->fh)->Stat();
  return 0;
}

template <typename T>
int OpenResource(const std::string& path, const int flags,
                 uint64_t* const handle, const mode_t mode = 0) {
  try {
    std::unique_ptr<T> t(
        new T(path == "/" ? *root_ : root_->OpenAt(MakeRelative(path).c_str(),
                                                   flags, mode)));
    static_assert(sizeof(*handle) == sizeof(std::uintptr_t),
                  "FUSE file handles are a different size than pointers");
    *handle = reinterpret_cast<std::uintptr_t>(t.release());
    return 0;
  } catch (const std::bad_alloc&) {
    return -ENOMEM;
  }
}

template <typename T>
int ReleaseResource(const uint64_t handle) noexcept {
  delete reinterpret_cast<T*>(handle);
  return 0;
}

int Mknod(const char* const c_path, const mode_t mode, const dev_t dev) {
  const std::string path(Encode(c_path));
  if (path == "/") {
    return -EISDIR;
  } else {
    root_->MkNod(MakeRelative(path).c_str(), mode, dev);
    return 0;
  }
}

int Chmod(const char* const c_path, const mode_t mode) {
  const std::string path(Encode(c_path));
  root_->ChModAt(path == "/" ? "." : MakeRelative(path).c_str(), mode);
  return 0;
}

int Rename(const char* const c_old_path, const char* const c_new_path) {
  const std::string old_path(Encode(c_old_path));
  const std::string new_path(Encode(c_new_path));
  if (old_path == "/" || new_path == "/") {
    return -EINVAL;
  } else {
    root_->RenameAt(MakeRelative(old_path).c_str(),
                    MakeRelative(new_path).c_str());
    return 0;
  }
}

int Create(const char* const path, const mode_t mode,
           fuse_file_info* const file_info) {
  return OpenResource<File>(Encode(path), file_info->flags | O_CREAT,
                            &file_info->fh, mode);
}

int Open(const char* const path, fuse_file_info* const file_info) {
  return OpenResource<File>(Encode(path), file_info->flags, &file_info->fh);
}

int Read(const char*, char* const buffer, const size_t bytes,
         const off_t offset, fuse_file_info* const file_info) {
  // This reinterpret_cast violates type aliasing rules, so a compiler may
  // invoke undefined behavior when file is dereferenced on the next line.
  // However, we compile with -fno-strict-aliasing, so it should be safe.
  auto* const file = reinterpret_cast<File*>(file_info->fh);
  const std::vector<std::uint8_t> read = file->Read(offset, bytes);
  std::memcpy(buffer, read.data(), read.size());
  return static_cast<int>(read.size());
}

int Write(const char*, const char* const buffer, const size_t bytes,
          const off_t offset, fuse_file_info* const file_info) {
  // See notes in Read about undefined behavior.
  auto* const file = reinterpret_cast<File*>(file_info->fh);
  const std::vector<std::uint8_t> to_write(buffer, buffer + bytes);
  file->Write(offset, to_write);
  return static_cast<int>(bytes);
}

int Utimens(const char* const c_path, const timespec times[2]) {
  const std::string path(Encode(c_path));
  root_->UTimeNs(path == "/" ? "." : MakeRelative(path).c_str(), times[0],
                 times[1]);
  return 0;
}

int Release(const char*, fuse_file_info* const file_info) {
  return ReleaseResource<File>(file_info->fh);
}

int Unlink(const char* c_path) {
  const std::string path(Encode(c_path));
  if (path == "/") {
    // Removing the root is probably a bad idea.
    return -EPERM;
  } else {
    root_->UnlinkAt(MakeRelative(path).c_str());
    return 0;
  }
}

int Symlink(const char*, const char*) { return -EPERM; }

int Readlink(const char*, char*, size_t) { return -EINVAL; }

int Mkdir(const char* const c_path, const mode_t mode) {
  const std::string path(Encode(c_path));
  if (path == "/") {
    // They're asking to create the mount point.  Huh?
    return -EEXIST;
  } else {
    root_->MkDir(MakeRelative(path).c_str(), mode);
    return 0;
  }
}

int Opendir(const char* const path, fuse_file_info* const file_info) {
  return OpenResource<Directory>(Encode(path), O_DIRECTORY, &file_info->fh);
}

int Readdir(const char*, void* const buffer, fuse_fill_dir_t filler,
            const off_t offset, fuse_file_info* const file_info) {
  // See notes in Read about undefined behavior.
  auto* const directory = reinterpret_cast<Directory*>(file_info->fh);

  static_assert(std::is_same<off_t, long>(),
                "off_t is not convertible with long");
  if (offset != directory->offset()) {
    directory->Seek(offset);
  }

  for (std::experimental::optional<dirent> entry = directory->ReadOne(); entry;
       entry = directory->ReadOne()) {
    struct stat stats;
    std::memset(&stats, 0, sizeof(stats));
    stats.st_ino = entry->d_ino;
    stats.st_mode = DirectoryTypeToFileType(entry->d_type);
    const off_t next_offset = directory->offset();
    if (filler(buffer, Decode(entry->d_name).c_str(), &stats, next_offset)) {
      break;
    }
  }
  return 0;
}

int Releasedir(const char*, fuse_file_info* const file_info) {
  return ReleaseResource<Directory>(file_info->fh);
}

int Truncate(const char* const c_path, const off_t size) {
  const std::string path(Encode(c_path));
  if (path == "/") {
    return -EISDIR;
  } else {
    root_->OpenAt(MakeRelative(path).c_str(), O_WRONLY).Truncate(size);
    return 0;
  }
}

int Ftruncate(const char*, const off_t size, fuse_file_info* const file_info) {
  // See notes in Read about undefined behavior.
  reinterpret_cast<File*>(file_info->fh)->Truncate(size);
  return 0;
}

int Rmdir(const char* c_path) {
  const std::string path(Encode(c_path));
  if (path == "/") {
    // Removing the root is probably a bad idea.
    return -EPERM;
  } else {
    root_->RmDirAt(MakeRelative(path).c_str());
    return 0;
  }
}

template <typename Function, Function f, typename... Args>
int CatchAndReturnExceptions(Args... args) noexcept {
  try {
    return f(args...);
  } catch (const std::system_error& e) {
    return -e.code().value();
  } catch (...) {
    LOG(ERROR) << "caught unexpected value";
    return -ENOTRECOVERABLE;
  }
}

}  // namespace

#define CATCH_AND_RETURN_EXCEPTIONS(f) CatchAndReturnExceptions<decltype(f), f>

fuse_operations FuseOperations(File* const root) {
  root_ = root;

  fuse_operations result;
  std::memset(&result, 0, sizeof(result));

  result.flag_nullpath_ok = true;
  result.flag_nopath = true;
  result.flag_utime_omit_ok = true;

  result.init = Initialize;
  result.destroy = Destroy;

  result.statfs = CATCH_AND_RETURN_EXCEPTIONS(Statfs);

  result.getattr = CATCH_AND_RETURN_EXCEPTIONS(Getattr);
  result.fgetattr = CATCH_AND_RETURN_EXCEPTIONS(Fgetattr);

  result.mknod = CATCH_AND_RETURN_EXCEPTIONS(Mknod);
  result.chmod = CATCH_AND_RETURN_EXCEPTIONS(Chmod);
  result.rename = CATCH_AND_RETURN_EXCEPTIONS(Rename);
  result.create = CATCH_AND_RETURN_EXCEPTIONS(Create);
  result.open = CATCH_AND_RETURN_EXCEPTIONS(Open);
  result.read = CATCH_AND_RETURN_EXCEPTIONS(Read);
  result.write = CATCH_AND_RETURN_EXCEPTIONS(Write);
  result.utimens = CATCH_AND_RETURN_EXCEPTIONS(Utimens);
  result.release = CATCH_AND_RETURN_EXCEPTIONS(Release);
  result.truncate = CATCH_AND_RETURN_EXCEPTIONS(Truncate);
  result.ftruncate = CATCH_AND_RETURN_EXCEPTIONS(Ftruncate);
  result.unlink = CATCH_AND_RETURN_EXCEPTIONS(Unlink);

  result.symlink = CATCH_AND_RETURN_EXCEPTIONS(Symlink);
  result.readlink = CATCH_AND_RETURN_EXCEPTIONS(Readlink);

  result.mkdir = CATCH_AND_RETURN_EXCEPTIONS(Mkdir);
  result.opendir = CATCH_AND_RETURN_EXCEPTIONS(Opendir);
  result.readdir = CATCH_AND_RETURN_EXCEPTIONS(Readdir);
  result.releasedir = CATCH_AND_RETURN_EXCEPTIONS(Releasedir);
  result.rmdir = CATCH_AND_RETURN_EXCEPTIONS(Rmdir);

  return result;
}

#undef CATCH_AND_RETURN_EXCEPTIONS

}  // namespace scoville
