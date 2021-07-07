// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_GL_MEMORY_TRACK_H_
#define ORBIT_GL_MEMORY_TRACK_H_

#include <string>
#include <utility>

#include "AnnotationTrack.h"
#include "GraphTrack.h"
#include "Track.h"
#include "Viewport.h"

namespace orbit_gl {

template <size_t Dimension>
class MemoryTrack final : public GraphTrack<Dimension>, public AnnotationTrack {
 public:
  explicit MemoryTrack(CaptureViewElement* parent, TimeGraph* time_graph,
                       orbit_gl::Viewport* viewport, TimeGraphLayout* layout, std::string name,
                       std::array<std::string, Dimension> series_names,
                       const orbit_client_model::CaptureData* capture_data)
      : GraphTrack<Dimension>(parent, time_graph, viewport, layout, name, series_names,
                              capture_data),
        AnnotationTrack() {}
  ~MemoryTrack() override = default;
  [[nodiscard]] Track::Type GetType() const override { return Track::Type::kMemoryTrack; }
  void Draw(Batcher& batcher, TextRenderer& text_renderer, uint64_t current_mouse_time_ns,
            PickingMode picking_mode, float z_offset = 0) override;

  void TrySetValueUpperBound(const std::string& pretty_label, double raw_value);
  void TrySetValueLowerBound(const std::string& pretty_label, double raw_value);

 protected:
  [[nodiscard]] double GetGraphMaxValue() const override;
  [[nodiscard]] double GetGraphMinValue() const override;

 private:
  [[nodiscard]] float GetAnnotatedTrackContentHeight() const override {
    return this->GetGraphContentHeight();
  }
  [[nodiscard]] Vec2 GetAnnotatedTrackPosition() const override { return this->pos_; };
  [[nodiscard]] Vec2 GetAnnotatedTrackSize() const override { return this->size_; };
  [[nodiscard]] uint32_t GetAnnotationFontSize() const override {
    return this->GetLegendFontSize();
  }
};

constexpr size_t kSystemMemoryTrackDimension = 3;
using SystemMemoryTrack = MemoryTrack<kSystemMemoryTrackDimension>;

constexpr size_t kCGroupAndProcessMemoryTrackDimension = 4;
using CGroupAndProcessMemoryTrack = MemoryTrack<kCGroupAndProcessMemoryTrackDimension>;

// Colors are selected from https://convertingcolors.com/list/avery.html.
// Use reddish colors for different used memories, yellowish colors for different cached memories
// and greenish colors for different unused memories.
const std::array<Color, kSystemMemoryTrackDimension> kSystemMemoryTrackColors{
    Color(231, 68, 53, 255),  // red
    Color(246, 196, 0, 255),  // orange
    Color(87, 166, 74, 255)   // green
};

const std::array<Color, kCGroupAndProcessMemoryTrackDimension> kCGroupAndProcessMemoryTrackColors{
    Color(231, 68, 53, 255),   // red
    Color(249, 96, 111, 255),  // warm red
    Color(246, 196, 0, 255),   // orange
    Color(87, 166, 74, 255)    // green
};

}  // namespace orbit_gl

#endif  // ORBIT_GL_MEMORY_TRACK_H_