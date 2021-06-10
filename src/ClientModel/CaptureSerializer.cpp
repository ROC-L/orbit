// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ClientModel/CaptureSerializer.h"

#include <absl/strings/str_cat.h>

#include <filesystem>
#include <string_view>
#include "CoreUtils.h"

namespace orbit_client_model {

namespace {
inline constexpr std::string_view kFileOrbitExtension = ".orbit";
}

namespace capture_serializer {

std::string GenerateCaptureFileName(std::string_view process_name, absl::Time time,
                                    std::string_view suffix) {
  return absl::StrCat(std::filesystem::path(process_name).stem().string(), "_",
                      orbit_core::FormatTime(time), suffix, kFileOrbitExtension);
}

void IncludeOrbitExtensionInFile(std::string& file_name) {
  const std::string extension = std::filesystem::path(file_name).extension().string();
  if (extension != kFileOrbitExtension) {
    file_name.append(kFileOrbitExtension);
  }
}

}  // namespace capture_serializer

}  // namespace orbit_client_model
