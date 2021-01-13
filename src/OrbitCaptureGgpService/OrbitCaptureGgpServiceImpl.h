// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_CAPTURE_GGP_SERVICE_ORBIT_CAPTURE_GGP_SERVICE_IMPL_H_
#define ORBIT_CAPTURE_GGP_SERVICE_ORBIT_CAPTURE_GGP_SERVICE_IMPL_H_

#include <grpcpp/grpcpp.h>

#include <memory>

#include "OrbitBase/ThreadPool.h"
#include "OrbitClientGgp/ClientGgp.h"
#include "services_ggp.grpc.pb.h"
#include "services_ggp.pb.h"

// Logic and data behind the server's behavior.
class CaptureClientGgpServiceImpl final
    : public orbit_grpc_protos::CaptureClientGgpService::Service {
 public:
  CaptureClientGgpServiceImpl();

  [[nodiscard]] grpc::Status StartCapture(
      grpc::ServerContext* context, const orbit_grpc_protos::StartCaptureRequest* request,
      orbit_grpc_protos::StartCaptureResponse* response) override;

  [[nodiscard]] grpc::Status StopAndSaveCapture(
      grpc::ServerContext* context, const orbit_grpc_protos::StopAndSaveCaptureRequest* request,
      orbit_grpc_protos::StopAndSaveCaptureResponse* response) override;

  [[nodiscard]] grpc::Status UpdateSelectedFunctions(
      grpc::ServerContext* context,
      const orbit_grpc_protos::UpdateSelectedFunctionsRequest* request,
      orbit_grpc_protos::UpdateSelectedFunctionsResponse* response) override;

  [[nodiscard]] grpc::Status ShutdownService(
      grpc::ServerContext* context, const orbit_grpc_protos::ShutdownServiceRequest* request,
      orbit_grpc_protos::ShutdownServiceResponse* response) override;

  bool ShutdownRequested();

 private:
  std::unique_ptr<ClientGgp> client_ggp_;
  std::unique_ptr<ThreadPool> thread_pool_;
  bool shutdown_ = false;

  void InitClientGgp();
  void SaveCapture();
  void Shutdown();
  bool CaptureIsRunning();
};

#endif  // ORBIT_CAPTURE_GGP_SERVICE_ORBIT_CAPTURE_GGP_SERVICE_IMPL_H_
