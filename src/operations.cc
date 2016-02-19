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
#include <experimental/optional>
#include <memory>
#include <new>
#include <type_traits>

#define FUSE_USE_VERSION 26
#include <dirent.h>
#include <fuse.h>
#include <glog/logging.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "posix_extras.h"

namespace scoville {

namespace {

// Pointer to the directory underlying the mount point.
File* root_;

File* FileInfoFile(fuse_file_info* const file_info) {
  return reinterpret_cast<File*>(file_info->fh);
}

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

void Destroy(void*) { LOG(INFO) << "destroy"; }

int Getattr(const char* const path, struct stat* output) noexcept {
  LOG(INFO) << "getattr(" << path << ")";

  if (path[0] == '\0') {
    LOG(ERROR) << "getattr called on path not starting with /";
    return -ENOENT;
  }

  try {
    if (strcmp(path, "/") == 0) {
      // They're asking for information about the mount point.
      *output = root_->Stat();
      return 0;
    }

    // Trim the leading slash so LinkStatAt will treat it relative to root_.
    LOG(INFO) << "getattr: trimming leading slash";
    *output = root_->LinkStatAt(path + 1);
    return 0;
  } catch (const IoError& e) {
    return -e.number();
  }
}

int Open(const char* const path, fuse_file_info* const file_info) {
  LOG(INFO) << "open(" << path << ")";

  std::unique_ptr<File> file;

  try {
    file.reset(new File(
        strcmp(path, "/") == 0
            ?
            // They're asking to open the mount point.
            *root_
            :
            // Trim the leading slash so OpenAt will treat it relative to root_.
            root_->OpenAt(path + 1, file_info->flags)));
  } catch (const std::bad_alloc&) {
    return -ENOMEM;
  } catch (const IoError& e) {
    return -e.number();
  }

  static_assert(sizeof(file_info->fh) == sizeof(uintptr_t),
                "FUSE file handles are a different size than pointers");
  file_info->fh = reinterpret_cast<uintptr_t>(file.release());
  return 0;
}

int Create(const char* const path, const mode_t mode,
           fuse_file_info* const file_info) {
  LOG(INFO) << "create(" << path << ")";

  if (strcmp(path, "/") == 0) {
    // They're asking to create the mount point.  Huh?
    return -EEXIST;
  }

  std::unique_ptr<File> file;

  try {
    // Trim the leading slash so OpenAt will treat it relative to root_.
    file.reset(new File(root_->OpenAt(path + 1, file_info->flags, mode)));
  } catch (const std::bad_alloc&) {
    return -ENOMEM;
  } catch (const IoError& e) {
    return -e.number();
  }

  static_assert(sizeof(file_info->fh) == sizeof(uintptr_t),
                "FUSE file handles are a different size than pointers");
  file_info->fh = reinterpret_cast<uintptr_t>(file.release());
  return 0;
}

int Release(const char*, fuse_file_info* const file_info) {
  LOG(INFO) << "release";
  File* const file = FileInfoFile(file_info);
  delete file;
  return 0;
}

int Opendir(const char* const path, fuse_file_info* const file_info) {
  LOG(INFO) << "opendir(" << path << ")";

  std::unique_ptr<Directory> directory;

  try {
    directory.reset(new Directory(
        strcmp(path, "/") == 0
            ?
            // They're asking to open the mount point.
            *root_
            :
            // Trim the leading slash so OpenAt will treat it relative to root_.
            root_->OpenAt(path + 1, O_DIRECTORY)));
  } catch (const std::bad_alloc&) {
    return -ENOMEM;
  } catch (const IoError& e) {
    return -e.number();
  }

  static_assert(sizeof(file_info->fh) == sizeof(uintptr_t),
                "FUSE file handles are a different size than pointers");
  file_info->fh = reinterpret_cast<uintptr_t>(directory.release());
  return 0;
}

int Readdir(const char*, void* const buffer, fuse_fill_dir_t filler,
            const off_t offset, fuse_file_info* const file_info) {
  LOG(INFO) << "readdir";

  Directory* const directory = FileInfoDirectory(file_info);

  try {
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
  } catch (const IoError& e) {
    return -e.number();
  }

  return 0;
}

int Releasedir(const char*, fuse_file_info* const file_info) {
  LOG(INFO) << "releasedir";
  Directory* const directory = FileInfoDirectory(file_info);
  delete directory;
  return 0;
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
