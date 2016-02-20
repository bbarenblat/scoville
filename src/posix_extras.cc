// Copyright 2016 Benjamin Barenblat
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License.  You may obtain a copy
// of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
// License for the specific language governing permissions and limitations under
// the License.

#include "posix_extras.h"

#include <cerrno>
#include <experimental/optional>
#include <stdexcept>
#include <system_error>

#include <dirent.h>
#include <fcntl.h>
#include <glog/logging.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace scoville {

namespace {

std::system_error SystemError() {
  return std::system_error(errno, std::system_category());
}

}  // namespace

File::File(const char* const path, const int flags, const mode_t mode)
    : path_(path) {
  if ((fd_ = open(path, flags, mode)) == -1) {
    throw SystemError();
  }
  VLOG(1) << "opening file descriptor " << fd_;
}

File::File(const File& other) : path_(other.path_), fd_(other.Duplicate()) {
  VLOG(1) << "opening file descriptor " << fd_;
}

File::~File() noexcept {
  VLOG(1) << "closing file descriptor " << fd_;
  if (close(fd_) == -1) {
    LOG(ERROR) << "failed to close file descriptor " << fd_;
  }
}

struct stat File::Stat() const {
  struct stat result;
  if (fstat(fd_, &result) == -1) {
    throw SystemError();
  }
  return result;
}

struct stat File::LinkStatAt(const char* const path) const {
  if (path[0] == '/') {
    throw std::invalid_argument("absolute path");
  }

  struct stat result;
  if (fstatat(fd_, path, &result, AT_SYMLINK_NOFOLLOW) == -1) {
    throw SystemError();
  }
  return result;
}

File File::OpenAt(const char* const path, const int flags) const {
  if (path[0] == '/') {
    throw std::invalid_argument("absolute path");
  }

  File result;
  if ((result.fd_ = openat(fd_, path, flags)) == -1) {
    throw SystemError();
  }
  result.path_ = path_ + "/" + path;
  return result;
}

File File::OpenAt(const char* const path, const int flags,
                  const mode_t mode) const {
  if (path[0] == '/') {
    throw std::invalid_argument("absolute path");
  }

  File result;
  if ((result.fd_ = openat(fd_, path, flags, mode)) == -1) {
    throw SystemError();
  }
  result.path_ = path_ + "/" + path;
  return result;
}

void File::UnlinkAt(const char* const path) const {
  if (path[0] == '/') {
    throw std::invalid_argument("absolute path");
  }

  if (unlinkat(fd_, path, 0) == -1) {
    throw SystemError();
  }
}

int File::Duplicate() const {
  int result;
  if ((result = dup(fd_)) == -1) {
    throw SystemError();
  }
  return result;
}

Directory::Directory(const File& file) {
  // "After a successful call to fdopendir(), fd is used internally by the
  // implementation, and should not otherwise be used by the application."  We
  // therefore need to grab an unmanaged copy of the file descriptor from file.
  if (!(stream_ = fdopendir(file.Duplicate()))) {
    throw SystemError();
  }
  rewinddir(stream_);
}

Directory::~Directory() noexcept {
  if (closedir(stream_) == -1) {
    LOG(ERROR) << "failed to close directory stream";
  }
}

long Directory::offset() const {
  long result;
  if ((result = telldir(stream_)) == -1) {
    throw SystemError();
  }
  return result;
}

void Directory::Seek(const long offset) noexcept { seekdir(stream_, offset); }

std::experimental::optional<dirent> Directory::ReadOne() {
  dirent* result;
  errno = 0;
  if (!(result = readdir(stream_))) {
    if (errno == 0) {
      return std::experimental::nullopt;
    } else {
      throw SystemError();
    }
  }
  return *result;
}

}  // scoville
