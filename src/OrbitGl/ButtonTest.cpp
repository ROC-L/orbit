// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "Button.h"
#include "CaptureViewElementTester.h"
#include "GlCanvas.h"

namespace orbit_gl {

TEST(Button, CaptureViewElementWorksAsIntended) {
  orbit_gl::CaptureViewElementTester tester;
  Button button(nullptr, tester.GetViewport(), tester.GetLayout());

  tester.RunTests(&button);
}

TEST(Button, SizeGettersAndSettersWork) {
  orbit_gl::CaptureViewElementTester tester;
  Button button(nullptr, tester.GetViewport(), tester.GetLayout());

  Vec2 size(10.f, 10.f);
  button.SetWidth(size[0]);
  button.SetHeight(size[1]);

  tester.SimulateDrawLoopAndCheckFlags(&button, true, true);

  EXPECT_EQ(button.GetWidth(), size[0]);
  EXPECT_EQ(button.GetHeight(), size[1]);
  EXPECT_EQ(button.GetSize(), size);

  // Setting width / height to the same values should not request an update
  button.SetWidth(size[0]);
  button.SetHeight(size[1]);

  tester.SimulateDrawLoopAndCheckFlags(&button, false, false);

  // Changing to a different size should affect the property and flags again
  size = Vec2(15.f, 20.f);
  button.SetWidth(size[0]);
  button.SetHeight(size[1]);

  tester.SimulateDrawLoopAndCheckFlags(&button, true, true);

  EXPECT_EQ(button.GetWidth(), size[0]);
  EXPECT_EQ(button.GetHeight(), size[1]);
  EXPECT_EQ(button.GetSize(), size);
}

TEST(Button, SizeCannotBeZero) {
  orbit_gl::CaptureViewElementTester tester;
  Button button(nullptr, tester.GetViewport(), tester.GetLayout());

  button.SetWidth(0.f);
  button.SetHeight(0.f);

  tester.SimulatePreRender(&button);

  EXPECT_EQ(button.GetWidth(), tester.GetLayout()->GetMinButtonSize());
  EXPECT_EQ(button.GetHeight(), tester.GetLayout()->GetMinButtonSize());
}

TEST(Button, LabelWorksAsExpected) {
  orbit_gl::CaptureViewElementTester tester;
  Button button(nullptr, tester.GetViewport(), tester.GetLayout());

  tester.SimulateDrawLoopAndCheckFlags(&button, true, true);

  const std::string kLabel = "UnitTest";
  button.SetLabel(kLabel);

  tester.SimulateDrawLoopAndCheckFlags(&button, true, false);

  EXPECT_EQ(button.GetLabel(), kLabel);
}

TEST(Button, MouseReleaseCallback) {
  orbit_gl::CaptureViewElementTester tester;
  Button button(nullptr, tester.GetViewport(), tester.GetLayout());

  uint32_t mouse_released_called = 0;
  auto callback = [&](Button* button_param) {
    if (button_param == &button) mouse_released_called++;
  };
  button.SetMouseReleaseCallback(callback);

  uint32_t mouse_released_called_expected = 0;
  button.OnRelease();
  EXPECT_EQ(mouse_released_called, ++mouse_released_called_expected);
  button.OnRelease();
  EXPECT_EQ(mouse_released_called, ++mouse_released_called_expected);
  button.SetMouseReleaseCallback(nullptr);
  button.OnRelease();
  EXPECT_EQ(mouse_released_called, mouse_released_called_expected);
}

TEST(Button, Rendering) {
  orbit_gl::CaptureViewElementTester tester;
  Button button(nullptr, tester.GetViewport(), tester.GetLayout());

  button.SetLabel("Test");
  const Vec2 kSize(400, 50);
  const Vec2 kPos(10, 10);
  button.SetWidth(kSize[0]);
  button.SetHeight(kSize[1]);
  button.SetPos(kPos[0], kPos[1]);

  tester.SimulateDrawLoop(&button, true, false);

  const MockBatcher& batcher = tester.GetBatcher();
  const MockTextRenderer& text_renderer = tester.GetTextRenderer();

  // I don't really care how the button is represented, but I expect at least a single box to be
  // drawn.
  EXPECT_GE(batcher.GetNumBoxes(), 1);
  EXPECT_TRUE(batcher.IsEverythingInsideRectangle(kPos, kSize));
  EXPECT_TRUE(batcher.IsEverythingBetweenZLayers(GlCanvas::kZValueUi, GlCanvas::kZValueUi));

  EXPECT_EQ(text_renderer.GetNumAddTextCalls(), 1);
  EXPECT_TRUE(text_renderer.AreAddTextsAlignedVertically());
  EXPECT_TRUE(text_renderer.IsTextInsideRectangle(kPos, kSize));
  EXPECT_TRUE(text_renderer.IsTextBetweenZLayers(GlCanvas::kZValueUi, GlCanvas::kZValueUi));

  tester.SimulateDrawLoop(&button, false, true);
  // Verify that `UpdatePrimitives` has no effect - all rendering of the button should be done in
  // `Draw`
  EXPECT_EQ(batcher.GetNumElements(), 0);
  EXPECT_EQ(text_renderer.GetNumAddTextCalls(), 0);

  // Make sure a small text is cut to the button extents
  const Vec2 kSmallSize = Vec2(tester.GetLayout()->GetMinButtonSize(), kSize[1]);
  button.SetWidth(kSmallSize[0]);
  tester.SimulateDrawLoop(&button, true, false);
  EXPECT_TRUE(text_renderer.IsTextInsideRectangle(kPos, kSmallSize));
}

}  // namespace orbit_gl