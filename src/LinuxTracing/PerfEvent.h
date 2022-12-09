// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LINUX_TRACING_PERF_EVENT_H_
#define LINUX_TRACING_PERF_EVENT_H_

#include <absl/base/casts.h>
#include <absl/types/span.h>
#include <asm/perf_regs.h>
#include <string.h>
#include <sys/types.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "GrpcProtos/Constants.h"
#include "OrbitBase/MakeUniqueForOverwrite.h"
#include "PerfEventOrderedStream.h"
#include "PerfEventRecords.h"

namespace orbit_linux_tracing {

class PerfEventVisitor;

[[nodiscard]] std::array<uint64_t, PERF_REG_X86_64_MAX>
perf_event_sample_regs_user_all_to_register_array(const perf_event_sample_regs_user_all& regs);

// This template class holds data from a specific perf_event_open event, based on the type argument.
// The top-level fields (`timestamp` and `ordered_in_file_descriptor`) are common to all events,
// while the `data` field holds the data specific to the individual event.
template <typename PerfEventDataT>
struct TypedPerfEvent {
  uint64_t timestamp = 0;
  PerfEventOrderedStream ordered_stream = PerfEventOrderedStream::kNone;
  PerfEventDataT data;
};

struct ForkPerfEventData {
  pid_t pid;
  pid_t tid;
};
using ForkPerfEvent = TypedPerfEvent<ForkPerfEventData>;

struct ExitPerfEventData {
  pid_t pid;
  pid_t tid;
};
using ExitPerfEvent = TypedPerfEvent<ExitPerfEventData>;

struct LostPerfEventData {
  uint64_t previous_timestamp = 0;
};
using LostPerfEvent = TypedPerfEvent<LostPerfEventData>;

// This struct doesn't correspond to any event generated by perf_event_open. Rather, these events
// are produced by PerfEventProcessor. We need them to be part of the same PerfEvent hierarchy.
struct DiscardedPerfEventData {
  uint64_t begin_timestamp_ns;
};
using DiscardedPerfEvent = TypedPerfEvent<DiscardedPerfEventData>;

struct StackSamplePerfEventData {
  [[nodiscard]] const perf_event_sample_regs_user_all& GetRegisters() const {
    return *absl::bit_cast<const perf_event_sample_regs_user_all*>(regs.get());
  }
  [[nodiscard]] std::array<uint64_t, PERF_REG_X86_64_MAX> GetRegistersAsArray() const {
    return perf_event_sample_regs_user_all_to_register_array(GetRegisters());
  }
  [[nodiscard]] const uint8_t* GetStackData() const { return data.get(); }
  // Handing out this non const pointer makes the stack data mutable even if the
  // StackSamplePerfEvent is const.  This mutablility is needed in
  // UprobesReturnAddressManager::PatchSample.
  [[nodiscard]] uint8_t* GetMutableStackData() const { return data.get(); }
  [[nodiscard]] uint64_t GetStackSize() const { return dyn_size; }
  [[nodiscard]] pid_t GetCallstackPidOrMinusOne() const { return pid; }
  [[nodiscard]] pid_t GetCallstackTid() const { return tid; }

  pid_t pid;
  pid_t tid;
  std::unique_ptr<uint64_t[]> regs;
  uint64_t dyn_size;
  std::unique_ptr<uint8_t[]> data;
};
using StackSamplePerfEvent = TypedPerfEvent<StackSamplePerfEventData>;

struct CallchainSamplePerfEventData {
  [[nodiscard]] const uint64_t* GetCallchain() const { return ips.get(); }
  [[nodiscard]] uint64_t GetCallchainSize() const { return ips_size; }
  [[nodiscard]] const perf_event_sample_regs_user_all& GetRegisters() const {
    return *absl::bit_cast<const perf_event_sample_regs_user_all*>(regs.get());
  }
  [[nodiscard]] std::array<uint64_t, PERF_REG_X86_64_MAX> GetRegistersAsArray() const {
    return perf_event_sample_regs_user_all_to_register_array(GetRegisters());
  }
  [[nodiscard]] const uint8_t* GetStackData() const { return data.get(); }
  void SetIps(absl::Span<const uint64_t> new_ips) const {
    ips_size = new_ips.size();
    ips = make_unique_for_overwrite<uint64_t[]>(ips_size);
    memcpy(ips.get(), new_ips.data(), ips_size * sizeof(uint64_t));
  }
  [[nodiscard]] std::vector<uint64_t> CopyOfIpsAsVector() const {
    return std::vector<uint64_t>(ips.get(), ips.get() + ips_size);
  }
  [[nodiscard]] pid_t GetCallstackPidOrMinusOne() const { return pid; }
  [[nodiscard]] pid_t GetCallstackTid() const { return tid; }

