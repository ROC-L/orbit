// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "StringManager.h"

#include <absl/container/flat_hash_map.h>
#include <absl/strings/string_view.h>
#include <absl/synchronization/mutex.h>

#include <utility>

bool StringManager::AddIfNotPresent(uint64_t key, std::string_view str) {
  absl::MutexLock lock{&mutex_};
  if (key_to_string_.contains(key)) {
    return false;
  }
  key_to_string_.emplace(key, str);
  return true;
}

bool StringManager::AddOrReplace(uint64_t key, std::string_view str) {
  absl::MutexLock lock{&mutex_};
  auto result = key_to_string_.insert_or_assign(key, str);
  return result.second;
}

std::optional<std::string> StringManager::Get(uint64_t key) const {
  absl::MutexLock lock{&mutex_};
  auto it = key_to_string_.find(key);
  if (it != key_to_string_.end()) {
    return it->second;
  } else {
    return std::optional<std::string>{};
  }
}

bool StringManager::Contains(uint64_t key) const {
  absl::MutexLock lock{&mutex_};
  return key_to_string_.contains(key);
}

void StringManager::Clear() {
  absl::MutexLock lock{&mutex_};
  key_to_string_.clear();
}
