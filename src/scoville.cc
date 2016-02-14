// Copyright (C) 2007, 2008  Jan Engelhardt <jengelh@medozas.de>
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

#include <iostream>

#include <fcntl.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "operations.h"
#include "utility.h"

constexpr char kUsage[] = R"(allow forbidden characters on VFAT file systems

usage: scoville [flags] target_dir [-- fuse_options])";

int main(int argc, char* argv[]) {
  google::InstallFailureSignalHandler();
  google::SetUsageMessage(kUsage);
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  const char* const root_path = argv[argc - 1];
  int root_fd;
  if ((root_fd = open(root_path, O_DIRECTORY)) == -1) {
    std::cerr << "scoville: bad mount point `" << root_path
              << "': " << scoville::ErrnoText();
    std::exit(EXIT_FAILURE);
  }
  LOG(INFO) << "overlaying " << root_path;

  const fuse_operations operations = scoville::FuseOperations(root_fd);
  return fuse_main(argc, argv, &operations, nullptr);
}
