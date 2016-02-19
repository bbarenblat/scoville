# Copyright 2016 Benjamin Barenblat
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License.  You may obtain a copy of
# the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations under
# the License.

include(FindPkgConfig)

pkg_check_modules(FUSE REQUIRED fuse)
pkg_check_modules(GFLAGS REQUIRED libgflags)
pkg_check_modules(GLOG REQUIRED libglog)

link_directories(
  ${FUSE_LIBRARY_DIRS}
  ${GFLAGS_LIBRARY_DIRS}
  ${GLOG_LIBRARY_DIRS}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FUSE_CFLAGS_OTHER}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GFLAGS_CFLAGS_OTHER}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GLOG_CFLAGS_OTHER}")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${FUSE_LDFLAGS_OTHER}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${GFLAGS_LDFLAGS_OTHER}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${GLOG_LDFLAGS_OTHER}")

add_executable(
  scoville
  encoding.cc
  operations.cc
  posix_extras.cc
  scoville.cc
)

target_include_directories(
  scoville
  SYSTEM PRIVATE ${FUSE_INCLUDE_DIRS}
  SYSTEM PRIVATE ${GFLAGS_INCLUDE_DIRS}
  SYSTEM PRIVATE ${GLOG_INCLUDE_DIRS}
)

target_link_libraries(
  scoville
  ${FUSE_LIBRARIES}
  ${GFLAGS_LIBRARIES}
  ${GLOG_LIBRARIES}
)
