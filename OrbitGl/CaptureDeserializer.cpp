// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CaptureDeserializer.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/message.h>

#include <fstream>
#include <memory>

#include "App.h"
#include "Callstack.h"
#include "EventTracer.h"
#include "FunctionUtils.h"
#include "OrbitBase/MakeUniqueForOverwrite.h"
#include "OrbitProcess.h"
#include "SamplingProfiler.h"
#include "TimeGraph.h"
#include "absl/strings/str_format.h"
#include "capture_data.pb.h"

using orbit_client_protos::CallstackEvent;
using orbit_client_protos::CallstackInfo;
using orbit_client_protos::CaptureInfo;
using orbit_client_protos::FunctionInfo;
using orbit_client_protos::FunctionStats;
using orbit_client_protos::TimerInfo;

ErrorMessageOr<void> CaptureDeserializer::Load(const std::string& filename) {
  SCOPE_TIMER_LOG(absl::StrFormat("Loading capture from \"%s\"", filename));

  // Binary
  std::ifstream file(filename, std::ios::binary);
  if (file.fail()) {
    ERROR("Loading capture from \"%s\": %s", filename, "file.fail()");
    return ErrorMessage("Error opening the file for reading");
  }

  return Load(file);
}

bool CaptureDeserializer::ReadMessage(google::protobuf::Message* message,
                                      google::protobuf::io::CodedInputStream* input) {
  uint32_t message_size;
  if (!input->ReadLittleEndian32(&message_size)) {
    return false;
  }

  std::unique_ptr<char[]> buffer = make_unique_for_overwrite<char[]>(message_size);
  if (!input->ReadRaw(buffer.get(), message_size)) {
    return false;
  }
  message->ParseFromArray(buffer.get(), message_size);

  return true;
}

static void FillEventBuffer(const CaptureData& capture_data) {
  GEventTracer.GetEventBuffer().Reset();
  for (const CallstackEvent& callstack_event :
       capture_data.GetCallstackData()->callstack_events()) {
    GEventTracer.GetEventBuffer().AddCallstackEvent(
        callstack_event.time(), callstack_event.callstack_hash(), callstack_event.thread_id());
  }
}

CaptureData CaptureDeserializer::GenerateCaptureData(const CaptureInfo& capture_info) {
  // Clear the old capture
  GOrbitApp->ClearSelectedFunctions();
  absl::flat_hash_map<uint64_t, orbit_client_protos::FunctionInfo> selected_functions;
  absl::flat_hash_set<uint64_t> visible_functions;
  for (const auto& function : capture_info.selected_functions()) {
    uint64_t address = FunctionUtils::GetAbsoluteAddress(function);
    selected_functions[address] = function;
    visible_functions.insert(address);
  }
  GOrbitApp->SetVisibleFunctions(std::move(visible_functions));

  absl::flat_hash_map<uint64_t, FunctionStats> functions_stats{
      capture_info.function_stats().begin(), capture_info.function_stats().end()};
  CaptureData capture_data(capture_info.process_id(), capture_info.process_name(),
                           std::make_shared<Process>(), std::move(selected_functions),
                           std::move(functions_stats));

  for (const auto& address_info : capture_info.address_infos()) {
    capture_data.InsertAddressInfo(address_info);
  }

  absl::flat_hash_map<int32_t, std::string> thread_names{capture_info.thread_names().begin(),
                                                         capture_info.thread_names().end()};
  capture_data.set_thread_names(thread_names);

  for (const CallstackInfo& callstack : capture_info.callstacks()) {
    CallStack unique_callstack({callstack.data().begin(), callstack.data().end()});
    capture_data.AddUniqueCallStack(std::move(unique_callstack));
  }
  for (CallstackEvent callstack_event : capture_info.callstack_events()) {
    capture_data.AddCallstackEvent(std::move(callstack_event));
  }
  SamplingProfiler sampling_profiler(*capture_data.GetCallstackData(), capture_data);
  capture_data.set_sampling_profiler(sampling_profiler);

  time_graph_->Clear();
  StringManager* string_manager = time_graph_->GetStringManager();
  string_manager->Clear();
  for (const auto& entry : capture_info.key_to_string()) {
    string_manager->AddIfNotPresent(entry.first, entry.second);
  }

  FillEventBuffer(capture_data);

  return capture_data;
}

ErrorMessageOr<void> CaptureDeserializer::Load(std::istream& stream) {
  google::protobuf::io::IstreamInputStream input_stream(&stream);
  google::protobuf::io::CodedInputStream coded_input(&input_stream);

  std::string error_message =
      "Error parsing the capture.\nNote: If the capture "
      "was taken with a previous Orbit version, it could be incompatible. "
      "Please check release notes for more information.";

  if (!ReadMessage(&header_, &coded_input) || header_.version().empty()) {
    ERROR("%s", error_message);
    return ErrorMessage(error_message);
  }
  if (header_.version() != kRequiredCaptureVersion) {
    std::string incompatible_version_error_message = absl::StrFormat(
        "This capture format is no longer supported but could be opened with "
        "Orbit version %s.",
        header_.version());
    ERROR("%s", incompatible_version_error_message);
    return ErrorMessage(incompatible_version_error_message);
  }

  CaptureInfo capture_info;
  if (!ReadMessage(&capture_info, &coded_input)) {
    ERROR("%s", error_message);
    return ErrorMessage(error_message);
  }
  CaptureData capture_data = GenerateCaptureData(capture_info);

  // Timers
  TimerInfo timer_info;
  while (ReadMessage(&timer_info, &coded_input)) {
    if (timer_info.function_address() > 0) {
      const FunctionInfo& func =
          capture_data.selected_functions().at(timer_info.function_address());
      time_graph_->ProcessTimer(timer_info, &func);
    } else {
      time_graph_->ProcessTimer(timer_info, nullptr);
    }
  }

  GOrbitApp->SetSamplingReport(capture_data.sampling_profiler(),
                               capture_data.GetCallstackData()->GetUniqueCallstacksCopy());
  GOrbitApp->SetTopDownView(capture_data);
  GOrbitApp->SetCaptureData(std::move(capture_data));
  GOrbitApp->FireRefreshCallbacks();
  return outcome::success();
}
