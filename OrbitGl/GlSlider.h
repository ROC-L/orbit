// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <functional>

#include "Batcher.h"
#include "Params.h"
#include "PickingManager.h"
#include "TextBox.h"

class GlCanvas;

class GlSlider : public Pickable, public std::enable_shared_from_this<GlSlider> {
 public:
  GlSlider();
  ~GlSlider(){};

  [[nodiscard]] bool Draggable() override { return true; }

  void SetCanvas(GlCanvas* canvas) { canvas_ = canvas; }
  void SetSliderRatio(float start);  // [0,1]

  void SetSliderWidthRatio(float ratio);  // [0,1]
  [[nodiscard]] Color GetBarColor() const { return slider_color_; }
  void SetPixelHeight(float height) { pixel_height_ = height; }
  [[nodiscard]] float GetPixelHeight() const { return pixel_height_; }

  void SetOrthogonalSliderSize(float size) { orthogonal_slider_size_ = size; }
  [[nodiscard]] float GetOrthogonalSliderSize() { return orthogonal_slider_size_; }

  typedef std::function<void(float)> DragCallback;
  void SetDragCallback(DragCallback callback) { drag_callback_ = callback; }

 protected:
  static Color GetLighterColor(const Color& color);
  static Color GetDarkerColor(const Color& color);

  void DrawBackground(GlCanvas* canvas, float x, float y, float width, float height);
  void DrawSlider(GlCanvas* canvas, float x, float y, float width, float height,
                  ShadingDirection shading_direction);

 protected:
  static const float kGradientFactor;

  GlCanvas* canvas_;
  float ratio_;
  float length_;
  float picking_ratio_;
  DragCallback drag_callback_;
  Color selected_color_;
  Color slider_color_;
  Color bar_color_;
  float min_slider_pixel_width_;
  float pixel_height_;
  float orthogonal_slider_size_;
};

class GlVerticalSlider : public GlSlider {
 public:
  GlVerticalSlider() : GlSlider(){};
  ~GlVerticalSlider(){};

  void OnPick(int x, int y) override;
  void OnDrag(int x, int y) override;
  void Draw(GlCanvas* canvas, PickingMode picking_mode) override;
};

class GlHorizontalSlider : public GlSlider {
 public:
  GlHorizontalSlider() : GlSlider(){};
  ~GlHorizontalSlider(){};

  void OnPick(int x, int y) override;
  void OnDrag(int x, int y) override;
  void Draw(GlCanvas* canvas, PickingMode picking_mode) override;
};