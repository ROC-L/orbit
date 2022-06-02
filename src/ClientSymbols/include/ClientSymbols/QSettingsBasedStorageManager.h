// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLIENT_SYMBOLS_Q_SETTINGS_BASED_STORAGE_MANAGER_H_
#define CLIENT_SYMBOLS_Q_SETTINGS_BASED_STORAGE_MANAGER_H_

#include <QCoreApplication>
#include <QSettings>
#include <QString>
#include <utility>

#include "ClientSymbols/PersistentStorageManager.h"

namespace orbit_client_symbols {

class QSettingsBasedStorageManager : public PersistentStorageManager {
 public:
  QSettingsBasedStorageManager()
      : QSettingsBasedStorageManager(QCoreApplication::organizationName(),
                                     QCoreApplication::applicationName()) {}

  QSettingsBasedStorageManager(const QString& organization, const QString& application)
      : settings_{organization, application} {}

  void SavePaths(absl::Span<const std::filesystem::path> paths) override;
  [[nodiscard]] std::vector<std::filesystem::path> LoadPaths() override;
  void SaveModuleSymbolFileMappings(const ModuleSymbolFileMappings& mappings) override;
  [[nodiscard]] ModuleSymbolFileMappings LoadModuleSymbolFileMappings() override;

 private:
  QSettings settings_;
};

}  // namespace orbit_client_symbols

#endif  // CLIENT_SYMBOLS_Q_SETTINGS_BASED_STORAGE_MANAGER_H_