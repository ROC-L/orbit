// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLIENT_DATA_SCOPE_TREE_TIMER_DATA_H_
#define CLIENT_DATA_SCOPE_TREE_TIMER_DATA_H_

#include <absl/synchronization/mutex.h>

#include <vector>

#include "Containers/ScopeTree.h"
#include "TimerData.h"
#include "TimerDataInterface.h"

namespace orbit_client_data {

// Stores all the timers from a particular ThreadId. Provides queries to get timers in a certain
// range as well as metadata from them.
class ScopeTreeTimerData final : public TimerDataInterface {
 public:
  enum class ScopeTreeUpdateType { kAlways, kOnCaptureComplete, kNever };
  explicit ScopeTreeTimerData(int64_t thread_id = -1, ScopeTreeUpdateType scope_tree_update_type =
                                                          ScopeTreeUpdateType::kAlways)
      : thread_id_(thread_id), scope_tree_update_type_(scope_tree_update_type){};

  [[nodiscard]] int64_t GetThreadId() const override { return thread_id_; }
  [[nodiscard]] TimerMetadata GetTimerMetadata() const override;

  [[nodiscard]] std::vector<const TimerChain*> GetChains() const override {
    return timer_data_.GetChains();
  }

  // We are using a ScopeTree to automatically manage timers and their depth, no need to set it
  // here.
  const orbit_client_protos::TimerInfo& AddTimer(orbit_client_protos::TimerInfo timer_info,
                                                 uint32_t /*unused_depth*/ = 0) override;
  void OnCaptureComplete() override;

  [[nodiscard]] std::vector<const orbit_client_protos::TimerInfo*> GetTimers(
      uint64_t start_ns = std::numeric_limits<uint64_t>::min(),
      uint64_t end_ns = std::numeric_limits<uint64_t>::max()) const override;
  [[nodiscard]] std::vector<const orbit_client_protos::TimerInfo*> GetTimersAtDepth(
      uint32_t depth, uint64_t start_ns = std::numeric_limits<uint64_t>::min(),
      uint64_t end_ns = std::numeric_limits<uint64_t>::max()) const;
  // This method avoids returning two timers that map to the same pixel in the screen, so is
  // especially useful when there are many timers in the screen (zooming-out for example).
  // The overall complexity is O(log(num_timers) * resolution). Resolution should be the number
  // of pixels-width where timers will be drawn.
  [[nodiscard]] std::vector<const orbit_client_protos::TimerInfo*> GetTimersAtDepthDiscretized(
      uint32_t depth, uint32_t resolution, uint64_t start_ns, uint64_t end_ns) const;

  [[nodiscard]] const orbit_client_protos::TimerInfo* GetLeft(
      const orbit_client_protos::TimerInfo& timer) const override;
  [[nodiscard]] const orbit_client_protos::TimerInfo* GetRight(
      const orbit_client_protos::TimerInfo& timer) const override;
  [[nodiscard]] const orbit_client_protos::TimerInfo* GetUp(
      const orbit_client_protos::TimerInfo& timer) const override;
  [[nodiscard]] const orbit_client_protos::TimerInfo* GetDown(
      const orbit_client_protos::TimerInfo& timer) const override;

 private:
  const int64_t thread_id_;
  mutable absl::Mutex scope_tree_mutex_;
  orbit_containers::ScopeTree<const orbit_client_protos::TimerInfo> scope_tree_
      GUARDED_BY(scope_tree_mutex_);
  ScopeTreeUpdateType scope_tree_update_type_;

  TimerData timer_data_;
};

}  // namespace orbit_client_data

#endif  // CLIENT_DATA_SCOPE_TREE_TIMER_DATA_H_
