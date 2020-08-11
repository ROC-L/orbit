// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_CORE_SAMPLING_PROFILER_H_
#define ORBIT_CORE_SAMPLING_PROFILER_H_

#include <utility>

#include "BlockChain.h"
#include "Callstack.h"
#include "Capture.h"
#include "EventBuffer.h"
#include "Pdb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "capture_data.pb.h"

class Process;

struct SampledFunction {
  SampledFunction() = default;

  std::string name;
  std::string module;
  std::string file;
  float exclusive = 0;
  float inclusive = 0;
  int line = 0;
  uint64_t address = 0;
  orbit_client_protos::FunctionInfo* function = nullptr;
};

struct ThreadSampleData {
  ThreadSampleData() { thread_usage.push_back(0); }
  [[nodiscard]] uint32_t GetCountForAddress(uint64_t address) const;
  absl::flat_hash_map<CallstackID, uint32_t> callstack_count;
  absl::flat_hash_map<uint64_t, uint32_t> address_count;
  absl::flat_hash_map<uint64_t, uint32_t> raw_address_count;
  absl::flat_hash_map<uint64_t, uint32_t> exclusive_count;
  std::multimap<uint32_t, uint64_t> address_count_sorted;
  uint32_t samples_count = 0;
  std::vector<SampledFunction> sampled_function;
  std::vector<float> thread_usage;
  float average_thread_usage = 0;
  ThreadID thread_id = 0;
};

struct CallstackCount {
  CallstackCount() = default;

  int count = 0;
  CallstackID callstack_id = 0;
};

struct SortedCallstackReport {
  SortedCallstackReport() = default;
  int callstacks_total_count = 0;
  std::vector<CallstackCount> callstacks_count;
};

class SamplingProfiler {
 public:
  explicit SamplingProfiler(std::shared_ptr<Process> a_Process)
      : process_{std::move(a_Process)} {}
  SamplingProfiler() : SamplingProfiler{std::make_shared<Process>()} {}

  int GetNumSamples() const { return samples_count_; }

  void AddCallStack(orbit_client_protos::CallstackEvent&& callstack_event);
  void AddUniqueCallStack(CallStack call_stack);

  std::shared_ptr<CallStack> GetCallStack(CallstackID callstack_id) {
    absl::MutexLock lock(&unique_callstacks_mutex_);
    return unique_callstacks_.at(callstack_id);
  }
  bool HasCallStack(CallstackID callstack_id) {
    absl::MutexLock lock(&unique_callstacks_mutex_);
    return unique_callstacks_.count(callstack_id) > 0;
  }

  const CallStack& GetResolvedCallstack(CallstackID raw_callstack_id) const;

  std::multimap<int, CallstackID> GetCallstacksFromAddress(
      uint64_t address, ThreadID thread_id, int* callstacks_count);
  std::shared_ptr<SortedCallstackReport> GetSortedCallstacksFromAddress(
      uint64_t address, ThreadID thread_id);

  BlockChain<orbit_client_protos::CallstackEvent, 16 * 1024>* GetCallstacks() {
    return &callstack_events_;
  }

  void ForEachUniqueCallstack(
      const std::function<void(const CallStack&)>& action) {
    absl::MutexLock lock(&unique_callstacks_mutex_);
    for (const auto& it : unique_callstacks_) {
      action(*it.second);
    }
  }

  const std::vector<ThreadSampleData*>& GetThreadSampleData() const {
    return sorted_thread_sample_data;
  }
  const ThreadSampleData* GetThreadSampleDataByThreadId(
      int32_t thread_id) const {
    auto it = thread_id_to_sample_data_.find(thread_id);
    if (it == thread_id_to_sample_data_.end()) {
      return nullptr;
    }

    return &it->second;
  }

  void SetGenerateSummary(bool a_Value) { generate_summary_ = a_Value; }
  bool GetGenerateSummary() const { return generate_summary_; }
  void SortByThreadUsage();
  void ProcessSamples();
  void UpdateAddressInfo(uint64_t address);
  [[nodiscard]] const ThreadSampleData* GetSummary() const;
  [[nodiscard]] uint32_t GetCountOfFunction(uint64_t function_address) const;

  void ClearCallstacks() {
    absl::MutexLock lock(&unique_callstacks_mutex_);
    unique_callstacks_.clear();
    callstack_events_.clear();
  }

  [[nodiscard]] const std::string& GetFunctionNameByAddress(
      uint64_t address) const;
  [[nodiscard]] const std::string& GetModuleNameByAddress(
      uint64_t address) const;

  static const int32_t kAllThreadsFakeTid;
  static const std::string kUnknownFunctionOrModuleName;

 protected:
  void ResolveCallstacks();
  void FillThreadSampleDataSampleReports();

 protected:
  std::shared_ptr<Process> process_;
  bool generate_summary_ = true;
  int samples_count_ = 0;

  // Filled before ProcessSamples by AddCallstack, AddHashedCallstack.
  BlockChain<orbit_client_protos::CallstackEvent, 16 * 1024> callstack_events_;
  absl::Mutex unique_callstacks_mutex_;
  absl::flat_hash_map<CallstackID, std::shared_ptr<CallStack>>
      unique_callstacks_;

  // Filled by ProcessSamples.
  absl::flat_hash_map<ThreadID, ThreadSampleData> thread_id_to_sample_data_;
  absl::flat_hash_map<CallstackID, std::shared_ptr<CallStack>>
      unique_resolved_callstacks_;
  absl::flat_hash_map<CallstackID, CallstackID> original_to_resolved_callstack_;
  absl::flat_hash_map<uint64_t, std::set<CallstackID>>
      function_address_to_callstack_;
  absl::flat_hash_map<uint64_t, uint64_t> exact_address_to_function_address_;
  absl::flat_hash_map<uint64_t, absl::flat_hash_set<uint64_t>>
      function_address_to_exact_addresses_;
  std::vector<ThreadSampleData*> sorted_thread_sample_data;

  absl::flat_hash_map<uint64_t, std::string> address_to_function_name_;
  absl::flat_hash_map<uint64_t, std::string> address_to_module_name_;
};

#endif  // ORBIT_CORE_SAMPLING_PROFILER_H_
