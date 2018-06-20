// Copyright 2016, 2018 Benjamin Barenblat
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
#include <functional>
#include <ios>
#include <sstream>
#include <string>

#include <glog/logging.h>

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
         c == '|' || c == '"' || c == ':' || c == '\\';
}

bool IsVfatBadLastCharacter(const char c) noexcept {
  return IsVfatBadCharacter(c) || c == '.' || c == ' ';
}

void EncodeStream(std::istringstream* const in, std::ostringstream* const out) {
  char c;
  while (!in->get(c).eof()) {
    in->peek();
    const bool processing_last_character = in->eof();

    if (IsVfatBadCharacter(c) ||
        (processing_last_character && IsVfatBadLastCharacter(c))) {
      *out << '%';
      WriteAsciiAsHex(c, out);
    } else if (c == '%') {
      *out << "%%";
    } else {
      *out << c;
    }
  }
}

void DecodeStream(std::istringstream* const in, std::ostringstream* const out) {
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

std::string TransformString(
    std::function<void(std::istringstream*, std::ostringstream*)> f,
    const std::string& in) {
  std::istringstream in_stream(in);
  std::ostringstream out_stream;
  f(&in_stream, &out_stream);
  return out_stream.str();
}

}  // namespace

std::string Encode(const std::string& in) {
  const std::string result = TransformString(EncodeStream, in);
  VLOG(1) << "Encode: \"" << in << "\" -> \"" << result << "\"";
  return result;
}

std::string Decode(const std::string& in) {
  const std::string result = TransformString(DecodeStream, in);
  VLOG(1) << "Decode: \"" << in << "\" -> \"" << result << "\"";
  return result;
}

}  // scoville
