// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "DeploymentConfigurations.h"

#include <QChar>
#include <QCharRef>
#include <QCoreApplication>
#include <QDir>
#include <QString>

static const char* const kSignatureExtension = ".asc";
static const char* const kPackageNameTemplate = "OrbitProfiler-%1.deb";
static const char* const kCollectorSubdirectory = "collector";

namespace orbit_qt {

SignedDebianPackageDeployment::SignedDebianPackageDeployment() {
  const auto version = []() {
    auto ver = QCoreApplication::applicationVersion();
    if (!ver.isEmpty() && ver[0] == 'v') {
      ver = ver.mid(1);
    }
    return ver;
  }();

  const auto collector_directory =
      QDir{QDir{QCoreApplication::applicationDirPath()}.absoluteFilePath(kCollectorSubdirectory)};

  const auto deb_path =
      collector_directory.absoluteFilePath(QString(kPackageNameTemplate).arg(version));

  path_to_package = deb_path.toStdString();
  path_to_signature = (deb_path + kSignatureExtension).toStdString();
}

}  // namespace orbit_qt
