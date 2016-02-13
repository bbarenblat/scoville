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

#ifndef OPERATIONS_H_
#define OPERATIONS_H_

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wpadded"
#define FUSE_USE_VERSION 26
#include <fuse.h>
#pragma clang diagnostic pop

namespace scoville {

fuse_operations FuseOperations();

}  // namespace scoville

#endif  // OPERATIONS_H_
