// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "OrbitClientData/UserDefinedCaptureData.h"

#include "OrbitClientData/FunctionUtils.h"

void UserDefinedCaptureData::InsertFrameTrack(const orbit_client_protos::FunctionInfo& function) {
  frame_track_functions_.insert(function);
}

void UserDefinedCaptureData::EraseFrameTrack(const orbit_client_protos::FunctionInfo& function) {
  frame_track_functions_.erase(function);
}

[[nodiscard]] bool UserDefinedCaptureData::ContainsFrameTrack(
    const orbit_client_protos::FunctionInfo& function) const {
  return frame_track_functions_.contains(function);
}
