// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ClientSymbols/QSettingsBasedStorageManager.h"

#include <QSettings>
#include <filesystem>
#include <memory>

constexpr const char* kSymbolPathsSettingsKey = "symbol_directories";
constexpr const char* kDirectoryPathKey = "directory_path";
constexpr const char* kModuleSymbolFileMappingKey = "module_symbol_file_mapping_key";
constexpr const char* kModuleSymbolFileMappingModuleKey = "module_symbol_file_mapping_module_key";
constexpr const char* kModuleSymbolFileMappingSymbolFileKey =
    "module_symbol_file_mapping_symbol_file_key";

namespace orbit_client_symbols {

std::vector<std::filesystem::path> QSettingsBasedStorageManager::LoadPaths() {
  const int size = settings_.beginReadArray(kSymbolPathsSettingsKey);
  std::vector<std::filesystem::path> paths{};
  paths.reserve(size);
  for (int i = 0; i < size; ++i) {
    settings_.setArrayIndex(i);
    paths.emplace_back(settings_.value(kDirectoryPathKey).toString().toStdString());
  }
  settings_.endArray();
  return paths;
}

void QSettingsBasedStorageManager::SavePaths(absl::Span<const std::filesystem::path> paths) {
  settings_.beginWriteArray(kSymbolPathsSettingsKey, static_cast<int>(paths.size()));
  for (size_t i = 0; i < paths.size(); ++i) {
    settings_.setArrayIndex(static_cast<int>(i));
    settings_.setValue(kDirectoryPathKey, QString::fromStdString(paths[i].string()));
  }
  settings_.endArray();
}

void QSettingsBasedStorageManager::SaveModuleSymbolFileMappings(
    const ModuleSymbolFileMappings& mappings) {
  settings_.beginWriteArray(kModuleSymbolFileMappingKey, static_cast<int>(mappings.size()));
  int index = 0;
  for (const auto& [module_path, symbol_file_path] : mappings) {
    settings_.setArrayIndex(index);
    settings_.setValue(kModuleSymbolFileMappingModuleKey, QString::fromStdString(module_path));
    settings_.setValue(kModuleSymbolFileMappingSymbolFileKey,
                       QString::fromStdString(symbol_file_path.string()));
    ++index;
  }
  settings_.endArray();
}

[[nodiscard]] ModuleSymbolFileMappings
QSettingsBasedStorageManager::LoadModuleSymbolFileMappings() {
  const int size = settings_.beginReadArray(kModuleSymbolFileMappingKey);
  ModuleSymbolFileMappings mappings{};
  mappings.reserve(size);
  for (int i = 0; i < size; ++i) {
    settings_.setArrayIndex(i);

    std::string module_path =
        settings_.value(kModuleSymbolFileMappingModuleKey).toString().toStdString();
    std::filesystem::path symbol_file_path = std::filesystem::path{
        settings_.value(kModuleSymbolFileMappingSymbolFileKey).toString().toStdString()};
    mappings[module_path] = symbol_file_path;
  }
  settings_.endArray();
  return mappings;
}

}  // namespace orbit_client_symbols