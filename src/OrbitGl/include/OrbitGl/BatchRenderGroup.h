// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_GL_BATCH_RENDER_GROUP_H_
#define ORBIT_GL_BATCH_RENDER_GROUP_H_

#include <functional>
#include <string>

#include "absl/container/flat_hash_map.h"

namespace orbit_gl {

// A BatchRenderGroupId identifies a group of primitives that can be rendered together in a single
// draw call. It is conceptually similar to a layer z-value, but it also includes a name that can be
// used to manually separate elements into different groups. It is used by the Batcher and
// TextRenderer to group elements into draw calls.
struct BatchRenderGroupId {
  static constexpr std::string_view kGlobalGroup = "global";

  std::string name = std::string(kGlobalGroup);
  float layer = 0;

  friend bool operator==(const BatchRenderGroupId& lhs, const BatchRenderGroupId& rhs) {
    return std::tie(lhs.layer, lhs.name) == std::tie(rhs.layer, rhs.name);
  }

  friend bool operator!=(const BatchRenderGroupId& lhs, const BatchRenderGroupId& rhs) {
    return !(lhs == rhs);
  }

  friend bool operator<(const BatchRenderGroupId& lhs, const BatchRenderGroupId& rhs) {
    return std::tie(lhs.layer, lhs.name) < std::tie(rhs.layer, rhs.name);
  }

  friend bool operator<=(const BatchRenderGroupId& lhs, const BatchRenderGroupId& rhs) {
    return lhs < rhs || lhs == rhs;
  }

  friend bool operator>(const BatchRenderGroupId& lhs, const BatchRenderGroupId& rhs) {
    return !(lhs <= rhs);
  }

  friend bool operator>=(const BatchRenderGroupId& lhs, const BatchRenderGroupId& rhs) {
    return !(lhs < rhs);
  }

  explicit BatchRenderGroupId(float layer = 0, std::string name = std::string(kGlobalGroup))
      : name(std::move(name)), layer(layer) {}

  template <typename H>
  friend H AbslHashValue(H h, const BatchRenderGroupId& o) {
    return H::combine(std::move(h), o.name, o.layer);
  }
};

// Group property: This defines if all rendering should be restricted to a bounding box, and if so,
// stores the extents of this box
struct StencilConfig {
  bool enabled = false;
  std::array<float, 2> pos = {0, 0};
  std::array<float, 2> size = {0, 0};
};

// Collection of all properties that are associated with a BatchRenderGroup and that will influence
// how the group is rendered.
struct BatchRenderGroupState {
  StencilConfig stencil;
};

// Maps render group names to a collection of properties that influence rendering. This decouples
// the state of render groups from the Batcher, as an instance of this class will be shared by
// multiple Batcher and TextRenderer instances (usually all that use the same canvas).
// Note that the mapping is done by group name only, not by the full group ID. Conceptually, this
// means that groups with the same name share common properties, and the "layer" part of the group
// ID defines a sub-ordering underneath this group name to ensure correct rendering order.
class BatchRenderGroupStateManager {
 public:
  [[nodiscard]] BatchRenderGroupState GetGroupState(std::string_view group_name) const {
    return group_name_to_state_.contains(group_name) ? group_name_to_state_.at(group_name)
                                                     : BatchRenderGroupState();
  }

  void SetGroupState(std::string_view group_name, BatchRenderGroupState state) {
    group_name_to_state_[group_name] = state;
  }

 private:
  absl::flat_hash_map<std::string, BatchRenderGroupState> group_name_to_state_;
};

}  // namespace orbit_gl

#endif