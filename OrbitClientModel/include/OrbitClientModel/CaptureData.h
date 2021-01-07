// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_CLIENT_MODEL_CAPTURE_DATA_H_
#define ORBIT_CLIENT_MODEL_CAPTURE_DATA_H_

#include <memory>
#include <vector>

#include "OrbitBase/Logging.h"
#include "OrbitClientData/CallstackData.h"
#include "OrbitClientData/FunctionInfoSet.h"
#include "OrbitClientData/ModuleManager.h"
#include "OrbitClientData/PostProcessedSamplingData.h"
#include "OrbitClientData/ProcessData.h"
#include "OrbitClientData/TracepointCustom.h"
#include "OrbitClientData/TracepointData.h"
#include "OrbitClientData/UserDefinedCaptureData.h"
#include "absl/container/flat_hash_map.h"
#include "capture_data.pb.h"
#include "process.pb.h"

class CaptureData {
 public:
  explicit CaptureData(
      ProcessData&& process, orbit_client_data::ModuleManager* module_manager,
      absl::flat_hash_map<uint64_t, orbit_client_protos::FunctionInfo> selected_functions,
      TracepointInfoSet selected_tracepoints, UserDefinedCaptureData user_defined_capture_data)
      : process_(std::move(process)),
        module_manager_(module_manager),
        selected_functions_{std::move(selected_functions)},
        selected_tracepoints_{std::move(selected_tracepoints)},
        callstack_data_(std::make_unique<CallstackData>()),
        selection_callstack_data_(std::make_unique<CallstackData>()),
        tracepoint_data_(std::make_unique<TracepointData>()),
        user_defined_capture_data_(std::move(user_defined_capture_data)) {}

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

  [[nodiscard]] std::string process_name() const;

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
  [[nodiscard]] std::optional<uint64_t> FindFunctionAbsoluteAddressByAddress(
      uint64_t absolute_address) const;
  [[nodiscard]] const std::string& GetModulePathByAddress(uint64_t absolute_address) const;
  [[nodiscard]] const ModuleData* GetModuleByPath(const std::string& module_path) const {
    return module_manager_->GetModuleByPath(module_path);
  }

  [[nodiscard]] const orbit_client_protos::FunctionInfo* FindFunctionByAddress(
      uint64_t absolute_address, bool is_exact) const;
  [[nodiscard]] ModuleData* FindModuleByAddress(uint64_t absolute_address) const;
  [[nodiscard]] uint64_t GetAbsoluteAddress(
      const orbit_client_protos::FunctionInfo& function) const;

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

  [[nodiscard]] const absl::flat_hash_map<int32_t,
                                          std::vector<orbit_client_protos::ThreadStateSliceInfo>>&
  thread_state_slices() const {
    return thread_state_slices_;
  }

  [[nodiscard]] bool HasThreadStatesForThread(int32_t tid) const {
    absl::MutexLock lock{thread_state_slices_mutex_.get()};
    return thread_state_slices_.count(tid) > 0;
  }

  void AddThreadStateSlice(orbit_client_protos::ThreadStateSliceInfo state_slice) {
    absl::MutexLock lock{thread_state_slices_mutex_.get()};
    thread_state_slices_[state_slice.tid()].emplace_back(std::move(state_slice));
  }

  // Allows the caller to iterate `action` over all the thread state slices of the specified thread
  // in the time range while holding for the whole time the internal mutex, acquired only once.
  void ForEachThreadStateSliceIntersectingTimeRange(
      int32_t thread_id, uint64_t min_timestamp, uint64_t max_timestamp,
      const std::function<void(const orbit_client_protos::ThreadStateSliceInfo&)>& action) const;

  [[nodiscard]] const FunctionInfoMap<orbit_client_protos::FunctionStats>& functions_stats() const {
    return functions_stats_;
  }

  [[nodiscard]] const orbit_client_protos::FunctionStats& GetFunctionStatsOrDefault(
      const orbit_client_protos::FunctionInfo& function) const;

  void UpdateFunctionStats(const orbit_client_protos::FunctionInfo& function,
                           uint64_t elapsed_nanos);

