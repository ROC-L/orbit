// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_GGP_CLIENT_H_
#define ORBIT_GGP_CLIENT_H_

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "OrbitBase/Future.h"
#include "OrbitBase/NotFoundOr.h"
#include "OrbitBase/Result.h"
#include "OrbitGgp/Account.h"
#include "OrbitGgp/Instance.h"
#include "OrbitGgp/Project.h"
#include "OrbitGgp/SshInfo.h"
#include "OrbitGgp/SymbolDownloadInfo.h"

namespace orbit_ggp {

constexpr const char* kDefaultGgpProgram{"ggp"};

class Client {
 public:
  /*
    InstancesListScope describes the scope of the instance list command.
    - kOnlyOwnInstances means only the users owned instances are returned;
    - kAllReservedInstances means all reserved instances.
  */
  enum class InstanceListScope { kOnlyOwnInstances, kAllReservedInstances };

  Client() = default;
  virtual ~Client() = default;
  [[nodiscard]] virtual orbit_base::Future<ErrorMessageOr<QVector<Instance>>> GetInstancesAsync(
      InstanceListScope scope, std::optional<Project> project) = 0;
  [[nodiscard]] virtual orbit_base::Future<ErrorMessageOr<QVector<Instance>>> GetInstancesAsync(
      InstanceListScope scope, std::optional<Project> project, int retry) = 0;
  [[nodiscard]] virtual orbit_base::Future<ErrorMessageOr<SshInfo>> GetSshInfoAsync(
      const QString& instance_id, std::optional<Project> project) = 0;
  [[nodiscard]] virtual orbit_base::Future<ErrorMessageOr<SshInfo>> GetSshInfoAsync(
      const QString& instance_id, std::optional<Project> project, int retry) = 0;
  [[nodiscard]] virtual orbit_base::Future<ErrorMessageOr<QVector<Project>>> GetProjectsAsync() = 0;
  [[nodiscard]] virtual orbit_base::Future<ErrorMessageOr<Project>> GetDefaultProjectAsync() = 0;
  [[nodiscard]] virtual orbit_base::Future<ErrorMessageOr<Instance>> DescribeInstanceAsync(
      const QString& instance_id) = 0;
  [[nodiscard]] virtual orbit_base::Future<ErrorMessageOr<Account>> GetDefaultAccountAsync() = 0;

  struct SymbolDownloadQuery {
    std::string module_name;
    std::string build_id;
  };
  [[nodiscard]] virtual orbit_base::Future<
      ErrorMessageOr<orbit_base::NotFoundOr<SymbolDownloadInfo>>>
  GetSymbolDownloadInfoAsync(const SymbolDownloadQuery& symbol_download_query) = 0;
};

[[nodiscard]] std::chrono::milliseconds GetClientDefaultTimeoutInMs();
ErrorMessageOr<std::unique_ptr<Client>> CreateClient(
    QString ggp_program = kDefaultGgpProgram,
    std::chrono::milliseconds timeout = GetClientDefaultTimeoutInMs());

}  // namespace orbit_ggp

#endif  // ORBIT_GGP_CLIENT_H_
