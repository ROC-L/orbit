// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ApiEventIdProvider.h"

#include <algorithm>
#include <cstdint>
#include <memory>

namespace orbit_gl {

[[nodiscard]] std::unique_ptr<NameEqualityApiEventIdProvider>
NameEqualityApiEventIdProvider::Create(const orbit_grpc_protos::CaptureOptions& capture_options) {
  const auto& instrumented_functions = capture_options.instrumented_functions();
  const uint64_t max_id =
      instrumented_functions.empty()
          ? 0
          : std::max_element(
                std::begin(instrumented_functions), std::end(instrumented_functions),
                [](const auto& a, const auto& b) { return a.function_id() < b.function_id(); })
                ->function_id();
  return std::unique_ptr<NameEqualityApiEventIdProvider>(
      new NameEqualityApiEventIdProvider(max_id + 1));
}

NameEqualityApiEventIdProvider::NameEqualityApiEventIdProvider(uint64_t start_id)
    : next_id_(start_id) {}

[[nodiscard]] uint64_t NameEqualityApiEventIdProvider::ProvideId(const TimerInfo& timer_info) {
  const auto key = make_pair(timer_info.type(), timer_info.api_scope_name());

  const auto it = name_to_id_.find(key);
  if (it != name_to_id_.end()) {
    return it->second;
  }

  name_to_id_[key] = next_id_;
  uint64_t id = next_id_;
  next_id_++;
  return id;
}

}  // namespace orbit_gl