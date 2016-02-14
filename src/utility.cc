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

#include "utility.h"

#include <string.h>

#include <cerrno>
#include <string>
#include <vector>

#include <glog/logging.h>

namespace scoville {

std::string ErrnoText() {
  std::vector<char> text(64);
  int strerror_result;
  while ((strerror_result = strerror_r(errno, text.data(), text.size())) ==
         ERANGE) {
    VLOG(1) << "ErrnoText doubling message size from " << text.size();
    text.resize(text.size() * 2);
  }
  if (strerror_result != 0) {
    return "(could not generate error message)";
  }
  return std::string(text.begin(), text.end());
}

}  // namespace scoville
