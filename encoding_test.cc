// Copyright 2020 Benjamin Barenblat
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

#include <absl/strings/str_format.h>
#include <gtest/gtest.h>

namespace scoville {
namespace {

const char kAllGoodCharacters[] =
    " !#$&'()+,-.0123456789;=@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqr"
    "stuvwxyz{}~\177";

TEST(ScovilleEncodingTest, EncodesEmptyToEmpty) { EXPECT_EQ(Encode(""), ""); }

TEST(ScovilleEncodingTest, EncodesBadCharacters) {
  for (int i = 1; i < 0x20; ++i) {
    EXPECT_EQ(Encode(absl::StrFormat("foo%cbar", i)),
              absl::StrFormat("foo%%%02xbar", i));
  }
  EXPECT_EQ(Encode("foo*bar"), "foo%2abar");
  EXPECT_EQ(Encode("foo?bar"), "foo%3fbar");
  EXPECT_EQ(Encode("foo<bar"), "foo%3cbar");
  EXPECT_EQ(Encode("foo>bar"), "foo%3ebar");
  EXPECT_EQ(Encode("foo|bar"), "foo%7cbar");
  EXPECT_EQ(Encode("foo\"bar"), "foo%22bar");
  EXPECT_EQ(Encode("foo:bar"), "foo%3abar");
  EXPECT_EQ(Encode("foo\\bar"), "foo%5cbar");
}

TEST(ScovilleEncodingTest, EncodesPercent) {
  EXPECT_EQ(Encode("foo%bar"), "foo%%bar");
}

TEST(ScovilleEncodingTest, EncodesGoodCharacters) {
  EXPECT_EQ(Encode(kAllGoodCharacters), kAllGoodCharacters);
}

TEST(ScovilleEncodingTest, EncodesTrailingBadCharacters) {
  EXPECT_EQ(Encode("foo."), "foo%2e");
  EXPECT_EQ(Encode("foo "), "foo%20");
}

TEST(ScovilleEncodingTest, EncodesDirectoryTrailingBadCharacters) {
  EXPECT_EQ(Encode("foo./bar"), "foo%2e/bar");
  EXPECT_EQ(Encode("foo /bar"), "foo%20/bar");
}

TEST(ScovilleDecodingTest, DecodesEmptyToEmpty) { EXPECT_EQ(Decode(""), ""); }

TEST(ScovilleDecodingTest, DecodesBadCharacters) {
  for (int i = 1; i < 0x20; ++i) {
    EXPECT_EQ(Decode(absl::StrFormat("foo%%%02xbar", i)),
              absl::StrFormat("foo%cbar", i));
  }
  EXPECT_EQ(Decode("foo%2abar"), "foo*bar");
  EXPECT_EQ(Decode("foo%3fbar"), "foo?bar");
  EXPECT_EQ(Decode("foo%3cbar"), "foo<bar");
  EXPECT_EQ(Decode("foo%3ebar"), "foo>bar");
  EXPECT_EQ(Decode("foo%7cbar"), "foo|bar");
  EXPECT_EQ(Decode("foo%22bar"), "foo\"bar");
  EXPECT_EQ(Decode("foo%3abar"), "foo:bar");
  EXPECT_EQ(Decode("foo%5cbar"), "foo\\bar");
}

TEST(ScovilleDecodingTest, DecodesPercent) {
  EXPECT_EQ(Decode("foo%%bar"), "foo%bar");
}

TEST(ScovilleDecodingTest, DecodesGoodCharacters) {
  EXPECT_EQ(Decode(kAllGoodCharacters), kAllGoodCharacters);
}

TEST(ScovilleDecodingTest, DecodesTrailingBadCharacters) {
  EXPECT_EQ(Decode("foo%2e"), "foo.");
  EXPECT_EQ(Decode("foo%20"), "foo ");
}

TEST(ScovilleDecodingTest, DecodesDirectoryTrailingBadCharacters) {
  EXPECT_EQ(Decode("foo%2e/bar"), "foo./bar");
  EXPECT_EQ(Decode("foo%20/bar"), "foo /bar");
}

}  // namespace
}  // namespace scoville
