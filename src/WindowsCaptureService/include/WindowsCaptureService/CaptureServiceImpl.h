// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOWS_SERVICE_CAPTURE_SERVICE_IMPL_H_
#define WINDOWS_SERVICE_CAPTURE_SERVICE_IMPL_H_

#include <grpcpp/grpcpp.h>

#include <atomic>

#include "CaptureStartStopListener.h"
#include "absl/container/flat_hash_set.h"
#include "services.grpc.pb.h"
#include "services.pb.h"

namespace windows_service {

class CaptureServiceImpl final : public orbit_grpc_protos::CaptureService::Service {
 public:
  grpc::Status Capture(
      grpc::ServerContext* context,
      grpc::ServerReaderWriter<orbit_grpc_protos::CaptureResponse,
                               orbit_grpc_protos::CaptureRequest>* reader_writer) override;

  void AddCaptureStartStopListener(CaptureStartStopListener* listener);
  void RemoveCaptureStartStopListener(CaptureStartStopListener* listener);

 private:
  std::atomic<bool> is_capturing = false;
  absl::flat_hash_set<CaptureStartStopListener*> capture_start_stop_listeners_;
};

}  // namespace windows_service

#endif  // WINDOWS_SERVICE_CAPTURE_SERVICE_IMPL_H_
