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

#define _POSIX_C_SOURCE 201502L
#undef _GNU_SOURCE

#include "posix_extras.h"

#include <cerrno>
#include <stdexcept>
#include <vector>

#include <fcntl.h>
#include <glog/logging.h>
#include <string.h>  // a POSIX header, not a libc one
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace scoville {

std::string IoError::Message(const int number) noexcept {
  try {
    std::vector<char> text(64);
    int strerror_result;
    while ((strerror_result = strerror_r(number, text.data(), text.size())) ==
           ERANGE) {
      VLOG(1) << "IoError::Message: doubling message size from " << text.size();
      text.resize(text.size() * 2);
    }

    if (strerror_result == 0) {
      return std::string(text.begin(), text.end());
    }
  } catch (...) {
  }
  return "(could not generate error message)";
}

File::File(const char* const path, const int flags) : path_(path) {
  if ((fd_ = open(path, flags)) == -1) {
    throw IoError();
  }
}

File::~File() noexcept {
  VLOG(1) << "closing file descriptor " << fd_;
  if (close(fd_) == -1) {
    LOG(ERROR) << "failed to close file descriptor " << fd_ << ": "
               << IoError::Message(errno);
  }
}

struct stat File::Stat() const {
  struct stat result;
  if (fstat(fd_, &result) == -1) {
    throw IoError();
  }
  return result;
}

struct stat File::LinkStatAt(const char* const path) const {
  if (path[0] == '/') {
    throw std::invalid_argument("absolute path");
  }

  struct stat result;
  if (fstatat(fd_, path, &result, AT_SYMLINK_NOFOLLOW) == -1) {
    throw IoError();
  }
  return result;
}

}  // scoville
