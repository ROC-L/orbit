// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Batcher.h"
#include "BlockChain.h"
#include "CoreMath.h"
#include "CoreUtils.h"
#include "Geometry.h"
#include "PickingManager.h"
#include "PickingManagerTest.h"

namespace {

class MockBatcher : public Batcher {
 public:
  explicit MockBatcher(BatcherId id, PickingManager* picking_manager = nullptr)
      : Batcher(id, picking_manager) {}

  void ResetMockDrawCounts() {
    drawn_line_colors_.clear();
    drawn_triangle_colors_.clear();
    drawn_box_colors_.clear();
  }

  const std::vector<Color>& GetDrawnLineColors() const { return drawn_line_colors_; }
  const std::vector<Color>& GetDrawnTriangleColors() const { return drawn_triangle_colors_; }
  const std::vector<Color>& GetDrawnBoxColors() const { return drawn_box_colors_; }

  // Simulate drawing by simple appending all colors to internal
  // buffers. Only a single color per element will be appended
  // (start point for line, first vertex for triangle and box)
  void Draw(bool picking = false) const override {
    for (auto& [unused_layer, buffer] : primitive_buffers_by_layer_) {
      if (picking) {
        for (auto it = buffer.line_buffer.picking_colors_.begin();
             it != buffer.line_buffer.picking_colors_.end();) {
          drawn_line_colors_.push_back(*it);
          ++it;
          ++it;
        }
        for (auto it = buffer.triangle_buffer.picking_colors_.begin();
             it != buffer.triangle_buffer.picking_colors_.end();) {
          drawn_triangle_colors_.push_back(*it);
          ++it;
          ++it;
          ++it;
        }
        for (auto it = buffer.box_buffer.picking_colors_.begin();
             it != buffer.box_buffer.picking_colors_.end();) {
          drawn_box_colors_.push_back(*it);
          ++it;
          ++it;
          ++it;
          ++it;
        }
      } else {
        for (auto it = buffer.line_buffer.colors_.begin();
             it != buffer.line_buffer.colors_.end();) {
          drawn_line_colors_.push_back(*it);
          ++it;
          ++it;
        }
        for (auto it = buffer.triangle_buffer.colors_.begin();
             it != buffer.triangle_buffer.colors_.end();) {
          drawn_triangle_colors_.push_back(*it);
          ++it;
          ++it;
          ++it;
        }
        for (auto it = buffer.box_buffer.colors_.begin(); it != buffer.box_buffer.colors_.end();) {
          drawn_box_colors_.push_back(*it);
          ++it;
          ++it;
          ++it;
          ++it;
        }
      }
    }
  }

