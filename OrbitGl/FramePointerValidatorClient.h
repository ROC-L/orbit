// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_CORE_FRAME_POINTER_VALIDATOR_CLIENT_H_
#define ORBIT_CORE_FRAME_POINTER_VALIDATOR_CLIENT_H_

#include "OrbitModule.h"
#include "TransactionClient.h"

class OrbitApp;

// This class can be called from the UI on the client in order to validate
// whether certain modules are compiled with frame pointers.
// It will send a request to FramePointerValidatorService, to perform the
// analysis on the client.
// On a response, it will display the number of functions that have a non-valid
// prologue/epilogue as an infobox.
// TODO(kuebler): The right output format need to be discussed and decided.
class FramePointerValidatorClient {
 public:
  explicit FramePointerValidatorClient(OrbitApp* core_app,
                                       TransactionClient* transaction_client);

  FramePointerValidatorClient() = delete;
  FramePointerValidatorClient(const FramePointerValidatorClient&) = delete;
  FramePointerValidatorClient& operator=(const FramePointerValidatorClient&) =
      delete;
  FramePointerValidatorClient(FramePointerValidatorClient&&) = delete;
  FramePointerValidatorClient& operator=(FramePointerValidatorClient&&) =
      delete;

  void AnalyzeModule(uint32_t process_id,
                     const std::vector<std::shared_ptr<Module>>& modules);

 private:
  void HandleResponse(const Message& message, uint64_t id);
  OrbitApp* app_;
  TransactionClient* transaction_client_;
  absl::flat_hash_map<uint64_t, std::vector<std::shared_ptr<Module>>>
      modules_map_;
  absl::Mutex id_mutex_;
};

#endif  // ORBIT_CORE_FRAME_POINTER_VALIDATOR_CLIENT_H_
