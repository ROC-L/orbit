// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SamplingProfiler.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "Capture.h"
#include "FunctionUtils.h"
#include "Log.h"
#include "OrbitModule.h"
#include "Path.h"

using orbit_client_protos::CallstackEvent;
using orbit_client_protos::FunctionInfo;
using orbit_client_protos::LinuxAddressInfo;

namespace {

std::multimap<int, CallstackID> SortCallstacks(
    const ThreadSampleData& data, const std::set<CallstackID>& callstacks,
    int* callstacks_total_count) {
  std::multimap<int, CallstackID> sorted_callstacks;
  int callstacks_count = 0;
  for (CallstackID id : callstacks) {
    auto it = data.callstack_count.find(id);
    if (it != data.callstack_count.end()) {
      int count = it->second;
      sorted_callstacks.insert(std::make_pair(count, id));
      callstacks_count += count;
    }
  }

  *callstacks_total_count = callstacks_count;
  return sorted_callstacks;
}

void ComputeAverageThreadUsage(ThreadSampleData* data) {
  data->average_thread_usage = 0.f;

  if (!data->thread_usage.empty()) {
    for (float thread_usage : data->thread_usage) {
      data->average_thread_usage += thread_usage;
    }

    data->average_thread_usage /= data->thread_usage.size();
  }
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
    uint64_t address, ThreadID thread_id, int* callstacks_count) {
  const std::set<CallstackID>& callstacks =
      function_address_to_callstack_[address];
  return SortCallstacks(thread_id_to_sample_data_[thread_id], callstacks,
                        callstacks_count);
}

void SamplingProfiler::AddCallStack(CallstackEvent&& callstack_event) {
  CallstackID hash = callstack_event.callstack_hash();
  if (!HasCallStack(hash)) {
    std::shared_ptr<CallStack> callstack =
        Capture::GSamplingProfiler->GetCallStack(hash);
    AddUniqueCallStack(*callstack);
  }

  callstack_events_.push_back(std::move(callstack_event));
}

void SamplingProfiler::AddUniqueCallStack(CallStack call_stack) {
  absl::MutexLock lock(&unique_callstacks_mutex_);
  auto key = call_stack.Hash();
  unique_callstacks_[key] = std::make_shared<CallStack>(std::move(call_stack));
}

const CallStack& SamplingProfiler::GetResolvedCallstack(
    CallstackID raw_callstack_id) const {
  auto resolved_callstack_id_it =
      original_to_resolved_callstack_.find(raw_callstack_id);
  CHECK(resolved_callstack_id_it != original_to_resolved_callstack_.end());
  auto resolved_callstack_it =
      unique_resolved_callstacks_.find(resolved_callstack_id_it->second);
  CHECK(resolved_callstack_it != unique_resolved_callstacks_.end());
  return *resolved_callstack_it->second;
}

std::shared_ptr<SortedCallstackReport>
SamplingProfiler::GetSortedCallstacksFromAddress(uint64_t address,
                                                 ThreadID thread_id) {
  std::shared_ptr<SortedCallstackReport> report =
      std::make_shared<SortedCallstackReport>();
  std::multimap<int, CallstackID> multi_map = GetCallstacksFromAddress(
      address, thread_id, &report->callstacks_total_count);
  size_t unique_callstacks_count = multi_map.size();
  report->callstacks_count.resize(unique_callstacks_count);
  size_t index = unique_callstacks_count;

  for (const auto& pair : multi_map) {
    CallstackCount* callstack = &report->callstacks_count[--index];
    callstack->count = pair.first;
    callstack->callstack_id = pair.second;
  }

  return report;
}

const int32_t SamplingProfiler::kAllThreadsFakeTid = 0;

void SamplingProfiler::SortByThreadUsage() {
  sorted_thread_sample_data.clear();
  sorted_thread_sample_data.reserve(thread_id_to_sample_data_.size());

  // "All"
  thread_id_to_sample_data_[kAllThreadsFakeTid].average_thread_usage = 100.f;

  for (auto& pair : thread_id_to_sample_data_) {
    ThreadSampleData* data = &pair.second;
    data->thread_id = pair.first;
    sorted_thread_sample_data.push_back(data);
  }

  sort(sorted_thread_sample_data.begin(), sorted_thread_sample_data.end(),
       [](const ThreadSampleData* a, const ThreadSampleData* b) {
         return a->average_thread_usage > b->average_thread_usage;
       });
}

void SamplingProfiler::ProcessSamples() {
  // Clear the result of a previous call to ProcessSamples.
  thread_id_to_sample_data_.clear();
  unique_resolved_callstacks_.clear();
  original_to_resolved_callstack_.clear();
  function_address_to_callstack_.clear();
  exact_address_to_function_address_.clear();
  function_address_to_exact_addresses_.clear();
  sorted_thread_sample_data.clear();
  address_to_function_name_.clear();
  address_to_module_name_.clear();

  // Unique call stacks and per thread data
  for (const CallstackEvent& callstack : callstack_events_) {
    if (!HasCallStack(callstack.callstack_hash())) {
      ERROR("Processed unknown callstack!");
      continue;
    }

    ThreadSampleData* thread_sample_data =
        &thread_id_to_sample_data_[callstack.thread_id()];
    thread_sample_data->samples_count++;
    thread_sample_data->callstack_count[callstack.callstack_hash()]++;

    absl::MutexLock lock(&unique_callstacks_mutex_);
    for (uint64_t address :
         unique_callstacks_[callstack.callstack_hash()]->m_Data) {
      thread_sample_data->raw_address_count[address]++;
    }

    if (generate_summary_) {
      ThreadSampleData* all_thread_sample_data =
          &thread_id_to_sample_data_[kAllThreadsFakeTid];
      all_thread_sample_data->samples_count++;
      all_thread_sample_data->callstack_count[callstack.callstack_hash()]++;
      for (uint64_t address :
           unique_callstacks_[callstack.callstack_hash()]->m_Data) {
        all_thread_sample_data->raw_address_count[address]++;
      }
    }
  }

  ResolveCallstacks();

  for (auto& sample_data_it : thread_id_to_sample_data_) {
    ThreadSampleData* thread_sample_data = &sample_data_it.second;

    ComputeAverageThreadUsage(thread_sample_data);

    // Address count per sample per thread
    for (const auto& callstack_count_it : thread_sample_data->callstack_count) {
      const CallstackID callstack_id = callstack_count_it.first;
      const uint32_t callstack_count = callstack_count_it.second;

      CallstackID resolved_callstack_id =
          original_to_resolved_callstack_[callstack_id];
      std::shared_ptr<CallStack>& resolved_callstack =
          unique_resolved_callstacks_[resolved_callstack_id];

      // exclusive stat
      thread_sample_data->exclusive_count[resolved_callstack->m_Data[0]] +=
          callstack_count;

      std::set<uint64_t> unique_addresses;
      for (uint32_t i = 0; i < resolved_callstack->m_Data.size(); ++i) {
        unique_addresses.insert(resolved_callstack->m_Data[i]);
      }

      for (uint64_t address : unique_addresses) {
        thread_sample_data->address_count[address] += callstack_count;
      }
    }

    // sort thread addresses by count
    for (const auto& address_count_it : thread_sample_data->address_count) {
      const uint64_t address = address_count_it.first;
      const uint32_t count = address_count_it.second;
      thread_sample_data->address_count_sorted.insert(
          std::make_pair(count, address));
    }
  }

  SortByThreadUsage();

  FillThreadSampleDataSampleReports();

  samples_count_ = callstack_events_.size();

  // Don't clear m_Callstacks, so that ProcessSamples can be called again, e.g.
  // when new callstacks have been added or after a module has been loaded.
}

void SamplingProfiler::ResolveCallstacks() {
  absl::MutexLock lock(&unique_callstacks_mutex_);

  for (const auto& it : unique_callstacks_) {
    CallstackID raw_callstack_id = it.first;
    const std::shared_ptr<CallStack> callstack = it.second;
    // A "resolved callstack" is a callstack where every address is replaced by
    // the start address of the function (if known).
    CallStack resolved_callstack = *callstack;

    for (uint32_t i = 0; i < callstack->m_Data.size(); ++i) {
      uint64_t address = callstack->m_Data[i];

      if (exact_address_to_function_address_.find(address) ==
          exact_address_to_function_address_.end()) {
        UpdateAddressInfo(address);
      }

      auto address_it = exact_address_to_function_address_.find(address);
      if (address_it != exact_address_to_function_address_.end()) {
        const uint64_t& function_address = address_it->second;
        resolved_callstack.m_Data[i] = function_address;
        function_address_to_callstack_[function_address].insert(
            raw_callstack_id);
      }
    }

    CallstackID resolved_callstack_id = resolved_callstack.Hash();
    if (unique_resolved_callstacks_.find(resolved_callstack_id) ==
        unique_resolved_callstacks_.end()) {
      unique_resolved_callstacks_[resolved_callstack_id] =
          std::make_shared<CallStack>(resolved_callstack);
    }

    original_to_resolved_callstack_[raw_callstack_id] = resolved_callstack_id;
  }
}

const ThreadSampleData* SamplingProfiler::GetSummary() const {
  auto summary_it = thread_id_to_sample_data_.find(kAllThreadsFakeTid);
  if (summary_it == thread_id_to_sample_data_.end()) {
    return nullptr;
  }
  return &(summary_it->second);
}

uint32_t SamplingProfiler::GetCountOfFunction(uint64_t function_address) const {
  auto addresses_of_functions_itr =
      function_address_to_exact_addresses_.find(function_address);
  if (addresses_of_functions_itr ==
      function_address_to_exact_addresses_.end()) {
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

const std::string SamplingProfiler::kUnknownFunctionOrModuleName = "???";

void SamplingProfiler::UpdateAddressInfo(uint64_t address) {
  LinuxAddressInfo* address_info =
      Capture::capture_data_.GetAddressInfo(address);
  FunctionInfo* function = process_->GetFunctionFromAddress(address, false);

  // Find the start address of the function this address falls inside.
  // Use the Function returned by Process::GetFunctionFromAddress, and
  // when this fails (e.g., the module containing the function has not
  // been loaded) use (for now) the LinuxAddressInfo that is collected
  // for every address in a callstack. SamplingProfiler relies heavily
  // on the association between address and function address held by
  // m_ExactAddressToFunctionAddress, otherwise each address is
  // considered a different function.
  uint64_t function_address;
  std::string function_name = kUnknownFunctionOrModuleName;
  if (function != nullptr) {
    function_address = FunctionUtils::GetAbsoluteAddress(*function);
    function_name = FunctionUtils::GetDisplayName(*function);
  } else if (address_info != nullptr) {
    function_address = address - address_info->offset_in_function();
    if (!address_info->function_name().empty()) {
      function_name = address_info->function_name();
    }
  } else {
    function_address = address;
  }

  if (function != nullptr && address_info != nullptr) {
    address_info->set_function_name(FunctionUtils::GetDisplayName(*function));
  }

  exact_address_to_function_address_[address] = function_address;
  function_address_to_exact_addresses_[function_address].insert(address);

  address_to_function_name_[address] = function_name;
  address_to_function_name_[function_address] = function_name;

  std::string module_name = kUnknownFunctionOrModuleName;
  std::shared_ptr<Module> module = process_->GetModuleFromAddress(address);
  if (module != nullptr) {
    module_name = module->m_Name;
  } else if (address_info != nullptr) {
    module_name = Path::GetFileName(address_info->module_name());
  }
  address_to_module_name_[address] = module_name;
  address_to_module_name_[function_address] = module_name;
}

void SamplingProfiler::FillThreadSampleDataSampleReports() {
  for (auto& data : thread_id_to_sample_data_) {
    ThreadID thread_id = data.first;
    ThreadSampleData* thread_sample_data = &data.second;
    std::vector<SampledFunction>* sampled_functions =
        &thread_sample_data->sampled_function;

    ORBIT_LOGV(thread_id);
    ORBIT_LOGV(thread_sample_data->samples_count);

    for (auto sortedIt = thread_sample_data->address_count_sorted.rbegin();
         sortedIt != thread_sample_data->address_count_sorted.rend();
         ++sortedIt) {
      uint32_t numOccurences = sortedIt->first;
      uint64_t address = sortedIt->second;
      float inclusive_percent =
          100.f * numOccurences / thread_sample_data->samples_count;

      SampledFunction function;
      // GAddressToFunctionName and GAddressToModuleName should be filled in
      // UpdateAddressInfo()
      CHECK(address_to_function_name_.count(address) > 0);
      function.name = address_to_function_name_.at(address);
      function.inclusive = inclusive_percent;
      function.exclusive = 0.f;
      auto it = thread_sample_data->exclusive_count.find(address);
      if (it != thread_sample_data->exclusive_count.end()) {
        function.exclusive =
            100.f * it->second / thread_sample_data->samples_count;
      }
      function.address = address;
      CHECK(address_to_module_name_.count(address) > 0);
      function.module = address_to_module_name_.at(address);

      sampled_functions->push_back(function);
    }
  }
}

const std::string& SamplingProfiler::GetFunctionNameByAddress(
    uint64_t address) const {
  auto it = address_to_function_name_.find(address);
  if (it != address_to_function_name_.end()) {
    return it->second;
  }
  return kUnknownFunctionOrModuleName;
}

const std::string& SamplingProfiler::GetModuleNameByAddress(
    uint64_t address) const {
  auto it = address_to_module_name_.find(address);
  if (it != address_to_module_name_.end()) {
    return it->second;
  }
  return kUnknownFunctionOrModuleName;
}