 private:
  mutable std::vector<Color> drawn_line_colors_;
  mutable std::vector<Color> drawn_triangle_colors_;
  mutable std::vector<Color> drawn_box_colors_;
};

void ExpectDraw(MockBatcher& batcher, uint32_t line_count, uint32_t triangle_count,
                uint32_t box_count) {
  batcher.ResetMockDrawCounts();
  batcher.Draw();
  EXPECT_EQ(batcher.GetDrawnLineColors().size(), line_count);
  EXPECT_EQ(batcher.GetDrawnTriangleColors().size(), triangle_count);
  EXPECT_EQ(batcher.GetDrawnBoxColors().size(), box_count);
}

TEST(Batcher, SimpleElementsDrawing) {
  MockBatcher batcher(BatcherId::kUi);

  ExpectDraw(batcher, 0, 0, 0);
  batcher.AddLine(Vec2(0, 0), Vec2(1, 0), 0, Color(255, 255, 255, 255));
  ExpectDraw(batcher, 1, 0, 0);
  EXPECT_EQ(batcher.GetDrawnLineColors()[0], Color(255, 255, 255, 255));
  batcher.AddTriangle(Triangle(Vec3(0, 0, 0), Vec3(0, 1, 0), Vec3(1, 0, 0)), Color(0, 255, 0, 255));
  ExpectDraw(batcher, 1, 1, 0);
  EXPECT_EQ(batcher.GetDrawnTriangleColors()[0], Color(0, 255, 0, 255));
  batcher.AddBox(Box(Vec2(0, 0), Vec2(1, 1), 0), Color(255, 0, 0, 255));
  ExpectDraw(batcher, 1, 1, 1);
  EXPECT_EQ(batcher.GetDrawnBoxColors()[0], Color(255, 0, 0, 255));
  batcher.StartNewFrame();
  ExpectDraw(batcher, 0, 0, 0);
}

TEST(Batcher, PickingElementsDrawing) {
  MockBatcher batcher(BatcherId::kUi);
  std::shared_ptr<PickableMock> pickable = std::make_shared<PickableMock>();
  PickingManager pm;

  ExpectDraw(batcher, 0, 0, 0);
  EXPECT_DEATH(batcher.AddLine(Vec2(0, 0), Vec2(1, 0), 0, Color(255, 255, 255, 255), pickable),
               "nullptr");
  batcher.SetPickingManager(&pm);
  batcher.AddLine(Vec2(0, 0), Vec2(1, 0), 0, Color(255, 255, 255, 255), pickable);
  ExpectDraw(batcher, 1, 0, 0);
  batcher.StartNewFrame();
  ExpectDraw(batcher, 0, 0, 0);
}

template <typename T>
void ExpectCustomDataEq(const MockBatcher& batcher, const Color& rendered_color, const T& value) {
  PickingId id = MockRenderPickingColor(rendered_color);
  const PickingUserData* rendered_data = batcher.GetUserData(id);
  EXPECT_NE(rendered_data, nullptr);
  EXPECT_NE(rendered_data->custom_data_, nullptr);
  EXPECT_EQ(*static_cast<const T*>(rendered_data->custom_data_), value);
}

TEST(Batcher, PickingSimpleElements) {
  MockBatcher batcher(BatcherId::kUi);

  std::string line_custom_data = "line custom data";
  auto line_user_data = std::make_unique<PickingUserData>();
  line_user_data->custom_data_ = &line_custom_data;

  std::string triangle_custom_data = "triangle custom data";
  auto triangle_user_data = std::make_unique<PickingUserData>();
  triangle_user_data->custom_data_ = &triangle_custom_data;

  std::string box_custom_data = "box custom data";
  auto box_user_data = std::make_unique<PickingUserData>();
  box_user_data->custom_data_ = &box_custom_data;

  batcher.AddLine(Vec2(0, 0), Vec2(1, 0), 0, Color(255, 255, 255, 255), std::move(line_user_data));
  batcher.AddTriangle(Triangle(Vec3(0, 0, 0), Vec3(0, 1, 0), Vec3(1, 0, 0)), Color(0, 255, 0, 255),
                      std::move(triangle_user_data));
  batcher.AddBox(Box(Vec2(0, 0), Vec2(1, 1), 0), Color(255, 0, 0, 255), std::move(box_user_data));

  batcher.Draw(true);
  ExpectCustomDataEq(batcher, batcher.GetDrawnLineColors()[0], line_custom_data);
  ExpectCustomDataEq(batcher, batcher.GetDrawnTriangleColors()[0], triangle_custom_data);
  ExpectCustomDataEq(batcher, batcher.GetDrawnBoxColors()[0], box_custom_data);
}

void ExpectPickableEq(const MockBatcher& batcher, const Color& rendered_color, PickingManager& pm,
                      const std::shared_ptr<const Pickable>& pickable) {
  PickingId id = MockRenderPickingColor(rendered_color);
  const PickingUserData* rendered_data = batcher.GetUserData(id);
  EXPECT_EQ(rendered_data, nullptr);
  EXPECT_EQ(id.type, PickingType::kPickable);
  EXPECT_EQ(pm.GetPickableFromId(id).get(), pickable.get());
}

TEST(Batcher, PickingPickables) {
  PickingManager pm;
  MockBatcher batcher(BatcherId::kUi, &pm);
  std::shared_ptr<PickableMock> line_pickable = std::make_shared<PickableMock>();
  std::shared_ptr<PickableMock> triangle_pickable = std::make_shared<PickableMock>();
  std::shared_ptr<PickableMock> box_pickable = std::make_shared<PickableMock>();

  batcher.AddLine(Vec2(0, 0), Vec2(1, 0), 0, Color(255, 255, 255, 255), line_pickable);
  batcher.AddTriangle(Triangle(Vec3(0, 0, 0), Vec3(0, 1, 0), Vec3(1, 0, 0)), Color(0, 255, 0, 255),
                      triangle_pickable);
  batcher.AddBox(Box(Vec2(0, 0), Vec2(1, 1), 0), Color(255, 0, 0, 255), box_pickable);

  batcher.Draw(true);
  ExpectPickableEq(batcher, batcher.GetDrawnLineColors()[0], pm, line_pickable);
  ExpectPickableEq(batcher, batcher.GetDrawnTriangleColors()[0], pm, triangle_pickable);
  ExpectPickableEq(batcher, batcher.GetDrawnBoxColors()[0], pm, box_pickable);
}

TEST(Batcher, MultipleDrawCalls) {
  MockBatcher batcher(BatcherId::kUi);

  std::string line_custom_data = "line custom data";
  auto line_user_data = std::make_unique<PickingUserData>();
  line_user_data->custom_data_ = &line_custom_data;

  std::string triangle_custom_data = "triangle custom data";
  auto triangle_user_data = std::make_unique<PickingUserData>();
  triangle_user_data->custom_data_ = &triangle_custom_data;

  std::string box_custom_data = "box custom data";
  auto box_user_data = std::make_unique<PickingUserData>();
  box_user_data->custom_data_ = &box_custom_data;

  batcher.AddLine(Vec2(0, 0), Vec2(1, 0), 0, Color(255, 255, 255, 255), std::move(line_user_data));
  batcher.AddTriangle(Triangle(Vec3(0, 0, 0), Vec3(0, 1, 0), Vec3(1, 0, 0)), Color(0, 255, 0, 255),
                      std::move(triangle_user_data));
  batcher.AddBox(Box(Vec2(0, 0), Vec2(1, 1), 0), Color(255, 0, 0, 255), std::move(box_user_data));

  batcher.Draw(true);

  auto line_color = batcher.GetDrawnLineColors()[0];
  auto triangle_color = batcher.GetDrawnTriangleColors()[0];
  auto box_color = batcher.GetDrawnBoxColors()[0];
  ExpectCustomDataEq(batcher, line_color, line_custom_data);
  ExpectCustomDataEq(batcher, triangle_color, triangle_custom_data);
  ExpectCustomDataEq(batcher, box_color, box_custom_data);

  batcher.ResetElements();
  ExpectCustomDataEq(batcher, line_color, line_custom_data);
  ExpectCustomDataEq(batcher, triangle_color, triangle_custom_data);
  ExpectCustomDataEq(batcher, box_color, box_custom_data);

  batcher.StartNewFrame();
  PickingId id = MockRenderPickingColor(line_color);
  const PickingUserData* rendered_data = nullptr;
  EXPECT_DEATH(rendered_data = batcher.GetUserData(id), "size");
  id = MockRenderPickingColor(triangle_color);
  EXPECT_DEATH(rendered_data = batcher.GetUserData(id), "size");
  id = MockRenderPickingColor(box_color);
  EXPECT_DEATH(rendered_data = batcher.GetUserData(id), "size");
  UNUSED(rendered_data);
}

}  // namespace