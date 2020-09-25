// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_CORE_CAPTURE_DATA_H_
#define ORBIT_CORE_CAPTURE_DATA_H_

#include <memory>
#include <vector>

#include "CallstackData.h"
#include "OrbitClientData/ProcessData.h"
#include "SamplingProfiler.h"
#include "TracepointCustom.h"
#include "TracepointEventBuffer.h"
#include "TracepointInfoManager.h"
#include "absl/container/flat_hash_map.h"
#include "capture_data.pb.h"
#include "process.pb.h"

class CaptureData {
 public:
  explicit CaptureData(
      std::unique_ptr<ProcessData> process,
      absl::flat_hash_map<std::string, ModuleData*>&& module_map,
      absl::flat_hash_map<uint64_t, orbit_client_protos::FunctionInfo> selected_functions,
      TracepointInfoSet selected_tracepoints)
      : process_(std::move(process)),
        module_map_(std::move(module_map)),
        selected_functions_{std::move(selected_functions)},
        selected_tracepoints_{std::move(selected_tracepoints)},
        callstack_data_(std::make_unique<CallstackData>()),
        selection_callstack_data_(std::make_unique<CallstackData>()),
        tracepoint_info_manager_(std::make_unique<TracepointInfoManager>()),
        tracepoint_event_buffer_(std::make_unique<TracepointEventBuffer>()) {
    CHECK(process_ != nullptr);
  }

  explicit CaptureData()
      : callstack_data_(std::make_unique<CallstackData>()),
        selection_callstack_data_(std::make_unique<CallstackData>()),
        tracepoint_info_manager_(std::make_unique<TracepointInfoManager>()),
        tracepoint_event_buffer_(std::make_unique<TracepointEventBuffer>()){};

  // We can not copy the unique_ptr, so we can not copy this object.
  CaptureData& operator=(const CaptureData& other) = delete;
  CaptureData(const CaptureData& other) = delete;

  CaptureData(CaptureData&& other) = default;
  CaptureData& operator=(CaptureData&& other) = default;

  [[nodiscard]] const absl::flat_hash_map<uint64_t, orbit_client_protos::FunctionInfo>&
  selected_functions() const {
    return selected_functions_;
  }

  [[nodiscard]] const orbit_client_protos::FunctionInfo* GetSelectedFunction(
      uint64_t function_address) const;

  [[nodiscard]] int32_t process_id() const;

  [[nodiscard]] const std::string process_name() const;

  [[nodiscard]] const std::chrono::system_clock::time_point& capture_start_time() const {
    return capture_start_time_;
  }

  [[nodiscard]] const absl::flat_hash_map<uint64_t, orbit_client_protos::LinuxAddressInfo>&
  address_infos() const {
    return address_infos_;
  }

  [[nodiscard]] const orbit_client_protos::LinuxAddressInfo* GetAddressInfo(
      uint64_t absolute_address) const;

  void InsertAddressInfo(orbit_client_protos::LinuxAddressInfo address_info);

  [[nodiscard]] const std::string& GetFunctionNameByAddress(uint64_t absolute_address) const;
  [[nodiscard]] const std::string& GetModulePathByAddress(uint64_t absolute_address) const;

  [[nodiscard]] const orbit_client_protos::FunctionInfo* FindFunctionByAddress(
      uint64_t absolute_address, bool is_exact) const;
  [[nodiscard]] ModuleData* FindModuleByAddress(uint64_t absolute_address) const;

  static const std::string kUnknownFunctionOrModuleName;

  [[nodiscard]] const absl::flat_hash_map<int32_t, std::string>& thread_names() const {
    return thread_names_;
  }

  [[nodiscard]] const std::string& GetThreadName(int32_t thread_id) const {
    static const std::string kEmptyString;
    auto it = thread_names_.find(thread_id);
    return it != thread_names_.end() ? it->second : kEmptyString;
  }

  void AddOrAssignThreadName(int32_t thread_id, std::string thread_name) {
    thread_names_.insert_or_assign(thread_id, std::move(thread_name));
  }

  [[nodiscard]] const absl::flat_hash_map<uint64_t, orbit_client_protos::FunctionStats>&
  functions_stats() const {
    return functions_stats_;
  }

  [[nodiscard]] const orbit_client_protos::FunctionStats& GetFunctionStatsOrDefault(
      const orbit_client_protos::FunctionInfo& function) const;

