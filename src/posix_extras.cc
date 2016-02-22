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

#include <array>
#include <cerrno>
#include <cstdint>
#include <experimental/optional>
#include <stdexcept>
#include <system_error>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <glog/logging.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace scoville {

namespace {

std::system_error SystemError() {
  return std::system_error(errno, std::system_category());
}

void ValidatePath(const char* const path) {
  if (path[0] == '/') {
    throw std::invalid_argument("absolute path");
  }
}

template <typename T>
T CheckSyscall(const T result) {
  if (result == -1) {
    throw SystemError();
  }
  return result;
}

}  // namespace

File::File(const char* const path, const int flags, const mode_t mode)
    : path_(path) {
  fd_ = CheckSyscall(open(path, flags, mode));
  VLOG(1) << "opening file descriptor " << fd_;
}

File::File(const File& other) : path_(other.path_), fd_(other.Duplicate()) {
  VLOG(1) << "opening file descriptor " << fd_;
}

File::~File() noexcept {
  VLOG(1) << "closing file descriptor " << fd_;
  try {
    CheckSyscall(close(fd_));
  } catch (...) {
    LOG(ERROR) << "failed to close file descriptor " << fd_;
  }
}

struct stat File::Stat() const {
  struct stat result;
  CheckSyscall(fstat(fd_, &result));
  return result;
}

void File::ChModAt(const char* const path, const mode_t mode) const {
  ValidatePath(path);
  CheckSyscall(fchmodat(fd_, path, mode, 0));
}

struct stat File::LinkStatAt(const char* const path) const {
  ValidatePath(path);
  struct stat result;
  CheckSyscall(fstatat(fd_, path, &result, AT_SYMLINK_NOFOLLOW));
  return result;
}

void File::MkDir(const char* const path, const mode_t mode) const {
  ValidatePath(path);
  CheckSyscall(mkdirat(fd_, path, mode | S_IFDIR));
}

void File::MkNod(const char* const path, const mode_t mode,
                 const dev_t dev) const {
  ValidatePath(path);
  CheckSyscall(mknodat(fd_, path, mode, dev));
}

File File::OpenAt(const char* const path, const int flags,
                  const mode_t mode) const {
  ValidatePath(path);
  File result;
  result.fd_ = CheckSyscall(openat(fd_, path, flags, mode));
  result.path_ = path_ + "/" + path;
  return result;
}

std::vector<std::uint8_t> File::Read(off_t offset, size_t bytes) const {
  std::vector<std::uint8_t> result(bytes, 0);
  size_t cursor = 0;
  ssize_t bytes_read;
  while (0 < (bytes_read = CheckSyscall(
                  pread(fd_, result.data() + cursor, bytes, offset)))) {
    cursor += static_cast<size_t>(bytes_read);
    offset += bytes_read;
    bytes -= static_cast<size_t>(bytes_read);
  }
  result.resize(cursor);
  return result;
}

void File::RenameAt(const char* old_path, const char* new_path) const {
  ValidatePath(old_path);
  ValidatePath(new_path);
  CheckSyscall(renameat(fd_, old_path, fd_, new_path));
}

void File::RmDirAt(const char* const path) const {
  ValidatePath(path);
  CheckSyscall(unlinkat(fd_, path, AT_REMOVEDIR));
}

void File::UnlinkAt(const char* const path) const {
  ValidatePath(path);
  CheckSyscall(unlinkat(fd_, path, 0));
}

void File::UTimeNs(const char* const path, const timespec& access,
                   const timespec& modification) const {
  ValidatePath(path);
  std::array<const timespec, 2> times{{access, modification}};
  CheckSyscall(utimensat(fd_, path, times.data(), AT_SYMLINK_NOFOLLOW));
}

size_t File::Write(const off_t offset,
                   const std::vector<std::uint8_t>& to_write) {
  size_t bytes_written = 0;
  while (bytes_written < to_write.size()) {
    bytes_written += static_cast<size_t>(CheckSyscall(pwrite(
        fd_, to_write.data() + bytes_written, to_write.size() - bytes_written,
        offset + static_cast<off_t>(bytes_written))));
  }
  return bytes_written;
}

int File::Duplicate() const { return CheckSyscall(dup(fd_)); }

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
  try {
    CheckSyscall(closedir(stream_));
  } catch (...) {
    LOG(ERROR) << "failed to close directory stream";
  }
}

long Directory::offset() const { return CheckSyscall(telldir(stream_)); }

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
