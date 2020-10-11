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

#include <absl/strings/escaping.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>
#include <glog/logging.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace scoville {

namespace {

bool IsVfatBadCharacter(const char c) noexcept {
  return (0 <= c && c < 0x20) || c == '*' || c == '?' || c == '<' || c == '>' ||
         c == '|' || c == '"' || c == ':' || c == '\\';
}

bool IsVfatBadLastCharacter(const char c) noexcept {
  return IsVfatBadCharacter(c) || c == '.' || c == ' ';
}

bool IsValidHex(const char c) noexcept {
  return ('0' <= c && c <= '9') || ('A' <= c && c <= 'F') ||
         ('a' <= c && c <= 'f');
}

std::string EncodeComponent(absl::string_view in) {
  std::string out;
  for (size_t i = 0; i < in.size(); ++i) {
    absl::string_view c(in.data() + i, 1);
    if (in[i] == '%') {
      absl::StrAppend(&out, "%%");
    } else if (IsVfatBadCharacter(in[i]) ||
               (i == in.size() - 1 && IsVfatBadLastCharacter(in[i]))) {
      absl::StrAppend(&out, "%", absl::BytesToHexString(c));
    } else {
      absl::StrAppend(&out, c);
    }
  }
  return out;
}

}  // namespace

std::string Encode(const std::string& in) {
  std::vector<std::string> out;
  for (auto component : absl::StrSplit(in, '/')) {
    out.push_back(EncodeComponent(component));
  }
  std::string result = absl::StrJoin(out, "/");
  VLOG(1) << "Encode: \"" << in << "\" -> \"" << result << "\"";
  return result;
}

std::string Decode(const std::string& in) {
  std::string result;
  for (size_t i = 0; i < in.size(); ++i) {
    if (in[i] != '%') {
      // This isn't an escaped byte.
      absl::StrAppend(&result, absl::string_view(in.data() + i, 1));
      continue;
    }

    char x, y;

    // Decode single-byte escapes. There's only one of these ("%%" -> "%").
    try {
      x = in.at(i + 1);
    } catch (std::out_of_range&) {
      throw DecodingFailure("clipped escape at end of string");
    }
    if (x == '%') {
      absl::StrAppend(&result, "%");
      ++i;
      continue;
    }

    // Decode double-byte escapes.
    try {
      y = in.at(i + 2);
    } catch (std::out_of_range&) {
      throw DecodingFailure("clipped escape at end of string");
    }
    if (!(IsValidHex(x) && IsValidHex(y))) {
      throw DecodingFailure("clipped escape at end of string");
    }
    absl::StrAppend(&result, absl::HexStringToBytes(
                                 absl::string_view(in.c_str() + i + 1, 2)));
    i += 2;
  }
  return result;
  VLOG(1) << "Decode: \"" << in << "\" -> \"" << result << "\"";
}

}  // namespace scoville
