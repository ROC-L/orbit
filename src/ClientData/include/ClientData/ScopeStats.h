// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CLIENT_DATA_FUNCTION_STATS_H_
#define CLIENT_DATA_FUNCTION_STATS_H_

#include <stdint.h>

namespace orbit_client_data {

// A simple class that keeps track of some basic statistics for a particular scope id (e.g. a
// particular function).
// Usage: Whenever we have a new occurrence of a particular scope, `UpdateStats` needs to be called
// with the respective duration.
// TODO(b/226880177): Currently, we need to recalculate the stats after the capture completes, to
//  fix rounding issues.
class ScopeStats {
 public:
  explicit ScopeStats() = default;

  void UpdateStats(uint64_t elapsed_nanos);

  [[nodiscard]] uint64_t count() const { return count_; }
  void set_count(uint64_t count) { count_ = count; }

  [[nodiscard]] uint64_t total_time_ns() const { return total_time_ns_; }
  void set_total_time_ns(uint64_t set_total_time_ns) { total_time_ns_ = set_total_time_ns; }

  [[nodiscard]] uint64_t average_time_ns() const {
    if (count_ == 0) {
      return 0;
    }
    return total_time_ns_ / count_;
  }

  [[nodiscard]] uint64_t min_ns() const { return min_ns_; }
  void set_min_ns(uint64_t min_ns) { min_ns_ = min_ns; }

  [[nodiscard]] uint64_t max_ns() const { return max_ns_; }
  void set_max_ns(uint64_t max_ns) { max_ns_ = max_ns; }

  [[nodiscard]] double variance_ns() const { return variance_ns_; }
  void set_variance_ns(double variance_ns) { variance_ns_ = variance_ns; }

  [[nodiscard]] uint64_t std_dev_ns() const { return std_dev_ns_; }
  void set_std_dev_ns(uint64_t std_dev_ns) { std_dev_ns_ = std_dev_ns; }

 private:
  uint64_t count_;
  uint64_t total_time_ns_;
  uint64_t min_ns_;
  uint64_t max_ns_;
  double variance_ns_;
  uint64_t std_dev_ns_;
};

}  // namespace orbit_client_data

#endif  // CLIENT_DATA_FUNCTION_STATS_H_
