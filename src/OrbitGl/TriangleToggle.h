// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_GL_TRIANGLE_TOGGLE_H_
#define ORBIT_GL_TRIANGLE_TOGGLE_H_

#include <functional>
#include <memory>

#include "CoreMath.h"
#include "PickingManager.h"

class TimeGraph;
class GlCanvas;

class TriangleToggle : public Pickable, public std::enable_shared_from_this<TriangleToggle> {
 public:
  enum class State { kInactive, kExpanded, kCollapsed };
  enum class InitialStateUpdate { kKeepInitialState, kReplaceInitialState };

  using StateChangeHandler = std::function<void(TriangleToggle::State)>;
  explicit TriangleToggle(State initial_state, StateChangeHandler handler, TimeGraph* time_graph);
  ~TriangleToggle() override = default;

  TriangleToggle() = delete;
  TriangleToggle(const TriangleToggle&) = delete;
  TriangleToggle& operator=(const TriangleToggle&) = delete;
  TriangleToggle(TriangleToggle&&) = delete;
  TriangleToggle& operator=(TriangleToggle&&) = delete;

  void Draw(GlCanvas* canvas, PickingMode picking_mode, float z_offset = 0);

  // Pickable
  void OnPick(int x, int y) override;
  void OnRelease() override;

  State GetState() const { return state_; }
  void SetState(State state,
                InitialStateUpdate update_initial_state = InitialStateUpdate::kKeepInitialState);
  void ResetToInitialState() { state_ = initial_state_; }
  bool IsCollapsed() const { return state_ == State::kCollapsed; }
  bool IsExpanded() const { return state_ == State::kExpanded; }
  bool IsInactive() const { return state_ == State::kInactive; }
  Vec2 GetPos() const { return pos_; }
  void SetPos(const Vec2& pos) { pos_ = pos; }

 private:
  State state_ = State::kInactive;
  State initial_state_ = State::kInactive;
  StateChangeHandler handler_;
  TimeGraph* time_graph_;
  Vec2 pos_ = {0, 0};
  float size_ = 10.f;
};

#endif  // ORBIT_GL_TRIANGLE_TOGGLE_H_
