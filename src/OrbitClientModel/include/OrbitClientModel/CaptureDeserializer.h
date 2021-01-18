// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_GL_CAPTURE_DESERIALIZER_H_
#define ORBIT_GL_CAPTURE_DESERIALIZER_H_

#include <atomic>
#include <iosfwd>
#include <outcome.hpp>
#include <string>

#include "CaptureData.h"
#include "OrbitBase/Result.h"
#include "OrbitCaptureClient/CaptureListener.h"
#include "OrbitClientData/ModuleManager.h"
#include "capture_data.pb.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/message.h"

namespace capture_deserializer {

void Load(std::istream& stream, const std::string& file_name, CaptureListener* capture_listener,
          orbit_client_data::ModuleManager* module_manager,
          std::atomic<bool>* cancellation_requested);
void Load(const std::string& file_name, CaptureListener* capture_listener,
          orbit_client_data::ModuleManager* module_manager,
          std::atomic<bool>* cancellation_requested);

namespace internal {

bool ReadMessage(google::protobuf::Message* message, google::protobuf::io::CodedInputStream* input);

void LoadCaptureInfo(const orbit_client_protos::CaptureInfo& capture_info,
                     CaptureListener* capture_listener,
                     orbit_client_data::ModuleManager* module_manager,
                     google::protobuf::io::CodedInputStream* coded_input,
                     std::atomic<bool>* cancellation_requested);

inline const std::string kRequiredCaptureVersion = "1.59";

}  // namespace internal

}  // namespace capture_deserializer

#endif  // ORBIT_GL_CAPTURE_DESERIALIZER_H_
