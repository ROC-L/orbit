// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/flags/flag.h>

#include <cstdio>
#include <filesystem>

#include "App.h"
#include "CaptureSerializer.h"
#include "SamplingProfiler.h"
#include "TimeGraph.h"

// Hack: This is declared in a header we include here
// and the definition needs to take place somewhere.
ABSL_FLAG(bool, enable_stale_features, false,
          "Enable obsolete features that are not working or are not "
          "implemented in the client's UI");
ABSL_FLAG(bool, devmode, false, "Enable developer mode in the client's UI");
ABSL_FLAG(uint16_t, sampling_rate, 1000,
          "Frequency of callstack sampling in samples per second");
ABSL_FLAG(bool, frame_pointer_unwinding, false,
          "Use frame pointers for unwinding");

std::string capture_file;

extern "C" {

int LLVMFuzzerTestOneInput(uint8_t* buf, size_t len) {
  CaptureSerializer serializer{};
  TimeGraph time_graph{};
  auto string_manager = std::make_shared<StringManager>();
  time_graph.SetStringManager(string_manager);
  serializer.time_graph_ = &time_graph;

  std::istringstream stream(
      std::string(reinterpret_cast<const char*>(buf), len));
  try {
    (void)serializer.Load(stream);
  } catch (...) {
  }

  Capture::GSamplingProfiler.reset();
  return 0;
}

int LLVMFuzzerInitialize(int*, char***) {
  OrbitApp::Init({}, nullptr);
  return 0;
}
}  // extern "C"
