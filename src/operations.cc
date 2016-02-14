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

#define BSD_SOURCE
#define _POSIX_C_SOURCE 201602L

#include "operations.h"

#include <cerrno>
#include <cstring>
#include <memory>
#include <new>

#define FUSE_USE_VERSION 26
#include <dirent.h>
#include <fuse.h>
#include <glog/logging.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "utility.h"

namespace scoville {

namespace {

int root_fd_;

struct Directory {
  DIR* fd;
  dirent* entry;
  off_t offset;
};

Directory* FileInfoDirectory(fuse_file_info* const file_info) {
  return reinterpret_cast<Directory*>(file_info->fh);
}

mode_t DirectoryTypeToFileType(const unsigned char type) {
  switch (type) {
    case DT_BLK:
      return S_IFBLK;
    case DT_CHR:
      return S_IFCHR;
    case DT_DIR:
      return S_IFDIR;
    case DT_FIFO:
      return S_IFIFO;
    case DT_LNK:
      return S_IFLNK;
    case DT_REG:
      return S_IFREG;
    case DT_SOCK:
      return S_IFSOCK;
    default:
      return 0;
  }
}

void* Initialize(fuse_conn_info*) {
  LOG(INFO) << "initialize";
  return nullptr;
}

void Destroy(void*) {
  LOG(INFO) << "destroy";
  LOG_IF(ERROR, close(root_fd_) == -1) << "could not close root FD: "
                                       << ErrnoText();
}

int Getattr(const char* const path, struct stat* output) {
  LOG(INFO) << "getattr(" << path << ")";
  if (lstat(path, output) == -1) {
    return -errno;
  }
  return 0;
}

int Opendir(const char* const path, fuse_file_info* const file_info) {
  LOG(INFO) << "opendir(" << path << ")";

  std::unique_ptr<Directory> directory;
  try {
    directory.reset(new Directory);
  } catch (std::bad_alloc) {
    return -ENOMEM;
  }

  if ((directory->fd = opendir(path)) == nullptr) {
    return -errno;
  }
  directory->entry = nullptr;
  directory->offset = 0;

  static_assert(sizeof(file_info->fh) == sizeof(uintptr_t),
                "FUSE file handles are a different size than pointers");
  file_info->fh = reinterpret_cast<uintptr_t>(directory.release());
  return 0;
}

int Readdir(const char*, void* const buffer, fuse_fill_dir_t filler,
            const off_t offset, fuse_file_info* const file_info) {
  LOG(INFO) << "readdir";

  Directory* const directory = FileInfoDirectory(file_info);

  if (offset != directory->offset) {
    seekdir(directory->fd, offset);
    directory->entry = nullptr;
    directory->offset = offset;
  }

  while (true) {
    if (!directory->entry) {
      directory->entry = readdir(directory->fd);
      if (!directory->entry) {
        break;
      }
    }

    struct stat stats;
    std::memset(&stats, 0, sizeof(stats));
    stats.st_ino = directory->entry->d_ino;
    stats.st_mode = DirectoryTypeToFileType(directory->entry->d_type);
    const off_t next_offset = telldir(directory->fd);
    if (filler(buffer, directory->entry->d_name, &stats, next_offset)) {
      break;
    }

    directory->entry = nullptr;
    directory->offset = next_offset;
  }

  return 0;
}

int Releasedir(const char*, fuse_file_info* const file_info) {
  LOG(INFO) << "releasedir";
  Directory* const directory = FileInfoDirectory(file_info);
  closedir(directory->fd);
  delete directory;
  return 0;
}

}  // namespace

fuse_operations FuseOperations(const int root_fd) {
  root_fd_ = root_fd;

  fuse_operations result;

  result.flag_nullpath_ok = true;
  result.flag_nopath = true;
  result.flag_utime_omit_ok = true;

  result.init = &Initialize;
  result.destroy = &Destroy;

  result.getattr = &Getattr;
  result.opendir = &Opendir;
  result.readdir = &Readdir;
  result.releasedir = &Releasedir;

  return result;
}

}  // namespace scoville