  pid_t pid;
  pid_t tid;
  // Mutability is needed in SetIps which in turn is needed by
  // LeafFunctionCallManager::PatchCallerOfLeafFunction.
  mutable uint64_t ips_size;
  mutable std::unique_ptr<uint64_t[]> ips;
  std::unique_ptr<uint64_t[]> regs;
  std::unique_ptr<uint8_t[]> data;
};
using CallchainSamplePerfEvent = TypedPerfEvent<CallchainSamplePerfEventData>;

struct UprobesPerfEventData {
  pid_t pid;
  pid_t tid;
  uint32_t cpu;
  uint64_t function_id = orbit_grpc_protos::kInvalidFunctionId;
  uint64_t sp;
  uint64_t ip;
  uint64_t return_address;
};
using UprobesPerfEvent = TypedPerfEvent<UprobesPerfEventData>;

struct UprobesWithArgumentsPerfEventData {
  pid_t pid;
  pid_t tid;
  uint32_t cpu;
  uint64_t function_id = orbit_grpc_protos::kInvalidFunctionId;
  uint64_t return_address;
  perf_event_sample_regs_user_sp_ip_arguments regs;
};
using UprobesWithArgumentsPerfEvent = TypedPerfEvent<UprobesWithArgumentsPerfEventData>;

struct UprobesWithStackPerfEventData {
  [[nodiscard]] const perf_event_sample_regs_user_sp& GetRegisters() const {
    return *absl::bit_cast<const perf_event_sample_regs_user_sp*>(regs.get());
  }
  uint64_t stream_id;
  pid_t pid;
  pid_t tid;
  std::unique_ptr<uint64_t[]> regs;