  void UpdateFunctionStats(const orbit_client_protos::FunctionInfo& function,
                           uint64_t elapsed_nanos);

  [[nodiscard]] const CallstackData* GetCallstackData() const { return callstack_data_.get(); };

  [[nodiscard]] orbit_grpc_protos::TracepointInfo GetTracepointInfo(uint64_t key) const {
    return tracepoint_info_manager_->Get(key);
  }

  [[nodiscard]] const TracepointInfoManager* GetTracepointInfoManager() const {
    return tracepoint_info_manager_.get();
  };

  [[nodiscard]] const TracepointEventBuffer* GetTracepointEventBuffer() const {
    return tracepoint_event_buffer_.get();
  }

  void ForEachTracepointEventPerThread(
      int32_t thread_id,
      const std::function<void(
          const std::map<uint64_t, orbit_client_protos::TracepointEventInfo>&)>& action) const {
    return tracepoint_event_buffer_->ForEachTracepointEventPerThread(thread_id, action);
  }

  [[nodiscard]] const std::map<uint64_t, orbit_client_protos::TracepointEventInfo>&
  GetTracepointsOfThread(int32_t thread_id) const {
    return tracepoint_event_buffer_->GetTracepointsOfThread(thread_id);
  }

  void AddUniqueCallStack(CallStack call_stack) {
    callstack_data_->AddUniqueCallStack(std::move(call_stack));
  }

  void AddCallstackEvent(orbit_client_protos::CallstackEvent callstack_event) {
    callstack_data_->AddCallstackEvent(std::move(callstack_event));
  }

  void FilterBrokenCallstacks() { callstack_data_->FilterCallstackEventsBasedOnMajorityStart(); }

  void AddUniqueTracepointEventInfo(uint64_t key,
                                    orbit_grpc_protos::TracepointInfo tracepoint_info) {
    tracepoint_info_manager_->AddUniqueTracepointEventInfo(key, std::move(tracepoint_info));
  }

  void AddTracepointEventAndMapToThreads(uint64_t time, uint64_t tracepoint_hash,
                                         int32_t process_id, int32_t thread_id, int32_t cpu,
                                         bool is_same_pid_as_target) {
    tracepoint_event_buffer_->AddTracepointEventAndMapToThreads(
        time, tracepoint_hash, process_id, thread_id, cpu, is_same_pid_as_target);
  }

  [[nodiscard]] const CallstackData* GetSelectionCallstackData() const {
    return selection_callstack_data_.get();
  };

  void set_selection_callstack_data(std::unique_ptr<CallstackData> selection_callstack_data) {
    selection_callstack_data_ = std::move(selection_callstack_data);
  }

  [[nodiscard]] const ProcessData* process() const { return process_.get(); }

  [[nodiscard]] const SamplingProfiler& sampling_profiler() const { return sampling_profiler_; }

  void set_sampling_profiler(SamplingProfiler sampling_profiler) {
    sampling_profiler_ = std::move(sampling_profiler);
  }

 private:
  std::unique_ptr<ProcessData> process_;
  absl::flat_hash_map<std::string, ModuleData*> module_map_;
  absl::flat_hash_map<uint64_t, orbit_client_protos::FunctionInfo> selected_functions_;

  TracepointInfoSet selected_tracepoints_;
  // std::unique_ptr<> allows to move and copy CallstackData easier
  // (as CallstackData stores an absl::Mutex inside)
  std::unique_ptr<CallstackData> callstack_data_;
  // selection_callstack_data_ is subset of callstack_data_
  std::unique_ptr<CallstackData> selection_callstack_data_;

  std::unique_ptr<TracepointInfoManager> tracepoint_info_manager_;
  std::unique_ptr<TracepointEventBuffer> tracepoint_event_buffer_;

  SamplingProfiler sampling_profiler_;

  absl::flat_hash_map<uint64_t, orbit_client_protos::LinuxAddressInfo> address_infos_;

  absl::flat_hash_map<uint64_t, orbit_client_protos::FunctionStats> functions_stats_;

  absl::flat_hash_map<int32_t, std::string> thread_names_;

  std::chrono::system_clock::time_point capture_start_time_ = std::chrono::system_clock::now();
};

#endif  // ORBIT_CORE_CAPTURE_DATA_H_
