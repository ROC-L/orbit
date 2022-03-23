// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "OrbitService.h"

#include <absl/strings/match.h>
#include <absl/strings/str_format.h>
#include <absl/strings/strip.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>

#include "OrbitBase/ExecuteCommand.h"
#include "OrbitBase/Logging.h"
#include "OrbitBase/ReadFileToString.h"
#include "OrbitGrpcServer.h"
#include "OrbitVersion/OrbitVersion.h"
#include "ProducerSideService/BuildAndStartProducerSideServer.h"
#include "ProducerSideService/ProducerSideServer.h"

using orbit_producer_side_service::ProducerSideServer;

namespace orbit_service {

namespace {

#ifdef __linux
void PrintInstanceVersions() {
  {
    constexpr const char* kKernelVersionCommand = "uname -a";
    std::optional<std::string> version = orbit_base::ExecuteCommand(kKernelVersionCommand);
    if (version.has_value()) {
      ORBIT_LOG("%s: %s", kKernelVersionCommand, absl::StripSuffix(version.value(), "\n"));
    } else {
      ORBIT_ERROR("Could not execute \"%s\"", kKernelVersionCommand);
    }
  }

  {
    constexpr const char* kVersionFilePath = "/usr/local/cloudcast/VERSION";
    ErrorMessageOr<std::string> version = orbit_base::ReadFileToString(kVersionFilePath);
    if (version.has_value()) {
      ORBIT_LOG("%s:\n%s", kVersionFilePath, absl::StripSuffix(version.value(), "\n"));
    } else {
      ORBIT_ERROR("%s", version.error().message());
    }
  }

  {
    constexpr const char* kBaseVersionFilePath = "/usr/local/cloudcast/BASE_VERSION";
    ErrorMessageOr<std::string> version = orbit_base::ReadFileToString(kBaseVersionFilePath);
    if (version.has_value()) {
      ORBIT_LOG("%s:\n%s", kBaseVersionFilePath, absl::StripSuffix(version.value(), "\n"));
    } else {
      ORBIT_ERROR("%s", version.error().message());
    }
  }

  {
    constexpr const char* kInstanceVersionFilePath = "/usr/local/cloudcast/INSTANCE_VERSION";
    ErrorMessageOr<std::string> version = orbit_base::ReadFileToString(kInstanceVersionFilePath);
    if (version.has_value()) {
      ORBIT_LOG("%s:\n%s", kInstanceVersionFilePath, absl::StripSuffix(version.value(), "\n"));
    } else {
      ORBIT_ERROR("%s", version.error().message());
    }
  }

  {
    constexpr const char* kDriverVersionCommand = "/usr/local/cloudcast/bin/gpuinfo driver-version";
    std::optional<std::string> version = orbit_base::ExecuteCommand(kDriverVersionCommand);
    std::string stripped_version;
    if (version.has_value()) {
      stripped_version = absl::StripSuffix(version.value(), "\n");
    }
    if (!stripped_version.empty()) {
      ORBIT_LOG("%s: %s", kDriverVersionCommand, stripped_version);
    } else {
      ORBIT_ERROR("Could not execute \"%s\"", kDriverVersionCommand);
    }
  }
}
#endif

std::string ReadStdIn() {
  int tmp = fgetc(stdin);
  if (tmp == -1) return "";

  std::string result;
  do {
    result += static_cast<char>(tmp);
    tmp = fgetc(stdin);
  } while (tmp != -1);

  return result;
}

bool IsSshConnectionAlive(std::chrono::time_point<std::chrono::steady_clock> last_ssh_message,
                          const int timeout_in_seconds) {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() -
                                                          last_ssh_message)
             .count() < timeout_in_seconds;
}

std::unique_ptr<OrbitGrpcServer> CreateGrpcServer(uint16_t grpc_port, bool dev_mode) {
  std::string grpc_address = absl::StrFormat("127.0.0.1:%d", grpc_port);
  ORBIT_LOG("Starting gRPC server at %s", grpc_address);
  std::unique_ptr<OrbitGrpcServer> grpc_server = OrbitGrpcServer::Create(grpc_address, dev_mode);
  if (grpc_server == nullptr) {
    ORBIT_ERROR("Unable to start gRPC server");
    return nullptr;
  }
  ORBIT_LOG("gRPC server is running");
  return grpc_server;
}

}  // namespace

int OrbitService::Run(std::atomic<bool>* exit_requested) {
#ifdef __linux
  PrintInstanceVersions();
#endif

  ORBIT_LOG("Running Orbit Service version %s", orbit_version::GetVersionString());
#ifndef NDEBUG
  ORBIT_LOG("**********************************");
  ORBIT_LOG("Orbit Service is running in DEBUG!");
  ORBIT_LOG("**********************************");
#endif

  std::unique_ptr<OrbitGrpcServer> grpc_server = CreateGrpcServer(grpc_port_, dev_mode_);
  if (grpc_server == nullptr) {
    ORBIT_ERROR("Unable to create gRPC server.");
    return -1;
  }

  std::unique_ptr<ProducerSideServer> producer_side_server =
      orbit_producer_side_service::BuildAndStartProducerSideServer();
  if (producer_side_server == nullptr) {
    ORBIT_ERROR("Unable to build and start ProducerSideServer.");
    return -1;
  }
  grpc_server->AddCaptureStartStopListener(producer_side_server.get());

#ifdef __linux
  // Make stdin non-blocking.
  fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
#endif

  int return_code = 0;

  // Wait for exit_request or for the watchdog to expire.
  while (!(*exit_requested)) {
    // TODO(b/211035029): Port SSH watchdog to Windows.
#ifdef __linux
    std::string stdin_data = ReadStdIn();
    // If ssh sends EOF, end main loop.
    if (feof(stdin) != 0) {
      ORBIT_LOG("Received EOF on stdin. Exiting main loop.");
      break;
    }

    if (IsSshWatchdogActive() || absl::StrContains(stdin_data, kStartWatchdogPassphrase)) {
      if (!stdin_data.empty()) {
        last_stdin_message_ = std::chrono::steady_clock::now();
      }

      if (!IsSshConnectionAlive(last_stdin_message_.value(), kWatchdogTimeoutInSeconds)) {
        ORBIT_ERROR("Connection is not alive (watchdog timed out). Exiting main loop.");
        return_code = -1;
        break;
      }
    }
#endif

    std::this_thread::sleep_for(std::chrono::milliseconds{200});
  }

  producer_side_server->ShutdownAndWait();
  grpc_server->RemoveCaptureStartStopListener(producer_side_server.get());

  grpc_server->Shutdown();
  grpc_server->Wait();

  return return_code;
}

}  // namespace orbit_service
