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

#ifndef POSIX_EXTRAS_H_
#define POSIX_EXTRAS_H_

#include <cstdint>
#include <experimental/optional>
#include <string>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

namespace scoville {

class File;

// RAII wrapper for Unix directory streams.
class Directory {
 public:
  explicit Directory(const File&);
  virtual ~Directory() noexcept;

  long offset() const;

  void Seek(long) noexcept;

  std::experimental::optional<dirent> ReadOne();

 private:
  Directory(const Directory&) = delete;
  Directory(Directory&&) = delete;

  void operator=(const Directory&) = delete;
  void operator=(Directory&&) = delete;

  DIR* stream_;
};

// RAII wrapper for Unix file descriptors.
class File {
 public:
  File(const char* path, int flags) : File(path, flags, 0777) {}
  File(const char* path, int flags, mode_t mode);
  File(const File&);
  File(File&& other) = default;
  virtual ~File() noexcept;

  const std::string& path() const noexcept { return path_; }

  // Calls fstat(2) on the file descriptor.
  struct stat Stat() const;

  // Calls lstat(2) on the path relative to the file descriptor.  The path must
  // indeed be relative (i.e., it must not start with '/').
  struct stat LinkStatAt(const char* path) const;

  // Creates a file or directory at the path relative to the file descriptor.
  // The path must indeed be relative (i.e., it must not start with '/').
  void MkNod(const char* path, mode_t mode, dev_t dev) const;

  // Calls openat(2) on the path relative to the file descriptor.  The path must
  // indeed be relative (i.e., it must not start with '/').
  File OpenAt(const char* const path, const int flags) const {
    return OpenAt(path, flags, 0);
  }
  File OpenAt(const char* path, int flags, mode_t mode) const;

  // Reads exactly the specified number of bytes from the file at the given
  // offset, unless doing so would run past the end of the file, in which case
  // fewer bytes are returned.
  std::vector<std::uint8_t> Read(off_t, size_t) const;

  // Removes the file at the path relative to the file descriptor.  The path
  // must indeed be relative (i.e., it must not start with '/').
  void UnlinkAt(const char* path) const;

  // Sets the access and modification times of the file at the path relative to
  // the file descriptor.  The path must indeed be relative (i.e., it must not
  // start with '/').  Does not follow symbolic links.
  void UTimeNs(const char* path, const timespec& access,
               const timespec& modification) const;

 private:
  File() {}

  void operator=(const File&) = delete;
  void operator=(File&&) = delete;

  // Duplicates fd_ and returns the raw new file descriptor.
  int Duplicate() const;

  std::string path_;
  int fd_;

  friend Directory::Directory(const File&);
};

}  // scoville

#endif  // POSIX_EXTRAS_H_
