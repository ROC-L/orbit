// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "OrbitClientModel/CaptureSerializer.h"

#include <memory>

#include "Callstack.h"
#include "OrbitProcess.h"
#include "Path.h"
#include "capture_data.pb.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/message.h"

using orbit_client_protos::CallstackInfo;
using orbit_client_protos::CaptureInfo;
using orbit_client_protos::FunctionStats;

namespace {
inline constexpr std::string_view kFileOrbitExtension = ".orbit";
}

namespace capture_serializer {

void WriteMessage(const google::protobuf::Message* message,
                  google::protobuf::io::CodedOutputStream* output) {
  uint32_t message_size = message->ByteSizeLong();
  output->WriteLittleEndian32(message_size);
  message->SerializeToCodedStream(output);
}

std::string GetCaptureFileName(const CaptureData& capture_data) {
  time_t timestamp = std::chrono::system_clock::to_time_t(capture_data.capture_start_time());
  std::string result = absl::StrCat(Path::StripExtension(capture_data.process_name()), "_",
                                    OrbitUtils::FormatTime(timestamp));
  IncludeOrbitExtensionInFile(result);
  return result;
}

void IncludeOrbitExtensionInFile(std::string& file_name) {
  const std::string extension = Path::GetExtension(file_name);
  if (extension != kFileOrbitExtension) {
    file_name.append(kFileOrbitExtension);
  }
}

namespace internal {

CaptureInfo GenerateCaptureInfo(
    const CaptureData& capture_data,
    const absl::flat_hash_map<uint64_t, std::string>& key_to_string_map) {
  CaptureInfo capture_info;
  for (const auto& pair : capture_data.selected_functions()) {
    capture_info.add_selected_functions()->CopyFrom(pair.second);
  }

  capture_info.set_process_id(capture_data.process_id());
  capture_info.set_process_name(capture_data.process_name());

  capture_info.mutable_thread_names()->insert(capture_data.thread_names().begin(),
                                              capture_data.thread_names().end());

  capture_info.mutable_address_infos()->Reserve(capture_data.address_infos().size());
  for (const auto& address_info : capture_data.address_infos()) {
    orbit_client_protos::LinuxAddressInfo* added_address_info = capture_info.add_address_infos();
    added_address_info->CopyFrom(address_info.second);
    // Fix names in address infos (some might only be in process):
    added_address_info->set_function_name(
        capture_data.GetFunctionNameByAddress(added_address_info->absolute_address()));
  }

  const absl::flat_hash_map<uint64_t, FunctionStats>& functions_stats =
      capture_data.functions_stats();
  capture_info.mutable_function_stats()->insert(functions_stats.begin(), functions_stats.end());

  // TODO: this is not really synchronized, since GetCallstackData processing below is not under the
  // same mutex lock we could end up having list of callstacks inconsistent with unique_callstacks.
  // Revisit sampling profiler data thread-safety.
  capture_data.GetCallstackData()->ForEachUniqueCallstack(
      [&capture_info](const CallStack& call_stack) {
        CallstackInfo* callstack = capture_info.add_callstacks();
        *callstack->mutable_data() = {call_stack.GetFrames().begin(), call_stack.GetFrames().end()};
      });

  capture_info.mutable_callstack_events()->Reserve(
      capture_data.GetCallstackData()->GetCallstackEventsCount());
  capture_data.GetCallstackData()->ForEachCallstackEvent(
      [&capture_info](const orbit_client_protos::CallstackEvent& event) {
        capture_info.add_callstack_events()->CopyFrom(event);
      });

  capture_data.GetTracepointInfoManager()->ForEachUniqueTracepointInfo(
      [&capture_info](const orbit_client_protos::TracepointInfo& tracepoint_info) {
        orbit_client_protos::TracepointInfo* new_tracepoint_info =
            capture_info.add_tracepoint_infos();
        *new_tracepoint_info->mutable_category() = tracepoint_info.category();
        *new_tracepoint_info->mutable_name() = tracepoint_info.name();
        new_tracepoint_info->set_tracepoint_info_key(tracepoint_info.tracepoint_info_key());
      });

  capture_data.GetTracepointEventBuffer()->ForEachTracepointEvent(
      [&capture_info](const orbit_client_protos::TracepointEventInfo& tracepoint_event_info) {
        capture_info.add_tracepoint_event_infos()->CopyFrom(tracepoint_event_info);
      });

  capture_info.mutable_key_to_string()->insert(key_to_string_map.begin(), key_to_string_map.end());

  return capture_info;
}

}  // namespace internal

}  // namespace capture_serializer
