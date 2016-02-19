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

#include <cerrno>
#include <stdexcept>
#include <string>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace scoville {

class IoError : public std::runtime_error {
 public:
  IoError() : IoError(errno) {}

  explicit IoError(const int number)
      : std::runtime_error(Message(number)), number_(number) {}

  int number() const noexcept { return number_; }

  // Converts an errno into a human-readable message, a la strerror(3).
  // Thread-safe.
  static std::string Message(int) noexcept;

 private:
  int number_;
};

// RAII wrapper for Unix file descriptors.
class File {
 public:
  File(const char* path, int flags);
  virtual ~File() noexcept;

  const std::string& path() const noexcept { return path_; }

  // Calls fstat(2) on the file descriptor.
  struct stat Stat() const;

  // Calls lstat(2) on the path relative to the file descriptor.  The path must
  // indeed be relative (i.e., it must not start with '/').
  struct stat LinkStatAt(const char* path) const;

 private:
  File(File&) = delete;

  void operator=(File) = delete;

  std::string path_;
  int fd_;
};

}  // scoville

#endif  // POSIX_EXTRAS_H_
