// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/time/time.h>
#include <gtest/gtest.h>

#include "DisplayFormats/DisplayFormats.h"

namespace orbit_display_formats {

TEST(DisplayFormats, GetDisplaySize) {
  EXPECT_EQ(GetDisplaySize(123), "123 B");
  EXPECT_EQ(GetDisplaySize(123 * 1024ULL + 512 + 256), "123.75 KB");
  EXPECT_EQ(GetDisplaySize(1024 * (123 * 1024ULL + 512 + 256)), "123.75 MB");
  EXPECT_EQ(GetDisplaySize(1024 * 1024 * (123 * 1024ULL + 512 + 256)), "123.75 GB");
  EXPECT_EQ(GetDisplaySize(1024 * 1024 * 1024 * (123 * 1024ULL + 512 + 256)), "123.75 TB");
}

TEST(DisplayFormats, GetDisplayTime) {
  EXPECT_EQ(GetDisplayTime(absl::Nanoseconds(12)), "12.000 ns");
  EXPECT_EQ(GetDisplayTime(absl::Nanoseconds(12'345)), "12.345 us");
  EXPECT_EQ(GetDisplayTime(absl::Nanoseconds(12'345'600)), "12.346 ms");
  EXPECT_EQ(GetDisplayTime(absl::Nanoseconds(12'345'600'000ULL)), "12.346 s");
  EXPECT_EQ(GetDisplayTime(absl::Nanoseconds(60 * 12'345'600'000ULL)), "12.346 min");
  EXPECT_EQ(GetDisplayTime(absl::Nanoseconds(60 * 60 * 12'345'600'000ULL)), "12.346 h");
  EXPECT_EQ(GetDisplayTime(absl::Nanoseconds(24 * 60 * 60 * 12'345'600'000ULL)), "12.346 days");
}

TEST(DisplayFormats, GetDisplayISOTimestamp) {
  // Short Captures
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Nanoseconds(12), 9), "00.000000012");
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Microseconds(304), 6), "00.000304");
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Milliseconds(450), 2), "00.45");
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Milliseconds(4005), 3), "04.005");
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Milliseconds(4500), 1), "04.5");
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Seconds(0), 0), "00s");
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Seconds(0), 1), "00.0");
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Seconds(10), 0), "10s");
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Seconds(13), 0), "13s");
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Minutes(1), 0), "01:00");
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Minutes(1), 1), "01:00.0");
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Nanoseconds(12'345'600), 7), "00.0123456");
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Nanoseconds(12'345'600'000ULL), 4), "12.3456");
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Nanoseconds(60 * 12'345'600'000ULL), 3), "12:20.736");
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Nanoseconds(60 * 60 * 12'345'600'000ULL), 2),
            "12:20:44.16");
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Nanoseconds(10 * 60 * 60 * 12'345'000'000ULL), 0),
            "123:27:00");

  // Long Captures tests
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Milliseconds(450), 2, absl::Seconds(27)), "00.45");
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Milliseconds(450), 9, absl::Minutes(1)),
            "00:00.450000000");
  EXPECT_EQ(GetDisplayISOTimestamp(absl::Milliseconds(450), 9, absl::Hours(1)),
            "00:00:00.450000000");
}

}  // namespace orbit_display_formats