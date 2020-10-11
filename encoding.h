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

#ifndef ENCODING_H_
#define ENCODING_H_

#include <string>
#include <stdexcept>

namespace scoville {

class DecodingFailure : public std::logic_error {
 public:
  using std::logic_error::logic_error;
};

std::string Encode(const std::string&);

std::string Decode(const std::string&);

}  // scoville

#endif  // ENCODING_H_
