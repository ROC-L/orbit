// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STATISTICS_DATA_SET_H_
#define STATISTICS_DATA_SET_H_

#include <cstdint>
#include <optional>
#include <vector>

namespace orbit_statistics {

class DataSet {
  DataSet(const std::vector<uint64_t>* data, uint64_t min, uint64_t max)
      : data_(data), min_(min), max_(max) {}
  const std::vector<uint64_t>* data_;
  uint64_t min_;
  uint64_t max_;

 public:
  [[nodiscard]] const std::vector<uint64_t>* GetData() const { return data_; }
  [[nodiscard]] uint64_t GetMin() const { return min_; }
  [[nodiscard]] uint64_t GetMax() const { return max_; }

  friend std::optional<DataSet> CreateDataSet(const std::vector<uint64_t>* data);
};

[[nodiscard]] std::optional<DataSet> CreateDataSet(const std::vector<uint64_t>* data);

}  // namespace orbit_statistics
#endif  // STATISTICS_DATA_SET_H_