  [[nodiscard]] const CallstackData* GetCallstackData() const { return callstack_data_.get(); };

  [[nodiscard]] orbit_grpc_protos::TracepointInfo GetTracepointInfo(uint64_t key) const {
    return tracepoint_data_->GetTracepointInfo(key);
  }

  [[nodiscard]] const TracepointData* GetTracepointData() const { return tracepoint_data_.get(); }

  void ForEachTracepointEventOfThreadInTimeRange(
      int32_t thread_id, uint64_t min_tick, uint64_t max_tick,
      const std::function<void(const orbit_client_protos::TracepointEventInfo&)>& action) const {
    return tracepoint_data_->ForEachTracepointEventOfThreadInTimeRange(thread_id, min_tick,
                                                                       max_tick, action);
  }

  uint32_t GetNumTracepointsForThreadId(int32_t thread_id) const {
    return tracepoint_data_->GetNumTracepointEventsForThreadId(thread_id);
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
    tracepoint_data_->AddUniqueTracepointInfo(key, std::move(tracepoint_info));
  }

  void AddTracepointEventAndMapToThreads(uint64_t time, uint64_t tracepoint_hash,
                                         int32_t process_id, int32_t thread_id, int32_t cpu,
                                         bool is_same_pid_as_target) {
    tracepoint_data_->EmplaceTracepointEvent(time, tracepoint_hash, process_id, thread_id, cpu,
                                             is_same_pid_as_target);
  }

  [[nodiscard]] const CallstackData* GetSelectionCallstackData() const {
    return selection_callstack_data_.get();
  };

  void set_selection_callstack_data(std::unique_ptr<CallstackData> selection_callstack_data) {
    selection_callstack_data_ = std::move(selection_callstack_data);
  }

  [[nodiscard]] const ProcessData* process() const { return &process_; }

  [[nodiscard]] const PostProcessedSamplingData& post_processed_sampling_data() const {
    CHECK(post_processed_sampling_data_.has_value());
    return post_processed_sampling_data_.value();
  }

  void set_post_processed_sampling_data(PostProcessedSamplingData post_processed_sampling_data) {
    post_processed_sampling_data_ = std::move(post_processed_sampling_data);
  }

  void EnableFrameTrack(const orbit_client_protos::FunctionInfo& function);
  void DisableFrameTrack(const orbit_client_protos::FunctionInfo& function);
  [[nodiscard]] bool IsFrameTrackEnabled(const orbit_client_protos::FunctionInfo& function) const;
  [[nodiscard]] const UserDefinedCaptureData& user_defined_capture_data() const {
    return user_defined_capture_data_;
  }

 private:
  ProcessData process_;
  orbit_client_data::ModuleManager* module_manager_;
  absl::flat_hash_map<uint64_t, orbit_client_protos::FunctionInfo> selected_functions_;

  TracepointInfoSet selected_tracepoints_;
  // std::unique_ptr<> allows to move and copy CallstackData easier
  // (as CallstackData stores an absl::Mutex inside)
  std::unique_ptr<CallstackData> callstack_data_;
  // selection_callstack_data_ is subset of callstack_data_
  std::unique_ptr<CallstackData> selection_callstack_data_;

  std::unique_ptr<TracepointData> tracepoint_data_;

  std::optional<PostProcessedSamplingData> post_processed_sampling_data_;

  absl::flat_hash_map<uint64_t, orbit_client_protos::LinuxAddressInfo> address_infos_;

  FunctionInfoMap<orbit_client_protos::FunctionStats> functions_stats_;

  absl::flat_hash_map<int32_t, std::string> thread_names_;

  absl::flat_hash_map<int32_t, std::vector<orbit_client_protos::ThreadStateSliceInfo>>
      thread_state_slices_;  // For each thread, assume sorted by timestamp and not overlapping.
  mutable std::unique_ptr<absl::Mutex> thread_state_slices_mutex_ = std::make_unique<absl::Mutex>();

  std::chrono::system_clock::time_point capture_start_time_ = std::chrono::system_clock::now();

  UserDefinedCaptureData user_defined_capture_data_;
};

#endif  // ORBIT_CLIENT_MODEL_CAPTURE_DATA_H_
