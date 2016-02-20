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

#include "encoding.h"

#include <array>
#include <cstdlib>
#include <ios>
#include <sstream>
#include <string>

namespace scoville {

namespace {

void WriteAsciiAsHex(const char c, std::ostringstream* const out) {
  if (1 < sizeof(c) && 0x100 <= c) {
    // Not ASCII!
    throw EncodingFailure("could not encode non-ASCII character '" +
                          std::string(1, c) + "'");
  }
  *out << std::hex << static_cast<int>(c);
}

char ReadHexAsAscii(std::istringstream* const in) {
  std::array<char, 3> hex_str;
  in->get(hex_str.data(), hex_str.size());
  char* decoded_end;
  const char result =
      static_cast<char>(std::strtol(hex_str.data(), &decoded_end, 16));
  if (decoded_end == hex_str.data()) {
    throw DecodingFailure("could not decode invalid hex");
  }
  return result;
}

bool IsVfatBadCharacter(const char c) noexcept {
  return (0 <= c && c < 0x20) || c == '*' || c == '?' || c == '<' || c == '>' ||
         c == '|' || c == '"' || c == ':' || c == '/' || c == '\\';
}

}  // namespace

void Encode(std::istringstream* const in, std::ostringstream* const out) {
  char c;
  while (!in->get(c).eof()) {
    if (IsVfatBadCharacter(c)) {
      *out << '%';
      WriteAsciiAsHex(c, out);
    } else if (c == '%') {
      *out << "%%";
    } else {
      *out << c;
    }
  }
}

void Decode(std::istringstream* const in, std::ostringstream* const out) {
  char c;
  while (!in->get(c).eof()) {
    if (c == '%') {
      if (in->peek() == '%') {
        in->ignore();
        *out << "%";
      } else {
        *out << ReadHexAsAscii(in);
      }
    } else {
      *out << c;
    }
  }
}

}  // scoville
