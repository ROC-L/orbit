// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "QueueManager.h"
#include "gtest/gtest.h"

#define UNUSED(x) (void)(x)

namespace orbit_vulkan_layer {

TEST(QueueManager, AnNonTrackedQueueCanNotBeRetrieved) {
  QueueManager manager;
  VkQueue queue = nullptr;
  EXPECT_DEATH(
      {
        VkDevice device = manager.GetDeviceOfQueue(queue);
        UNUSED(device);
      },
      "");
}

TEST(QueueManager, AQueueCanBeTrackedAndRetrieved) {
  QueueManager manager;
  VkDevice device = nullptr;
  VkQueue queue = nullptr;

  manager.TrackQueue(queue, device);
  VkDevice result = manager.GetDeviceOfQueue(queue);
  EXPECT_EQ(device, result);
}

}  // namespace orbit_vulkan_layer