  uint64_t dyn_size;
  // This mutablility allows moving the data out of this class in the UprobesUnwindingVisitor even
  // if we only have a const reference there. This requires the explicit knowledge that there is
  // only one visitor being applied to this event.
  mutable std::unique_ptr<uint8_t[]> data;
};
using UprobesWithStackPerfEvent = TypedPerfEvent<UprobesWithStackPerfEventData>;

struct UretprobesPerfEventData {
  pid_t pid;
  pid_t tid;
};
using UretprobesPerfEvent = TypedPerfEvent<UretprobesPerfEventData>;

struct UretprobesWithReturnValuePerfEventData {
  pid_t pid;
  pid_t tid;
  uint64_t rax;
};
using UretprobesWithReturnValuePerfEvent = TypedPerfEvent<UretprobesWithReturnValuePerfEventData>;

// UserSpaceFunctionEntryPerfEvent and UserSpaceFunctionExitPerfEvent don't correspond to records
// produced by perf_event_open, but rather to the FunctionEntry and FunctionExit protos emitted by
// user space instrumentation. They are processed in LinuxTracing because, just like with uprobes,
// unwinding and dynamic instrumentation need to work together.
struct UserSpaceFunctionEntryPerfEventData {
  pid_t pid;
  pid_t tid;
  uint64_t function_id = orbit_grpc_protos::kInvalidFunctionId;
  uint64_t sp;
  uint64_t return_address;
};
using UserSpaceFunctionEntryPerfEvent = TypedPerfEvent<UserSpaceFunctionEntryPerfEventData>;

struct UserSpaceFunctionExitPerfEventData {
  pid_t pid;
  pid_t tid;
};
using UserSpaceFunctionExitPerfEvent = TypedPerfEvent<UserSpaceFunctionExitPerfEventData>;

struct MmapPerfEventData {
  uint64_t address;
  uint64_t length;
  uint64_t page_offset;
  std::string filename;
  bool executable;
  pid_t pid;
};
using MmapPerfEvent = TypedPerfEvent<MmapPerfEventData>;

struct GenericTracepointPerfEventData {
  pid_t pid;
  pid_t tid;
  uint32_t cpu;
};
using GenericTracepointPerfEvent = TypedPerfEvent<GenericTracepointPerfEventData>;

struct TaskNewtaskPerfEventData {
  char comm[16];
  pid_t new_tid;
  pid_t was_created_by_tid;
  pid_t was_created_by_pid;
};
using TaskNewtaskPerfEvent = TypedPerfEvent<TaskNewtaskPerfEventData>;

struct TaskRenamePerfEventData {
  char newcomm[16];
  pid_t renamed_tid;
};
using TaskRenamePerfEvent = TypedPerfEvent<TaskRenamePerfEventData>;

struct SchedSwitchPerfEventData {
  uint32_t cpu;
  pid_t prev_pid_or_minus_one;
  pid_t prev_tid;
  int64_t prev_state;
  int32_t next_tid;
};
using SchedSwitchPerfEvent = TypedPerfEvent<SchedSwitchPerfEventData>;

struct SchedWakeupPerfEventData {
  pid_t woken_tid;
  pid_t was_unblocked_by_tid;
  pid_t was_unblocked_by_pid;
};
using SchedWakeupPerfEvent = TypedPerfEvent<SchedWakeupPerfEventData>;

struct AmdgpuCsIoctlPerfEventData {
  pid_t pid;
  pid_t tid;
  uint32_t context;
  uint32_t seqno;
  std::string timeline_string;
};
using AmdgpuCsIoctlPerfEvent = TypedPerfEvent<AmdgpuCsIoctlPerfEventData>;

struct AmdgpuSchedRunJobPerfEventData {
  pid_t pid;
  pid_t tid;
  uint32_t context;
  uint32_t seqno;
  std::string timeline_string;
};
using AmdgpuSchedRunJobPerfEvent = TypedPerfEvent<AmdgpuSchedRunJobPerfEventData>;

struct DmaFenceSignaledPerfEventData {
  pid_t pid;
  pid_t tid;
  uint32_t context;
  uint32_t seqno;
  std::string timeline_string;
};
using DmaFenceSignaledPerfEvent = TypedPerfEvent<DmaFenceSignaledPerfEventData>;

struct SchedWakeupWithCallchainPerfEventData {
  [[nodiscard]] const uint64_t* GetCallchain() const { return ips.get(); }
  [[nodiscard]] uint64_t GetCallchainSize() const { return ips_size; }
  [[nodiscard]] std::array<uint64_t, PERF_REG_X86_64_MAX> GetRegistersAsArray() const {
    return perf_event_sample_regs_user_all_to_register_array(GetRegisters());
  }
  [[nodiscard]] const perf_event_sample_regs_user_all& GetRegisters() const {
    return *absl::bit_cast<const perf_event_sample_regs_user_all*>(regs.get());
  }
  [[nodiscard]] const uint8_t* GetStackData() const { return data.get(); }
  void SetIps(absl::Span<const uint64_t> new_ips) const {
    ips_size = new_ips.size();
    ips = make_unique_for_overwrite<uint64_t[]>(ips_size);
    memcpy(ips.get(), new_ips.data(), ips_size * sizeof(uint64_t));
  }
  [[nodiscard]] std::vector<uint64_t> CopyOfIpsAsVector() const {
    return std::vector<uint64_t>(ips.get(), ips.get() + ips_size);
  }
  [[nodiscard]] pid_t GetCallstackPidOrMinusOne() const { return was_unblocked_by_pid; }
  [[nodiscard]] pid_t GetCallstackTid() const { return was_unblocked_by_tid; }

