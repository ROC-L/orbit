// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_GL_TRACEPOINT_TRACK_H_
#define ORBIT_GL_TRACEPOINT_TRACK_H_

#include <stdint.h>

#include <string>

#include "EventTrack.h"
#include "PickingManager.h"

class TracepointTrack : public EventTrack {
 public:
  explicit TracepointTrack(TimeGraph* time_graph, int32_t thread_id, OrbitApp* app,
                           CaptureData* capture_data);

  void Draw(GlCanvas* canvas, PickingMode picking_mode, float z_offset = 0) override;

  void UpdatePrimitives(uint64_t min_tick, uint64_t max_tick, PickingMode picking_mode,
                        float z_offset = 0) override;

  void OnPick(int x, int y) override;
  void OnRelease() override;
  bool IsEmpty() const override;

 private:
  std::string GetTracepointTooltip(PickingId id) const;
};

#endif  // ORBIT_GL_TRACEPOINT_TRACK_H_
