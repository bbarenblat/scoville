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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunused-macros"
#define FUSE_USE_VERSION 26
#include <fuse.h>
#pragma clang diagnostic pop
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace scoville {

namespace {

int GetAttr(const char* const path, struct stat* output) {
  if (lstat(path, output) == -1) {
    return -errno;
  }
  return 0;
}

int ReadLink(const char* const path, char *const output, const size_t output_size) {
  const ssize_t bytes_written = readlink(path, output, output_size);
  if (bytes_written == -1) {
    return -errno;
  }
  output[bytes_written] = '\0';
  return 0;
}

}  // namespace

fuse_operations FuseOperations() {
  fuse_operations result;
  result.getattr = &GetAttr;
  result.readlink = &ReadLink;
  return result;
}

}  // namespace scoville