  pid_t woken_tid;
  pid_t was_unblocked_by_tid;
  pid_t was_unblocked_by_pid;
  // Mutability is needed in SetIps which in turn is needed by
  // LeafFunctionCallManager::PatchCallerOfLeafFunction.
  mutable uint64_t ips_size;
  mutable std::unique_ptr<uint64_t[]> ips;
  std::unique_ptr<uint64_t[]> regs;
  std::unique_ptr<uint8_t[]> data;
};
using SchedWakeupWithCallchainPerfEvent = TypedPerfEvent<SchedWakeupWithCallchainPerfEventData>;

struct SchedSwitchWithCallchainPerfEventData {
  [[nodiscard]] const uint64_t* GetCallchain() const { return ips.get(); }
  [[nodiscard]] uint64_t GetCallchainSize() const { return ips_size; }
  [[nodiscard]] std::array<uint64_t, PERF_REG_X86_64_MAX> GetRegistersAsArray() const {
    return perf_event_sample_regs_user_all_to_register_array(GetRegisters());
  }
  [[nodiscard]] const perf_event_sample_regs_user_all& GetRegisters() const {
    return *absl::bit_cast<const perf_event_sample_regs_user_all*>(regs.get());
  }
  [[nodiscard]] const uint8_t* GetStackData() const { return data.get(); }
  void SetIps(absl::Span<const uint64_t> new_ips) const {
    ips_size = new_ips.size();
    ips = make_unique_for_overwrite<uint64_t[]>(ips_size);
    memcpy(ips.get(), new_ips.data(), ips_size * sizeof(uint64_t));
  }
  [[nodiscard]] std::vector<uint64_t> CopyOfIpsAsVector() const {
    return std::vector<uint64_t>(ips.get(), ips.get() + ips_size);
  }
  [[nodiscard]] pid_t GetCallstackPidOrMinusOne() const { return prev_pid_or_minus_one; }
  [[nodiscard]] pid_t GetCallstackTid() const { return prev_tid; }

  uint32_t cpu;
  pid_t prev_pid_or_minus_one;
  pid_t prev_tid;
  int64_t prev_state;
  int32_t next_tid;
  // Mutability is needed in SetIps which in turn is needed by
  // LeafFunctionCallManager::PatchCallerOfLeafFunction.
  mutable uint64_t ips_size;
  mutable std::unique_ptr<uint64_t[]> ips;
  std::unique_ptr<uint64_t[]> regs;
  std::unique_ptr<uint8_t[]> data;
};
using SchedSwitchWithCallchainPerfEvent = TypedPerfEvent<SchedSwitchWithCallchainPerfEventData>;

struct SchedWakeupWithStackPerfEventData {
  [[nodiscard]] std::array<uint64_t, PERF_REG_X86_64_MAX> GetRegistersAsArray() const {
    return perf_event_sample_regs_user_all_to_register_array(GetRegisters());
  }
  [[nodiscard]] const perf_event_sample_regs_user_all& GetRegisters() const {
    return *absl::bit_cast<const perf_event_sample_regs_user_all*>(regs.get());
  }
  [[nodiscard]] const uint8_t* GetStackData() const { return data.get(); }
  // Handing out this non const pointer makes the stack data mutable even if the
  // StackSamplePerfEvent is const.  This mutablility is needed in
  // UprobesReturnAddressManager::PatchSample.
  [[nodiscard]] uint8_t* GetMutableStackData() const { return data.get(); }
  [[nodiscard]] uint64_t GetStackSize() const { return dyn_size; }
  [[nodiscard]] pid_t GetCallstackPidOrMinusOne() const { return was_unblocked_by_pid; }
  [[nodiscard]] pid_t GetCallstackTid() const { return was_unblocked_by_tid; }

