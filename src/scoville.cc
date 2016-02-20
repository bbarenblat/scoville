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

#include <memory>

#include <fcntl.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include "fuse.h"
#include "operations.h"
#include "posix_extras.h"

constexpr char kUsage[] = R"(allow forbidden characters on VFAT file systems

usage: scoville [flags] target_dir [-- fuse_options])";

int main(int argc, char* argv[]) {
  google::InstallFailureSignalHandler();
  google::SetUsageMessage(kUsage);
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  // This is an overlay file system, which means once we start FUSE, the
  // underlying file system will be inaccessible through normal means.  Open a
  // file descriptor to the underlying root now so we can still do operations on
  // it while it's overlayed.
  std::unique_ptr<scoville::File> root;
  const char* const root_path = argv[argc - 1];
  try {
    root.reset(new scoville::File(root_path, O_DIRECTORY));
  } catch (const scoville::IoError& e) {
    LOG(FATAL) << "scoville: bad mount point `" << root_path
               << "': " << e.what();
  }
  LOG(INFO) << "overlaying " << root->path();
  const fuse_operations operations = scoville::FuseOperations(root.get());
  return fuse_main(argc, argv, &operations, nullptr);
}
