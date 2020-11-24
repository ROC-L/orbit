// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_VULKAN_LAYER_VULKAN_LAYER_PRODUCER_H_
#define ORBIT_VULKAN_LAYER_VULKAN_LAYER_PRODUCER_H_

#include "capture.pb.h"

namespace orbit_vulkan_layer {

// This interface exposes methods for the communication between the Vulkan layer and Orbit,
// while also allowing to be mocked for testing.
// In particular, it provides such methods to LayerLogic and CommandBufferManager.
class VulkanLayerProducer {
 public:
  virtual ~VulkanLayerProducer() = default;

  // This method tries to establish a gRPC connection with OrbitService over Unix domain socket
  // and gets the class ready to send CaptureEvents.
  [[nodiscard]] virtual bool BringUp(std::string_view unix_domain_socket_path) = 0;

  // This method causes the class to stop sending any remaining queued CaptureEvent
  // and closes the connection with OrbitService.
  virtual void TakeDown() = 0;

  // Use this method to query whether Orbit is currently capturing.
  [[nodiscard]] virtual bool IsCapturing() = 0;

  // Use this method to enqueue a CaptureEvent to be sent to OrbitService.
  virtual void EnqueueCaptureEvent(orbit_grpc_protos::CaptureEvent&& capture_event) = 0;

  // This method enqueues an InternedString to be sent to OrbitService the first time the string
  // passed as argument is seen. In all cases, it returns the key corresponding to the string.
  [[nodiscard]] virtual uint64_t InternStringIfNecessaryAndGetKey(std::string str) = 0;
};

}  // namespace orbit_vulkan_layer

#endif  // ORBIT_VULKAN_LAYER_VULKAN_LAYER_PRODUCER_H_