  pid_t woken_tid;
  pid_t was_unblocked_by_tid;
  pid_t was_unblocked_by_pid;
  std::unique_ptr<uint64_t[]> regs;
  uint64_t dyn_size;
  std::unique_ptr<uint8_t[]> data;
};
using SchedWakeupWithStackPerfEvent = TypedPerfEvent<SchedWakeupWithStackPerfEventData>;

struct SchedSwitchWithStackPerfEventData {
  [[nodiscard]] std::array<uint64_t, PERF_REG_X86_64_MAX> GetRegistersAsArray() const {
    return perf_event_sample_regs_user_all_to_register_array(GetRegisters());
  }
  [[nodiscard]] const perf_event_sample_regs_user_all& GetRegisters() const {
    return *absl::bit_cast<const perf_event_sample_regs_user_all*>(regs.get());
  }
  [[nodiscard]] const uint8_t* GetStackData() const { return data.get(); }
  // Handing out this non const pointer makes the stack data mutable even if the
  // StackSamplePerfEvent is const.  This mutablility is needed in
  // UprobesReturnAddressManager::PatchSample.
  [[nodiscard]] uint8_t* GetMutableStackData() const { return data.get(); }
  [[nodiscard]] uint64_t GetStackSize() const { return dyn_size; }
  [[nodiscard]] pid_t GetCallstackPidOrMinusOne() const { return prev_pid_or_minus_one; }
  [[nodiscard]] pid_t GetCallstackTid() const { return prev_tid; }

  uint32_t cpu;
  pid_t prev_pid_or_minus_one;
  pid_t prev_tid;
  int64_t prev_state;
  int32_t next_tid;
  std::unique_ptr<uint64_t[]> regs;
  uint64_t dyn_size;
  std::unique_ptr<uint8_t[]> data;
};
using SchedSwitchWithStackPerfEvent = TypedPerfEvent<SchedSwitchWithStackPerfEventData>;

// This struct holds the data we need from any of the possible perf_event_open events that we
// collect. The top-level fields (`timestamp` and `ordered_in_file_descriptor`) are common to all
// events, while each of the possible `...PerfEventData`s in the `std::variant` contains the data
// specific to each possible event.
//
// We use `std::variant` instead of a more traditional class hierarchy because the latter requires
// heap-allocating the objects, which is overall more expensive than the allocating in place and
// copying that `std::variant` allows.
struct PerfEvent {
  template <typename PerfEventDataT>
  // NOLINTNEXTLINE(google-explicit-constructor): Non-explicit constructor for conversions.
  PerfEvent(const TypedPerfEvent<PerfEventDataT>& typed_perf_event)
      : timestamp{typed_perf_event.timestamp},
        ordered_stream{typed_perf_event.ordered_stream},
        data{typed_perf_event.data} {}

  template <typename PerfEventDataT>
  // NOLINTNEXTLINE(google-explicit-constructor): Non-explicit constructor for conversions.
  PerfEvent(TypedPerfEvent<PerfEventDataT>&& typed_perf_event)
      : timestamp{typed_perf_event.timestamp},
        ordered_stream{typed_perf_event.ordered_stream},
        data{std::move(typed_perf_event.data)} {}

  uint64_t timestamp;
  PerfEventOrderedStream ordered_stream = PerfEventOrderedStream::kNone;
  std::variant<ForkPerfEventData, ExitPerfEventData, LostPerfEventData, DiscardedPerfEventData,
               StackSamplePerfEventData, CallchainSamplePerfEventData, UprobesPerfEventData,
               UprobesWithArgumentsPerfEventData, UprobesWithStackPerfEventData,
               UretprobesPerfEventData, UretprobesWithReturnValuePerfEventData,
               UserSpaceFunctionEntryPerfEventData, UserSpaceFunctionExitPerfEventData,
               MmapPerfEventData, GenericTracepointPerfEventData, TaskNewtaskPerfEventData,
               TaskRenamePerfEventData, SchedSwitchPerfEventData, SchedWakeupPerfEventData,
               SchedSwitchWithCallchainPerfEventData, SchedWakeupWithCallchainPerfEventData,
               SchedSwitchWithStackPerfEventData, SchedWakeupWithStackPerfEventData,
               AmdgpuCsIoctlPerfEventData, AmdgpuSchedRunJobPerfEventData,
               DmaFenceSignaledPerfEventData>
      data;

  void Accept(PerfEventVisitor* visitor) const;
};

}  // namespace orbit_linux_tracing

#endif  // LINUX_TRACING_PERF_EVENT_H_
