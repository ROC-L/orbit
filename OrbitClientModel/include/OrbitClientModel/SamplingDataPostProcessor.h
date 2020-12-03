// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_CLIENT_MODEL_SAMPLING_DATA_POST_PROCESSOR_H_
#define ORBIT_CLIENT_MODEL_SAMPLING_DATA_POST_PROCESSOR_H_

#include "OrbitClientData/CallstackData.h"
#include "OrbitClientModel/CaptureData.h"

namespace sampling_data_post_processor {
PostProcessedSamplingData CreatePostProcessedSamplingData(const CallstackData& callstack_data,
                                                          const CaptureData& capture_data,
                                                          bool generate_summary = true);
};  // namespace sampling_data_post_processor

#endif  // ORBIT_CLIENT_MODEL_SAMPLING_DATA_POST_PROCESSOR_H_
