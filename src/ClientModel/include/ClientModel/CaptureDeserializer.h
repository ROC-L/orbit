// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLIENT_MODEL_CAPTURE_DESERIALIZER_H_
#define CLIENT_MODEL_CAPTURE_DESERIALIZER_H_

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/message.h>

#include <atomic>
#include <filesystem>
#include <iosfwd>
#include <string>

#include "CaptureClient/CaptureListener.h"
#include "CaptureData.h"
#include "ClientData/ModuleManager.h"
#include "OrbitBase/File.h"
#include "OrbitBase/Result.h"
#include "capture_data.pb.h"

namespace orbit_client_model::capture_deserializer {

ErrorMessageOr<orbit_capture_client::CaptureListener::CaptureOutcome> Load(
    google::protobuf::io::CodedInputStream* input_stream, const std::filesystem::path& file_name,
    orbit_capture_client::CaptureListener* capture_listener,
    orbit_client_data::ModuleManager* module_manager, std::atomic<bool>* cancellation_requested);
ErrorMessageOr<orbit_capture_client::CaptureListener::CaptureOutcome> Load(
    const std::filesystem::path& file_name, orbit_capture_client::CaptureListener* capture_listener,
    orbit_client_data::ModuleManager* module_manager, std::atomic<bool>* cancellation_requested);

namespace internal {

bool ReadMessage(google::protobuf::Message* message, google::protobuf::io::CodedInputStream* input);

ErrorMessageOr<orbit_capture_client::CaptureListener::CaptureOutcome> LoadCaptureInfo(
    const orbit_client_protos::CaptureInfo& capture_info,
    orbit_capture_client::CaptureListener* capture_listener,
    orbit_client_data::ModuleManager* module_manager,
    google::protobuf::io::CodedInputStream* coded_input, std::atomic<bool>* cancellation_requested);

inline const std::string kRequiredCaptureVersion = "1.59";

}  // namespace internal

}  // namespace orbit_client_model::capture_deserializer

#endif  // CLIENT_MODEL_CAPTURE_DESERIALIZER_H_
