// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "OrbitClientModel/SamplingProfiler.h"

#include "OrbitClientModel/CaptureData.h"

using orbit_client_protos::CallstackEvent;
using orbit_client_protos::FunctionInfo;
using orbit_client_protos::LinuxAddressInfo;

namespace {

std::multimap<int, CallstackID> SortCallstacks(const ThreadSampleData& data,
                                               const std::set<CallstackID>& callstacks) {
  std::multimap<int, CallstackID> sorted_callstacks;
  for (CallstackID id : callstacks) {
    auto it = data.callstack_count.find(id);
    if (it != data.callstack_count.end()) {
      int count = it->second;
      sorted_callstacks.insert(std::make_pair(count, id));
    }
  }

  return sorted_callstacks;
}

}  // namespace

uint32_t ThreadSampleData::GetCountForAddress(uint64_t address) const {
  auto res = raw_address_count.find(address);
  if (res == raw_address_count.end()) {
    return 0;
  }
  return (*res).second;
}

std::multimap<int, CallstackID> SamplingProfiler::GetCallstacksFromAddress(
    uint64_t address, ThreadID thread_id) const {
  const auto& callstacks_it = function_address_to_callstack_.find(address);
  const auto& sample_data_it = thread_id_to_sample_data_.find(thread_id);
  if (callstacks_it == function_address_to_callstack_.end() ||
      sample_data_it == thread_id_to_sample_data_.end()) {
    return std::multimap<int, CallstackID>();
  }
  return SortCallstacks(sample_data_it->second, callstacks_it->second);
}

const CallStack& SamplingProfiler::GetResolvedCallstack(CallstackID raw_callstack_id) const {
  auto resolved_callstack_id_it = original_to_resolved_callstack_.find(raw_callstack_id);
  CHECK(resolved_callstack_id_it != original_to_resolved_callstack_.end());
  auto resolved_callstack_it = unique_resolved_callstacks_.find(resolved_callstack_id_it->second);
  CHECK(resolved_callstack_it != unique_resolved_callstacks_.end());
  return *resolved_callstack_it->second;
}

std::unique_ptr<SortedCallstackReport> SamplingProfiler::GetSortedCallstackReportFromAddress(
    uint64_t address, ThreadID thread_id) const {
  std::unique_ptr<SortedCallstackReport> report = std::make_unique<SortedCallstackReport>();
  std::multimap<int, CallstackID> multi_map = GetCallstacksFromAddress(address, thread_id);
  size_t unique_callstacks_count = multi_map.size();
  report->callstacks_count.resize(unique_callstacks_count);
  size_t index = unique_callstacks_count;

  for (const auto& pair : multi_map) {
    CallstackCount* callstack = &report->callstacks_count[--index];
    callstack->count = pair.first;
    callstack->callstack_id = pair.second;
    report->callstacks_total_count += callstack->count;
  }

  return report;
}

const int32_t SamplingProfiler::kAllThreadsFakeTid = -1;

void SamplingProfiler::SortByThreadUsage() {
  sorted_thread_sample_data_.reserve(thread_id_to_sample_data_.size());

  for (auto& pair : thread_id_to_sample_data_) {
    ThreadSampleData& data = pair.second;
    data.thread_id = pair.first;
    sorted_thread_sample_data_.push_back(data);
  }

  sort(sorted_thread_sample_data_.begin(), sorted_thread_sample_data_.end(),
       [](const ThreadSampleData& a, const ThreadSampleData& b) {
         return a.samples_count > b.samples_count;
       });
}

void SamplingProfiler::ProcessSamples(const CallstackData& callstack_data,
                                      const CaptureData& capture_data, bool generate_summary) {
  // Unique call stacks and per thread data
  callstack_data.ForEachCallstackEvent(
      [this, &callstack_data, generate_summary](const CallstackEvent& event) {
        CHECK(callstack_data.HasCallStack(event.callstack_hash()));

        ThreadSampleData* thread_sample_data = &thread_id_to_sample_data_[event.thread_id()];
        thread_sample_data->samples_count++;
        thread_sample_data->callstack_count[event.callstack_hash()]++;
        callstack_data.ForEachFrameInCallstack(event.callstack_hash(),
                                               [&thread_sample_data](uint64_t address) {
                                                 thread_sample_data->raw_address_count[address]++;
                                               });

        if (generate_summary) {
          ThreadSampleData* all_thread_sample_data = &thread_id_to_sample_data_[kAllThreadsFakeTid];
          all_thread_sample_data->samples_count++;
          all_thread_sample_data->callstack_count[event.callstack_hash()]++;
          callstack_data.ForEachFrameInCallstack(
              event.callstack_hash(), [&all_thread_sample_data](uint64_t address) {
                all_thread_sample_data->raw_address_count[address]++;
              });
        }
      });

  ResolveCallstacks(callstack_data, capture_data);

  for (auto& sample_data_it : thread_id_to_sample_data_) {
    ThreadSampleData* thread_sample_data = &sample_data_it.second;

    // Address count per sample per thread
    for (const auto& callstack_count_it : thread_sample_data->callstack_count) {
      const CallstackID callstack_id = callstack_count_it.first;
      const uint32_t callstack_count = callstack_count_it.second;

      CallstackID resolved_callstack_id = original_to_resolved_callstack_[callstack_id];
      std::shared_ptr<CallStack>& resolved_callstack =
          unique_resolved_callstacks_[resolved_callstack_id];

      // exclusive stat
      thread_sample_data->exclusive_count[resolved_callstack->GetFrame(0)] += callstack_count;

      std::set<uint64_t> unique_addresses;
      for (uint64_t address : resolved_callstack->GetFrames()) {
        unique_addresses.insert(address);
      }

      for (uint64_t address : unique_addresses) {
        thread_sample_data->address_count[address] += callstack_count;
      }
    }

    // sort thread addresses by count
    for (const auto& address_count_it : thread_sample_data->address_count) {
      const uint64_t address = address_count_it.first;
      const uint32_t count = address_count_it.second;
      thread_sample_data->address_count_sorted.insert(std::make_pair(count, address));
    }
  }

  FillThreadSampleDataSampleReports(capture_data);

  SortByThreadUsage();
}

