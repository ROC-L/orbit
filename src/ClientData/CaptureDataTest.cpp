// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/strings/numbers.h>
#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <tuple>
#include <vector>

#include "ClientData/CaptureData.h"
#include "ClientData/ScopeIdConstants.h"
#include "ClientData/ScopeIdProvider.h"
#include "ClientData/ScopeStats.h"
#include "ClientProtos/capture_data.pb.h"
#include "GrpcProtos/capture.pb.h"
#include "OrbitBase/Logging.h"
#include "OrbitBase/ReadFileToString.h"
#include "Test/Path.h"

namespace orbit_client_data {

namespace {

constexpr size_t kTimersForFirstId = 3;
constexpr size_t kTimersForSecondId = 2;
constexpr size_t kTimerCount = kTimersForFirstId + kTimersForSecondId;
constexpr uint64_t kFirstId = 1;
constexpr uint64_t kSecondId = 2;
const std::string kFirstName = "foo()";
const std::string kSecondName = "bar()";
constexpr std::array<uint64_t, kTimerCount> kTimerIds = {kFirstId, kFirstId, kFirstId, kSecondId,
                                                         kSecondId};
constexpr std::array<uint64_t, kTimerCount> kStarts = {10, 20, 30, 40, 50};
constexpr std::array<uint64_t, kTimersForFirstId> kDurationsForFirstId = {300, 100, 200};
constexpr std::array<uint64_t, kTimersForSecondId> kDurationsForSecondId = {500, 400};
constexpr std::array<uint64_t, kTimersForFirstId> kSortedDurationsForFirstId = {100, 200, 300};
constexpr std::array<uint64_t, kTimersForSecondId> kSortedDurationsForSecondId = {400, 500};

constexpr uint64_t kLargeInteger = 10'000'000'000'000'000;

static const std::array<uint64_t, kTimerCount> kDurations = [] {
  std::array<uint64_t, kTimerCount> result;
  std::copy(std::begin(kDurationsForFirstId), std::end(kDurationsForFirstId), std::begin(result));
  std::copy(std::begin(kDurationsForSecondId), std::end(kDurationsForSecondId),
            std::begin(result) + kTimersForFirstId);
  return result;
}();
static const std::array<TimerInfo, kTimerCount> kTimerInfos = [] {
  std::array<TimerInfo, kTimerCount> result;
  for (size_t i = 0; i < kTimerCount; ++i) {
    result[i].set_function_id(kTimerIds[i]);
    result[i].set_start(kStarts[i]);
    result[i].set_end(kStarts[i] + kDurations[i]);
  }
  return result;
}();

constexpr double kFirstVariance = 6666.66666;
constexpr double kSecondVariance = 2500.0;

template <std::size_t Size>
[[nodiscard]] static ScopeStats GetStats(const std::array<uint64_t, Size>& durations,
                                         double variance) {
  ScopeStats stats{};
  stats.set_count(Size);
  const auto begin = std::begin(durations);
  const auto end = std::end(durations);

  stats.set_total_time_ns(std::reduce(begin, end));
  stats.set_min_ns(*std::min_element(begin, end));
  stats.set_max_ns(*std::max_element(begin, end));
  stats.set_variance_ns(variance);

  return stats;
}

const TimerInfo kTimerInfoWithInvalidScopeId = []() {
  TimerInfo timer;
  timer.set_start(0);
  timer.set_end(std::numeric_limits<uint64_t>::max());
  timer.set_function_id(0);
  return timer;
}();

static void ExpectStatsEqual(const ScopeStats& actual, const ScopeStats& other) {
  EXPECT_EQ(actual.total_time_ns(), other.total_time_ns());
  EXPECT_EQ(actual.min_ns(), other.min_ns());
  EXPECT_EQ(actual.max_ns(), other.max_ns());

  EXPECT_NEAR(actual.variance_ns(), other.variance_ns(), 1.0);
  EXPECT_NEAR(actual.ComputeStdDevNs(), other.ComputeStdDevNs(), 1.0);
}

void AddInstrumentedFunction(orbit_grpc_protos::CaptureOptions& capture_options,
                             uint64_t function_id, const std::string& name) {
  orbit_grpc_protos::InstrumentedFunction* function = capture_options.add_instrumented_functions();
  function->set_function_id(function_id);
  function->set_function_name(name);
}

[[nodiscard]] orbit_grpc_protos::CaptureStarted CreateCaptureStarted() {
  orbit_grpc_protos::CaptureStarted capture_started;
  AddInstrumentedFunction(*capture_started.mutable_capture_options(), kFirstId, kFirstName);
  AddInstrumentedFunction(*capture_started.mutable_capture_options(), kSecondId, kSecondName);
  return capture_started;
}

class CaptureDataTest : public testing::Test {
 public:
  explicit CaptureDataTest()
      : capture_data_{
            CreateCaptureStarted(), std::nullopt, {}, CaptureData::DataSource::kLiveCapture} {}

