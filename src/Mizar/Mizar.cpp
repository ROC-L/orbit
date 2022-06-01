// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/container/flat_hash_set.h>

#include <iostream>
#include <string>

#include "CaptureClient/LoadCapture.h"
#include "CaptureFile/CaptureFile.h"
#include "MizarData/MizarData.h"
#include "OrbitBase/Logging.h"

int main(int argc, char* argv[]) {
  // The main in its current state is used to testing/experimenting and serves no other purpose
  if (argc < 2) {
    ORBIT_ERROR("No file path given");
    return 1;
  }
  const std::filesystem::path path = argv[1];
  auto capture_file_or_error = orbit_capture_file::CaptureFile::OpenForReadWrite(path);
  if (capture_file_or_error.has_error()) {
    ORBIT_ERROR("%s", capture_file_or_error.error().message());
    return 1;
  }
  std::atomic<bool> capture_loading_cancellation_requested = false;

  orbit_mizar_data::MizarData data;
  auto status = orbit_capture_client::LoadCapture(&data, capture_file_or_error.value().get(),
                                                  &capture_loading_cancellation_requested);
  const auto& callstack_data = data.GetCaptureData().GetCallstackData();
  std::vector<orbit_client_data::CallstackEvent> callstack_events =
      callstack_data.GetCallstackEventsInTimeRange(std::numeric_limits<uint64_t>::min(),
                                                   std::numeric_limits<uint64_t>::max());

  absl::flat_hash_set<std::string> names;
  for (auto event : callstack_events) {
    const orbit_client_data::CallstackInfo* callstack =
        callstack_data.GetCallstack(event.callstack_id());
    for (auto addr : callstack->frames()) {
      std::optional<std::string> name = data.GetFunctionNameFromAddress(addr);
      if (name.has_value()) names.insert(name.value());
    }
  }

  for (const auto& name : names) {
    ORBIT_LOG("%s", name);
  }

  ORBIT_LOG("total stack sample count %u, total number of functions %u",
            callstack_data.GetCallstackEventsCount(), names.size());
  return 0;
}