// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_CAPTURE_CLIENT_CAPTURE_EVENT_PROCESSOR_H_
#define ORBIT_CAPTURE_CLIENT_CAPTURE_EVENT_PROCESSOR_H_

#include "OrbitCaptureClient/CaptureListener.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "services.pb.h"

class CaptureEventProcessor {
 public:
  explicit CaptureEventProcessor(CaptureListener* capture_listener)
      : capture_listener_(capture_listener) {}

  void ProcessEvent(const orbit_grpc_protos::CaptureEvent& event);

  template <typename Iterable>
  void ProcessEvents(const Iterable& events) {
    for (const auto& event : events) {
      ProcessEvent(event);
    }
  }

 private:
  void ProcessSchedulingSlice(
      const orbit_grpc_protos::SchedulingSlice& scheduling_slice);
  void ProcessInternedCallstack(
      orbit_grpc_protos::InternedCallstack interned_callstack);
  void ProcessCallstackSample(
      const orbit_grpc_protos::CallstackSample& callstack_sample);
  void ProcessFunctionCall(
      const orbit_grpc_protos::FunctionCall& function_call);
  void ProcessInternedString(orbit_grpc_protos::InternedString interned_string);
  void ProcessGpuJob(const orbit_grpc_protos::GpuJob& gpu_job);
  void ProcessThreadName(const orbit_grpc_protos::ThreadName& thread_name);
  void ProcessAddressInfo(const orbit_grpc_protos::AddressInfo& address_info);

  absl::flat_hash_map<uint64_t, orbit_grpc_protos::Callstack>
      callstack_intern_pool;
  absl::flat_hash_map<uint64_t, std::string> string_intern_pool;
  CaptureListener* capture_listener_ = nullptr;

  absl::flat_hash_set<uint64_t> callstack_hashes_seen_;
  uint64_t GetCallstackHashAndSendToListenerIfNecessary(
      const orbit_grpc_protos::Callstack& callstack);
  absl::flat_hash_set<uint64_t> string_hashes_seen_;
  uint64_t GetStringHashAndSendToListenerIfNecessary(const std::string& str);
};

#endif  // ORBIT_GL_CAPTURE_EVENT_PROCESSOR_H_