 protected:
  CaptureData capture_data_;
};

}  // namespace

TEST_F(CaptureDataTest, UpdateScopeStatsIsCorrect) {
  for (const TimerInfo& timer : kTimerInfos) {
    capture_data_.UpdateScopeStats(timer);
  }
  capture_data_.UpdateScopeStats(kTimerInfoWithInvalidScopeId);

  ExpectStatsEqual(capture_data_.GetScopeStatsOrDefault(kFirstId),
                   GetStats(kDurationsForFirstId, kFirstVariance));
  ExpectStatsEqual(capture_data_.GetScopeStatsOrDefault(kSecondId),
                   GetStats(kDurationsForSecondId, kSecondVariance));
}

TEST_F(CaptureDataTest, VarianceIsCorrectForLongDurations) {
  for (TimerInfo timer : kTimerInfos) {
    timer.set_end(timer.end() + kLargeInteger);
    capture_data_.UpdateScopeStats(timer);
  }

  capture_data_.UpdateScopeStats(kTimerInfoWithInvalidScopeId);

  EXPECT_NEAR(capture_data_.GetScopeStatsOrDefault(kFirstId).variance_ns(), kFirstVariance, 1.0);
  EXPECT_NEAR(capture_data_.GetScopeStatsOrDefault(kSecondId).variance_ns(), kSecondVariance, 1.0);
}

// The dataset contains 208'916 durations acquired in the course of 22 seconds.
// The file first line of the file contains the expected variance. The rest of the lines store
// durations one per line. The last line is empty.
const auto [kScimitarVariance, kScimitarTimers] = [] {
  std::filesystem::path path = orbit_test::GetTestdataDir() / "scimitar_variance_and_durations.csv";
  const ErrorMessageOr<std::string> file_content_or_error = orbit_base::ReadFileToString(path);
  EXPECT_TRUE(file_content_or_error.has_value());
  const std::string& file_content = file_content_or_error.value();
  const std::vector<std::string_view> tokens = absl::StrSplit(file_content, '\n');

  double expected_variance;
  EXPECT_TRUE(absl::SimpleAtod(*tokens.begin(), &expected_variance));

  std::vector<TimerInfo> timers;
  std::transform(std::begin(tokens) + 1, std::end(tokens) - 1, std::back_inserter(timers),
                 [](const std::string_view line) {
                   const uint64_t duration = std::stoull(std::string(line));
                   TimerInfo timer;
                   timer.set_function_id(kFirstId);
                   timer.set_start(0);
                   timer.set_end(duration);
                   return timer;
                 });
  return std::make_tuple(expected_variance, timers);
}();

TEST_F(CaptureDataTest, VarianceIsCorrectOnScimitarDataset) {
  for (const TimerInfo& timer : kScimitarTimers) {
    capture_data_.UpdateScopeStats(timer);
  }

  const double actual_variance = capture_data_.GetScopeStatsOrDefault(kFirstId).variance_ns();
  EXPECT_LE(abs(actual_variance / kScimitarVariance - 1.0), 1e-5);
}

constexpr size_t kNumberOfTimesWeRepeatScimitarDataset = 100;

// Here we simulate a dataset of 20'891'600 acquired in the course of 36 minutes
TEST_F(CaptureDataTest, VarianceIsCorrectOnRepeatedScimitarDataset) {
  for (size_t i = 0; i < kNumberOfTimesWeRepeatScimitarDataset; ++i) {
    for (const TimerInfo& timer : kScimitarTimers) {
      capture_data_.UpdateScopeStats(timer);
    }
  }

  const double actual_variance = capture_data_.GetScopeStatsOrDefault(kFirstId).variance_ns();
  EXPECT_LE(abs(actual_variance / kScimitarVariance - 1.0), 1e-5);
}

TEST_F(CaptureDataTest, UpdateTimerDurationsIsCorrect) {
  for (const TimerInfo& timer : kTimerInfos) {
    capture_data_.GetThreadTrackDataProvider()->AddTimer(timer);
  }

  capture_data_.OnCaptureComplete();

  const std::vector<uint64_t>* durations_first =
      capture_data_.GetSortedTimerDurationsForScopeId(kFirstId);
  EXPECT_EQ(*durations_first, std::vector(std::begin(kSortedDurationsForFirstId),
                                          std::end(kSortedDurationsForFirstId)));

  const std::vector<uint64_t>* durations_second =
      capture_data_.GetSortedTimerDurationsForScopeId(kSecondId);
  EXPECT_EQ(*durations_second, std::vector(std::begin(kSortedDurationsForSecondId),
                                           std::end(kSortedDurationsForSecondId)));

  EXPECT_THAT(capture_data_.GetSortedTimerDurationsForScopeId(kInvalidScopeId), testing::IsNull());
}

}  // namespace orbit_client_data