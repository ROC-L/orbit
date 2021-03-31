// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_API_MANUAL_INSTRUMENTATION_H_
#define ORBIT_API_MANUAL_INSTRUMENTATION_H_

#include "OrbitBase/Result.h"

namespace orbit_api {

ErrorMessageOr<void> InitializeManualInstrumentationForProcess(int pid);

}  // namespace orbit_api

#endif