void SamplingProfiler::ResolveCallstacks(const CallstackData& callstack_data,
                                         const CaptureData& capture_data) {
  callstack_data.ForEachUniqueCallstack([this, &capture_data](const CallStack& call_stack) {
    // A "resolved callstack" is a callstack where every address is replaced
    // by the start address of the function (if known).
    std::vector<uint64_t> resolved_callstack_data;

    for (uint64_t address : call_stack.GetFrames()) {
      if (exact_address_to_function_address_.find(address) ==
          exact_address_to_function_address_.end()) {
        MapAddressToFunctionAddress(address, capture_data);
      }
      uint64_t function_address = exact_address_to_function_address_.at(address);

      resolved_callstack_data.push_back(function_address);
      function_address_to_callstack_[function_address].insert(call_stack.GetHash());
    }

    CallStack resolved_callstack(std::move(resolved_callstack_data));

    CallstackID resolved_callstack_id = resolved_callstack.GetHash();
    if (unique_resolved_callstacks_.find(resolved_callstack_id) ==
        unique_resolved_callstacks_.end()) {
      unique_resolved_callstacks_[resolved_callstack_id] =
          std::make_shared<CallStack>(resolved_callstack);
    }

    original_to_resolved_callstack_[call_stack.GetHash()] = resolved_callstack_id;
  });
}

const ThreadSampleData* SamplingProfiler::GetSummary() const {
  auto summary_it = thread_id_to_sample_data_.find(kAllThreadsFakeTid);
  if (summary_it == thread_id_to_sample_data_.end()) {
    return nullptr;
  }
  return &(summary_it->second);
}

uint32_t SamplingProfiler::GetCountOfFunction(uint64_t function_address) const {
  auto addresses_of_functions_itr = function_address_to_exact_addresses_.find(function_address);
  if (addresses_of_functions_itr == function_address_to_exact_addresses_.end()) {
    return 0;
  }
  uint32_t result = 0;
  const ThreadSampleData* summary = GetSummary();
  if (summary == nullptr) {
    return 0;
  }
  const auto& function_addresses = addresses_of_functions_itr->second;
  for (uint64_t address : function_addresses) {
    auto count_itr = summary->raw_address_count.find(address);
    if (count_itr != summary->raw_address_count.end()) {
      result += count_itr->second;
    }
  }
  return result;
}

void SamplingProfiler::MapAddressToFunctionAddress(uint64_t absolute_address,
                                                   const CaptureData& capture_data) {
  const LinuxAddressInfo* address_info = capture_data.GetAddressInfo(absolute_address);
  const FunctionInfo* function = capture_data.FindFunctionByAddress(absolute_address, false);

  // Find the start address of the function this address falls inside.
  // Use the Function returned by Process::GetFunctionFromAddress, and
  // when this fails (e.g., the module containing the function has not
  // been loaded) use (for now) the LinuxAddressInfo that is collected
  // for every address in a callstack. SamplingProfiler relies heavily
  // on the association between address and function address held by
  // exact_address_to_function_address_, otherwise each address is
  // considered a different function.
  uint64_t absolute_function_address;
  if (function != nullptr) {
    absolute_function_address = capture_data.GetAbsoluteAddress(*function);
  } else if (address_info != nullptr) {
    absolute_function_address = absolute_address - address_info->offset_in_function();
  } else {
    absolute_function_address = absolute_address;
  }

  exact_address_to_function_address_[absolute_address] = absolute_function_address;
  function_address_to_exact_addresses_[absolute_function_address].insert(absolute_address);
}

void SamplingProfiler::FillThreadSampleDataSampleReports(const CaptureData& capture_data) {
  for (auto& data : thread_id_to_sample_data_) {
    ThreadSampleData* thread_sample_data = &data.second;
    std::vector<SampledFunction>* sampled_functions = &thread_sample_data->sampled_function;

    for (auto sorted_it = thread_sample_data->address_count_sorted.rbegin();
         sorted_it != thread_sample_data->address_count_sorted.rend(); ++sorted_it) {
      uint32_t num_occurences = sorted_it->first;
      uint64_t absolute_address = sorted_it->second;
      float inclusive_percent = 100.f * num_occurences / thread_sample_data->samples_count;

      SampledFunction function;
      function.name = capture_data.GetFunctionNameByAddress(absolute_address);
      function.inclusive = inclusive_percent;
      function.exclusive = 0.f;
      auto it = thread_sample_data->exclusive_count.find(absolute_address);
      if (it != thread_sample_data->exclusive_count.end()) {
        function.exclusive = 100.f * it->second / thread_sample_data->samples_count;
      }
      function.absolute_address = absolute_address;
      function.module_path = capture_data.GetModulePathByAddress(absolute_address);

      const FunctionInfo* function_info =
          capture_data.FindFunctionByAddress(absolute_address, false);
      if (function_info != nullptr) {
        function.line = function_info->line();
        function.file = function_info->file();
      }

      sampled_functions->push_back(function);
    }
  }
}
