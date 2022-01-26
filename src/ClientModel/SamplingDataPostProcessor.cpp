// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ClientModel/SamplingDataPostProcessor.h"

#include <absl/hash/hash.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ClientData/CallstackEvent.h"
#include "ClientData/CallstackTypes.h"
#include "ClientData/ModuleAndFunctionLookup.h"
#include "ClientProtos/capture_data.pb.h"
#include "OrbitBase/Logging.h"
#include "OrbitBase/ThreadConstants.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"

using orbit_client_data::CallstackData;
using orbit_client_data::CallstackEvent;
using orbit_client_data::CaptureData;
using orbit_client_data::ModuleManager;
using orbit_client_data::PostProcessedSamplingData;
using orbit_client_data::SampledFunction;
using orbit_client_data::ThreadID;
using orbit_client_data::ThreadSampleData;

using orbit_client_protos::CallstackInfo;

namespace orbit_client_model {

namespace {

// class equivalent of orbit_client_protos::CallstackInfo for use as key of absl::flat_hash_map.
class CallstackInfoAsClass {
 public:
  CallstackInfoAsClass(std::vector<uint64_t> frames, CallstackInfo::CallstackType type)
      : frames_(std::move(frames)), type_(type) {}

  [[nodiscard]] const std::vector<uint64_t>& frames() const { return frames_; }
  [[nodiscard]] CallstackInfo::CallstackType type() const { return type_; }

 private:
  std::vector<uint64_t> frames_;
  CallstackInfo::CallstackType type_;
};

template <typename H>
H AbslHashValue(H h, const CallstackInfoAsClass& o) {
  return H::combine(std::move(h), o.frames(), o.type());
}

using CallstackInfoAsPairWithLvalueRefToFrames =
    std::pair<const std::vector<uint64_t>&, CallstackInfo::CallstackType>;

// CallstackInfoHash and CallstackInfoEq allow heterogeneous lookup in
// SamplingDataPostProcessor::resolved_callstack_to_id_;
struct CallstackInfoHash {
  using is_transparent = void;  // Makes this functor transparent, enabling heterogeneous lookup.

  size_t operator()(const CallstackInfoAsClass& o) const {
    return absl::Hash<CallstackInfoAsClass>{}(o);
  }

  size_t operator()(const CallstackInfoAsPairWithLvalueRefToFrames& p) const {
    return absl::Hash<CallstackInfoAsPairWithLvalueRefToFrames>{}(p);
  }
};

struct CallstackInfoEq {
  using is_transparent = void;  // Makes this functor transparent, enabling heterogeneous lookup.

  bool operator()(const CallstackInfoAsClass& lhs, const CallstackInfoAsClass& rhs) const {
    return std::equal(lhs.frames().begin(), lhs.frames().end(), rhs.frames().begin(),
                      rhs.frames().end()) &&
           lhs.type() == rhs.type();
  }

  bool operator()(const CallstackInfoAsClass& lhs,
                  const CallstackInfoAsPairWithLvalueRefToFrames& rhs) const {
    return std::equal(lhs.frames().begin(), lhs.frames().end(), rhs.first.begin(),
                      rhs.first.end()) &&
           lhs.type() == rhs.second;
  }
};

class SamplingDataPostProcessor {
 public:
  explicit SamplingDataPostProcessor() = default;
  SamplingDataPostProcessor& operator=(const SamplingDataPostProcessor& other) = default;
  SamplingDataPostProcessor(const SamplingDataPostProcessor& other) = default;

  SamplingDataPostProcessor(SamplingDataPostProcessor&& other) = default;
  SamplingDataPostProcessor& operator=(SamplingDataPostProcessor&& other) = default;

  PostProcessedSamplingData ProcessSamples(const CallstackData& callstack_data,
                                           const CaptureData& capture_data,
                                           const ModuleManager& module_manager,
                                           bool generate_summary);

