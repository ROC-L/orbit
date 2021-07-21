// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LINUX_TRACING_THREAD_STATE_VISITOR_H_
#define LINUX_TRACING_THREAD_STATE_VISITOR_H_

#include <absl/container/flat_hash_map.h>
#include <absl/hash/hash.h>
#include <sys/types.h>

#include <atomic>
#include <cstdint>
#include <optional>
#include <vector>

#include "ContextSwitchManager.h"
#include "PerfEvent.h"
#include "PerfEventVisitor.h"
#include "ThreadStateManager.h"
#include "TracingInterface/TracerListener.h"
#include "capture.pb.h"

namespace orbit_linux_tracing {

// This PerfEventVisitor visits PerfEvents associated with scheduling slices and thread states,
// processes them using ContextSwitchManager and ThreadStateManager, and passes the results to the
// specified TracerListener.
// As for some of these events the process id is not available, but only the thread id, this class
// also keeps the association between tids and pids system wide. The initial association extracted
// from the proc filesystem is passed by calling ProcessInitialTidToPidAssociation for each thread,
// and is updated with ForkPerfEvents (and also ExitPerfEvents, see Visit(ExitPerfEvent*)).
// For thread states, we are able to collect partial slices at the beginning and at the end of the
// capture, hence the ProcessInitialState and ProcessRemainingOpenStates methods.
// Also, we only process thread states of the process with pid specified with
// SetThreadStatePidFilter (so that we can collect thread states only for the process we are
// profiling). For this we also need the system-wide association between tids and pids.
class SwitchesStatesNamesVisitor : public PerfEventVisitor {
 public:
  explicit SwitchesStatesNamesVisitor(orbit_tracing_interface::TracerListener* listener)
      : listener_{listener} {
    CHECK(listener_ != nullptr);
  }

  void SetThreadStateCounter(std::atomic<uint64_t>* thread_state_counter) {
    thread_state_counter_ = thread_state_counter;
  }

  void SetProduceSchedulingSlices(bool produce_scheduling_slices) {
    produce_scheduling_slices_ = produce_scheduling_slices;
  }
  void SetThreadStatePidFilter(pid_t pid) { thread_state_pid_filter_ = pid; }

  void ProcessInitialTidToPidAssociation(pid_t tid, pid_t pid);
  void Visit(ForkPerfEvent* event) override;
  void Visit(ExitPerfEvent* event) override;

  void ProcessInitialState(uint64_t timestamp_ns, pid_t tid, char state_char);
  void Visit(TaskNewtaskPerfEvent* event) override;
  void Visit(SchedSwitchPerfEvent* event) override;
  void Visit(SchedWakeupPerfEvent* event) override;
  void ProcessRemainingOpenStates(uint64_t timestamp_ns);

  void Visit(TaskRenamePerfEvent* event) override;

 private:
  static std::optional<orbit_grpc_protos::ThreadStateSlice::ThreadState> GetThreadStateFromChar(
      char c);
  static orbit_grpc_protos::ThreadStateSlice::ThreadState GetThreadStateFromBits(uint64_t bits);

  orbit_tracing_interface::TracerListener* listener_;
  std::atomic<uint64_t>* thread_state_counter_ = nullptr;

  bool produce_scheduling_slices_ = false;

  bool TidMatchesPidFilter(pid_t tid);
  std::optional<pid_t> GetPidOfTid(pid_t tid);
  static constexpr pid_t kPidFilterNoThreadState = -1;
  pid_t thread_state_pid_filter_ = kPidFilterNoThreadState;
  absl::flat_hash_map<pid_t, pid_t> tid_to_pid_association_;

  ContextSwitchManager switch_manager_;
  ThreadStateManager state_manager_;
};

}  // namespace orbit_linux_tracing

#endif  // LINUX_TRACING_THREAD_STATE_VISITOR_H_
