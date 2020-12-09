// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_GGP_SSH_INFO_H_
#define ORBIT_GGP_SSH_INFO_H_

#include <QByteArray>
#include <QString>
#include <outcome.hpp>

namespace orbit_ggp {

struct SshInfo {
  QString host;
  QString key_path;
  QString known_hosts_path;
  int port;
  QString user;

  static outcome::result<SshInfo> CreateFromJson(const QByteArray& json);
};

}  // namespace orbit_ggp

#endif  // ORBIT_GGP_SSH_INFO_H_
