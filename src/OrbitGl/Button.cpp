// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "OrbitGl/Button.h"

#include <GteVector.h>

#include <algorithm>
#include <utility>

#include "OrbitBase/Logging.h"
#include "OrbitGl/AccessibleButton.h"
#include "OrbitGl/CoreMath.h"
#include "OrbitGl/Geometry.h"
#include "OrbitGl/GlCanvas.h"

// TODO (b/230726102): Code below is copied from GlSlider. This should live in a central place.
namespace {
constexpr float kGradientFactor = 0.25f;

[[nodiscard]] Color GetLighterColor(const Color& color) {
  const float kLocalGradientFactor = 1.0f + kGradientFactor;
  return Color(static_cast<unsigned char>(color[0] * kLocalGradientFactor),
               static_cast<unsigned char>(color[1] * kLocalGradientFactor),
               static_cast<unsigned char>(color[2] * kLocalGradientFactor), 255);
}

[[nodiscard]] Color GetDarkerColor(const Color& color) {
  const float kLocalGradientFactor = 1.0f - kGradientFactor;
  return Color(static_cast<unsigned char>(color[0] * kLocalGradientFactor),
               static_cast<unsigned char>(color[1] * kLocalGradientFactor),
               static_cast<unsigned char>(color[2] * kLocalGradientFactor), 255);
}
}  // namespace

namespace orbit_gl {

Button::Button(CaptureViewElement* parent, const Viewport* viewport, const TimeGraphLayout* layout,
               std::string name, SymbolType symbol_type)
    : CaptureViewElement(parent, viewport, layout),
      name_(std::move(name)),
      symbol_type_{symbol_type} {
  SetWidth(layout->GetMinButtonSize());
  SetHeight(layout->GetMinButtonSize());
}

CaptureViewElement::EventResult Button::OnMouseEnter() {
  EventResult event_result = CaptureViewElement::OnMouseEnter();
  RequestUpdate(RequestUpdateScope::kDraw);
  return event_result;
}

CaptureViewElement::EventResult Button::OnMouseLeave() {
  EventResult event_result = CaptureViewElement::OnMouseLeave();
  RequestUpdate(RequestUpdateScope::kDraw);
  return event_result;
}

void Button::SetHeight(float height) {
  if (height == height_) return;

  height_ = height;
  RequestUpdate();
}

void Button::SetMouseReleaseCallback(MouseReleaseCallback callback) {
  mouse_release_callback_ = std::move(callback);
}

void Button::OnRelease() {
  CaptureViewElement::OnRelease();
  if (!IsMouseOver()) return;

  if (mouse_release_callback_ != nullptr) {
    mouse_release_callback_(this);
  }
}

void Button::DoUpdateLayout() {
  CaptureViewElement::DoUpdateLayout();

  SetWidth(std::max(GetWidth(), layout_->GetMinButtonSize()));
  SetHeight(std::max(GetHeight(), layout_->GetMinButtonSize()));
}

void Button::DoDraw(PrimitiveAssembler& primitive_assembler, TextRenderer& /*text_renderer*/,
                    const DrawContext& /*draw_context*/) {
  const float z = GlCanvas::kZValueButton;
  const Vec2 pos = GetPos();
  const Vec2 size = GetSize();

  const Color kHighlightColor(75, 75, 75, 255);
  const Color kBaseColor(68, 68, 68, 255);
  const Color kDarkBorderColor = GetDarkerColor(kBaseColor);
  const Color kLightBorderColor = GetLighterColor(kBaseColor);

  const Vec2 kBorderSize = Vec2(1.f, 1.f);

  Vec2 pos_w_border = pos;
  Vec2 size_w_border = size;

  // Dark border
  primitive_assembler.AddBox(MakeBox(pos_w_border, size_w_border), z, kDarkBorderColor,
                             shared_from_this());
  pos_w_border += kBorderSize;
  size_w_border -= kBorderSize + kBorderSize;

  // Light border
  primitive_assembler.AddBox(MakeBox(pos_w_border, size_w_border), z, kLightBorderColor,
                             shared_from_this());
  pos_w_border += kBorderSize;
  size_w_border -= kBorderSize + kBorderSize;

  // Button itself
  const Color slider_color = IsMouseOver() ? kHighlightColor : kBaseColor;
  primitive_assembler.AddShadedBox(pos_w_border, size_w_border, z, slider_color, shared_from_this(),
                                   ShadingDirection::kTopToBottom);
  DrawSymbol(primitive_assembler);
}

void Button::DrawSymbol(PrimitiveAssembler& primitive_assembler) {
  const Color kSymbolBaseColor(191, 191, 192, 255);
  const Color kSymbolHighlightColor(255, 255, 255, 255);
  // Symbol width and the padding are related to the size of the button, so they can scale
  // proportionally if the button size changes.
  const float symbol_padding_size = GetWidth() / 5.f;
  const float symbol_width = GetWidth() / 5.f;

  Color symbol_color = IsMouseOver() ? kSymbolHighlightColor : kSymbolBaseColor;

  switch (symbol_type_) {
    case SymbolType::kNoSymbol:
      break;
    case SymbolType::kPlusSymbol:
      primitive_assembler.AddBox(MakeBox({GetPos()[0] + (GetWidth() - symbol_width) / 2.f,
                                          GetPos()[1] + symbol_padding_size},
                                         {symbol_width, GetHeight() - 2 * symbol_padding_size}),
                                 GlCanvas::kZValueButton, symbol_color, shared_from_this());
      [[fallthrough]];
    case SymbolType::kMinusSymbol:
      primitive_assembler.AddBox(MakeBox({GetPos()[0] + symbol_padding_size,
                                          GetPos()[1] + (GetHeight() - symbol_width) / 2.f},
                                         {GetWidth() - 2 * symbol_padding_size, symbol_width}),
                                 GlCanvas::kZValueButton, symbol_color, shared_from_this());
      break;
    default:
      ORBIT_UNREACHABLE();
  }
}

std::unique_ptr<orbit_accessibility::AccessibleInterface> Button::CreateAccessibleInterface() {
  return std::make_unique<AccessibleButton>(this);
}

}  // namespace orbit_gl