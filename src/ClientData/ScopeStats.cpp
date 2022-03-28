// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ClientData/ScopeStats.h"

#include <cmath>

namespace orbit_client_data {
void ScopeStats::UpdateStats(uint64_t elapsed_nanos) {
  double old_avg = static_cast<double>(average_time_ns());
  count_++;
  total_time_ns_ += elapsed_nanos;
  double new_avg = static_cast<double>(average_time_ns());  // it should be double for arithmetics
  double elapsed_nanos_double = static_cast<double>(elapsed_nanos);

  // variance(N) = ( (N-1)*variance(N-1) + (x-avg(N))*(x-avg(N-1)) ) / N
  variance_ns_ = ((static_cast<double>(count_ - 1) * variance_ns_ +
                   (elapsed_nanos_double - new_avg) * (elapsed_nanos_double - old_avg)) /
                  static_cast<double>(count_));
  // std_dev = sqrt(variance)
  std_dev_ns_ = static_cast<uint64_t>(std::sqrt(variance_ns_));

  if (max_ns_ < elapsed_nanos) {
    max_ns_ = elapsed_nanos;
  }

  if (min_ns_ == 0 || elapsed_nanos < min_ns_) {
    min_ns_ = elapsed_nanos;
  }
}
}  // namespace orbit_client_data