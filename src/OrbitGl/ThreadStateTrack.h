// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_GL_THREAD_STATE_TRACK_H_
#define ORBIT_GL_THREAD_STATE_TRACK_H_

#include <GteVector.h>
#include <stdint.h>

#include <string>

#include "CoreMath.h"
#include "PickingManager.h"
#include "Track.h"

class OrbitApp;

// This is a track dedicated to displaying thread states in different colors
// and with the corresponding tooltips.
// It is a thin sub-track of ThreadTrack, added above the callstack track (EventTrack).
// The colors are determined only by the states, not by the color assigned to the thread.

class ThreadStateTrack final : public Track {
 public:
  explicit ThreadStateTrack(TimeGraph* time_graph, int32_t thread_id, OrbitApp* app,
                            CaptureData* capture_data);
  Type GetType() const override { return kThreadStateTrack; }

  void Draw(GlCanvas* canvas, PickingMode picking_mode, float z_offset) override;
  void UpdatePrimitives(uint64_t min_tick, uint64_t max_tick, PickingMode picking_mode,
                        float z_offset) override;

  void OnPick(int x, int y) override;
  void OnDrag(int /*x*/, int /*y*/) override {}
  void OnRelease() override { picked_ = false; };
  float GetHeight() const override { return size_[1]; }

  bool IsEmpty() const override;

 private:
  std::string GetThreadStateSliceTooltip(PickingId id) const;

  OrbitApp* app_ = nullptr;
};

#endif  // ORBIT_GL_THREAD_STATE_TRACK_H_