 private:
  void ResolveCallstacks(const CallstackData& callstack_data, const CaptureData& capture_data,
                         const ModuleManager& module_manager);

  void MapAddressToFunctionAddress(uint64_t absolute_address, const CaptureData& capture_data,
                                   const ModuleManager& module_manager);

  void FillThreadSampleDataSampleReports(const CaptureData& capture_data,
                                         const ModuleManager& module_manager);

  // Filled by ProcessSamples.
  absl::flat_hash_map<ThreadID, ThreadSampleData> thread_id_to_sample_data_;
  absl::flat_hash_map<uint64_t, CallstackInfo> id_to_resolved_callstack_;
  absl::flat_hash_map<CallstackInfoAsClass, uint64_t, CallstackInfoHash, CallstackInfoEq>
      resolved_callstack_to_id_;
  absl::flat_hash_map<uint64_t, uint64_t> original_id_to_resolved_callstack_id_;
  absl::flat_hash_map<uint64_t, absl::flat_hash_set<uint64_t>>
      function_address_to_sampled_callstack_ids_;
  absl::flat_hash_map<uint64_t, uint64_t> exact_address_to_function_address_;
};

}  // namespace

PostProcessedSamplingData CreatePostProcessedSamplingData(const CallstackData& callstack_data,
                                                          const CaptureData& capture_data,
                                                          const ModuleManager& module_manager,
                                                          bool generate_summary) {
  ORBIT_SCOPED_TIMED_LOG("CreatePostProcessedSamplingData");
  return SamplingDataPostProcessor{}.ProcessSamples(callstack_data, capture_data, module_manager,
                                                    generate_summary);
}

namespace {
PostProcessedSamplingData SamplingDataPostProcessor::ProcessSamples(
    const CallstackData& callstack_data, const CaptureData& capture_data,
    const ModuleManager& module_manager, bool generate_summary) {
  // Unique call stacks and per thread data
  callstack_data.ForEachCallstackEvent([this, &callstack_data,
                                        generate_summary](const CallstackEvent& event) {
    ORBIT_CHECK(callstack_data.HasCallstack(event.callstack_id()));
    const orbit_client_protos::CallstackInfo* callstack_info =
        callstack_data.GetCallstack(event.callstack_id());

    // For non-kComplete callstacks, only use the innermost frame for statistics, as it's the
    // only one known to be correct. Note that, in the vast majority of cases, the innermost
    // frame is also the only one available.
    absl::flat_hash_set<uint64_t> unique_frames;
    ORBIT_CHECK(!callstack_info->frames().empty());
    if (callstack_info->type() == CallstackInfo::kComplete) {
      for (uint64_t frame : callstack_info->frames()) {
        unique_frames.insert(frame);
      }
    } else {
      unique_frames.insert(callstack_info->frames(0));
    }

    ThreadSampleData* thread_sample_data = &thread_id_to_sample_data_[event.thread_id()];
    thread_sample_data->thread_id = event.thread_id();
    thread_sample_data->samples_count++;
    thread_sample_data->sampled_callstack_id_to_events[event.callstack_id()].emplace_back(event);
    for (uint64_t frame : unique_frames) {
      thread_sample_data->sampled_address_to_count[frame]++;
    }

    if (!generate_summary) {
      return;
    }
    ThreadSampleData* all_thread_sample_data =
        &thread_id_to_sample_data_[orbit_base::kAllProcessThreadsTid];
    all_thread_sample_data->thread_id = orbit_base::kAllProcessThreadsTid;
    all_thread_sample_data->samples_count++;
    all_thread_sample_data->sampled_callstack_id_to_events[event.callstack_id()].emplace_back(
        event);
    for (uint64_t frame : unique_frames) {
      all_thread_sample_data->sampled_address_to_count[frame]++;
    }
  });

  ResolveCallstacks(callstack_data, capture_data, module_manager);

  for (auto& sample_data_it : thread_id_to_sample_data_) {
    ThreadSampleData* thread_sample_data = &sample_data_it.second;

    // Address count per sample per thread
    for (const auto& [sampled_callstack_id, callstack_events] :
         thread_sample_data->sampled_callstack_id_to_events) {
      uint64_t callstack_count = callstack_events.size();
      uint64_t resolved_callstack_id = original_id_to_resolved_callstack_id_[sampled_callstack_id];
      const CallstackInfo& resolved_callstack = id_to_resolved_callstack_[resolved_callstack_id];

      // "Exclusive" stat.
      ORBIT_CHECK(!resolved_callstack.frames().empty());
      thread_sample_data->resolved_address_to_exclusive_count[resolved_callstack.frames(0)] +=
          callstack_count;

      absl::flat_hash_set<uint64_t> unique_resolved_addresses;
      if (resolved_callstack.type() == CallstackInfo::kComplete) {
        for (uint64_t resolved_address : resolved_callstack.frames()) {
          unique_resolved_addresses.insert(resolved_address);
        }
      } else {
        // For non-kComplete callstacks, only use the innermost frame for statistics.
        unique_resolved_addresses.insert(resolved_callstack.frames(0));
      }

      // "Inclusive" stat.
      for (uint64_t resolved_address : unique_resolved_addresses) {
        thread_sample_data->resolved_address_to_count[resolved_address] += callstack_count;
      }

      // "Unwind errors" stat.
      if (resolved_callstack.type() != CallstackInfo::kComplete) {
        thread_sample_data->resolved_address_to_error_count[resolved_callstack.frames(0)] +=
            callstack_count;
      }
    }

    // For each thread, sort resolved (function) addresses by inclusive count.
    for (const auto& address_count_it : thread_sample_data->resolved_address_to_count) {
      const uint64_t address = address_count_it.first;
      const uint32_t count = address_count_it.second;
      thread_sample_data->sorted_count_to_resolved_address.insert(std::make_pair(count, address));
    }
  }

  FillThreadSampleDataSampleReports(capture_data, module_manager);

  return PostProcessedSamplingData(std::move(thread_id_to_sample_data_),
                                   std::move(id_to_resolved_callstack_),
                                   std::move(original_id_to_resolved_callstack_id_),
                                   std::move(function_address_to_sampled_callstack_ids_));
}

void SamplingDataPostProcessor::ResolveCallstacks(const CallstackData& callstack_data,
                                                  const CaptureData& capture_data,
                                                  const ModuleManager& module_manager) {
  callstack_data.ForEachUniqueCallstack([this, &capture_data, &module_manager](
                                            uint64_t callstack_id, const CallstackInfo& callstack) {
    // A "resolved callstack" is a callstack where every address is replaced by the start address of
    // the function (if known).
    std::vector<uint64_t> resolved_callstack_frames;

    for (uint64_t address : callstack.frames()) {
      if (!exact_address_to_function_address_.contains(address)) {
        MapAddressToFunctionAddress(address, capture_data, module_manager);
      }
      auto function_address_it = exact_address_to_function_address_.find(address);
      ORBIT_CHECK(function_address_it != exact_address_to_function_address_.end());
      resolved_callstack_frames.push_back(function_address_it->second);
    }

    if (callstack.type() == CallstackInfo::kComplete) {
      for (uint64_t function_address : resolved_callstack_frames) {
        // Create a new entry if it doesn't exist.
        auto it = function_address_to_sampled_callstack_ids_.try_emplace(function_address).first;
        it->second.insert(callstack_id);
      }
    } else {
      // For non-kComplete callstacks, only use the innermost frame for statistics.
      auto it = function_address_to_sampled_callstack_ids_.try_emplace(resolved_callstack_frames[0])
                    .first;
      it->second.insert(callstack_id);
    }

    CallstackInfo::CallstackType resolved_callstack_type = callstack.type();

    // Check if we already have this resolved callstack, and if not, create one.
    uint64_t resolved_callstack_id;
    auto it = resolved_callstack_to_id_.find(CallstackInfoAsPairWithLvalueRefToFrames{
        resolved_callstack_frames, resolved_callstack_type});
    if (it == resolved_callstack_to_id_.end()) {
      resolved_callstack_id = callstack_id;
      ORBIT_CHECK(!id_to_resolved_callstack_.contains(resolved_callstack_id));

      CallstackInfo resolved_callstack;
      *resolved_callstack.mutable_frames() = {resolved_callstack_frames.begin(),
                                              resolved_callstack_frames.end()};
      resolved_callstack.set_type(resolved_callstack_type);
      id_to_resolved_callstack_.insert_or_assign(resolved_callstack_id, resolved_callstack);

      resolved_callstack_to_id_.emplace(
          CallstackInfoAsClass{resolved_callstack_frames, resolved_callstack_type},
          resolved_callstack_id);
    } else {
      resolved_callstack_id = it->second;
    }

    original_id_to_resolved_callstack_id_[callstack_id] = resolved_callstack_id;
  });
}

void SamplingDataPostProcessor::MapAddressToFunctionAddress(uint64_t absolute_address,
                                                            const CaptureData& capture_data,
                                                            const ModuleManager& module_manager) {
  // SamplingDataPostProcessor relies heavily on the association between address and function
  // address held by exact_address_to_function_address_, otherwise each address is considered a
  // different function. We are storing this mapping for faster lookup.
  std::optional<uint64_t> absolute_function_address_option =
      orbit_client_data::FindFunctionAbsoluteAddressByInstructionAbsoluteAddress(
          module_manager, capture_data, absolute_address);
  uint64_t absolute_function_address = absolute_function_address_option.value_or(absolute_address);

  exact_address_to_function_address_[absolute_address] = absolute_function_address;
}

void SamplingDataPostProcessor::FillThreadSampleDataSampleReports(
    const CaptureData& capture_data, const ModuleManager& module_manager) {
  for (auto& data : thread_id_to_sample_data_) {
    ThreadSampleData* thread_sample_data = &data.second;
    std::vector<SampledFunction>* sampled_functions = &thread_sample_data->sampled_functions;

    for (auto sorted_it = thread_sample_data->sorted_count_to_resolved_address.rbegin();
         sorted_it != thread_sample_data->sorted_count_to_resolved_address.rend(); ++sorted_it) {
      uint32_t num_occurrences = sorted_it->first;
      uint64_t absolute_address = sorted_it->second;

      SampledFunction function;
      function.name = orbit_client_data::GetFunctionNameByAddress(module_manager, capture_data,
                                                                  absolute_address);

      function.inclusive = num_occurrences;
      function.inclusive_percent = 100.f * num_occurrences / thread_sample_data->samples_count;

      function.exclusive = 0;
      function.exclusive_percent = 0.f;

      if (auto it = thread_sample_data->resolved_address_to_exclusive_count.find(absolute_address);
          it != thread_sample_data->resolved_address_to_exclusive_count.end()) {
        function.exclusive = it->second;
        function.exclusive_percent = 100.f * it->second / thread_sample_data->samples_count;
      }

      function.unwind_errors = 0;
      function.unwind_errors_percent = 0.f;
      if (auto it = thread_sample_data->resolved_address_to_error_count.find(absolute_address);
          it != thread_sample_data->resolved_address_to_error_count.end()) {
        function.unwind_errors = it->second;
        function.unwind_errors_percent = 100.f * it->second / thread_sample_data->samples_count;
      }
      function.absolute_address = absolute_address;
      function.module_path =
          orbit_client_data::GetModulePathByAddress(module_manager, capture_data, absolute_address);

      sampled_functions->push_back(function);
    }
  }
}

}  // namespace

}  // namespace orbit_client_model
