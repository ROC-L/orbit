// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SessionSetup/ConnectToTargetDialog.h"

#include <QApplication>
#include <QMessageBox>
#include <algorithm>
#include <memory>

#include "ClientServices/ProcessClient.h"
#include "OrbitBase/JoinFutures.h"
#include "OrbitBase/Logging.h"
#include "SessionSetup/ConnectToTargetDialog.h"
#include "SessionSetup/ServiceDeployManager.h"
#include "SessionSetup/SessionSetupUtils.h"
#include "grpcpp/channel.h"
#include "ui_ConnectToTargetDialog.h"

namespace orbit_session_setup {

ConnectToTargetDialog::ConnectToTargetDialog(
    SshConnectionArtifacts* ssh_connection_artifacts, const ConnectionTarget& target,
    orbit_metrics_uploader::MetricsUploader* metrics_uploader, QWidget* parent)
    : QDialog{parent, Qt::Window},
      ui_(std::make_unique<Ui::ConnectToTargetDialog>()),
      ssh_connection_artifacts_(ssh_connection_artifacts),
      target_(target),
      metrics_uploader_(metrics_uploader),
      main_thread_executor_(orbit_qt_utils::MainThreadExecutorImpl::Create()) {
  ORBIT_CHECK(ssh_connection_artifacts != nullptr);

  ui_->setupUi(this);
  ui_->instanceIdLabel->setText(target_.instance_name_or_id);
  ui_->processIdLabel->setText(target_.process_name_or_path);
}

ConnectToTargetDialog::~ConnectToTargetDialog() {}

std::optional<TargetConfiguration> ConnectToTargetDialog::Exec() {
  ORBIT_LOG("Trying to establish a connection to process \"%s\" on instance \"%s\"",
            target_.process_name_or_path.toStdString(), target_.instance_name_or_id.toStdString());

  auto ggp_client_result = orbit_ggp::CreateClient();
  if (ggp_client_result.has_error()) {
    LogAndDisplayError(ggp_client_result.error());
    return std::nullopt;
  }
  ggp_client_ = std::move(ggp_client_result.value());

  SetStatusMessage("Loading encryption credentials for instance...");

  auto process_future = ggp_client_->GetSshInfoAsync(target_.instance_name_or_id, std::nullopt);
  auto instance_future = ggp_client_->DescribeInstanceAsync(target_.instance_name_or_id);
  auto joined_future = orbit_base::JoinFutures(process_future, instance_future);

  joined_future.Then(main_thread_executor_.get(),
                     [this](MaybeSshAndInstanceData ssh_instance_data) {
                       OnAsyncDataAvailable(std::move(ssh_instance_data));
                     });

  int rc = QDialog::exec();

  if (rc != QDialog::Accepted) {
    return std::nullopt;
  }

  return std::move(target_configuration_);
}

void ConnectToTargetDialog::OnAsyncDataAvailable(MaybeSshAndInstanceData ssh_instance_data) {
  auto maybe_ssh_info = std::get<0>(ssh_instance_data);

  if (maybe_ssh_info.has_error()) {
    LogAndDisplayError(maybe_ssh_info.error());
    reject();
    return;
  }
  orbit_ggp::SshInfo ssh_info = std::move(maybe_ssh_info.value());

  auto maybe_instance_data = std::get<1>(ssh_instance_data);

  if (maybe_instance_data.has_error()) {
    LogAndDisplayError(maybe_instance_data.error());
    reject();
    return;
  }
  orbit_ggp::Instance instance = std::move(maybe_instance_data.value());

  auto service_deploy_manager = std::make_unique<orbit_session_setup::ServiceDeployManager>(
      ssh_connection_artifacts_->GetDeploymentConfiguration(),
      ssh_connection_artifacts_->GetSshContext(), CredentialsFromSshInfo(ssh_info),
      ssh_connection_artifacts_->GetGrpcPort());

  auto maybe_grpc_port = DeployOrbitService(service_deploy_manager.get());

  if (maybe_grpc_port.has_error()) {
    LogAndDisplayError(maybe_grpc_port.error());
    reject();
    return;
  }

  auto grpc_channel = CreateGrpcChannel(maybe_grpc_port.value().grpc_port);
  stadia_connection_ = orbit_session_setup::StadiaConnection(
      std::move(instance), std::move(service_deploy_manager), std::move(grpc_channel));

  process_manager_ = orbit_client_services::ProcessManager::Create(
      stadia_connection_.value().GetGrpcChannel(), absl::Milliseconds(1000));
  process_manager_->SetProcessListUpdateListener(
      [this](std::vector<orbit_grpc_protos::ProcessInfo> process_list) {
        OnProcessListUpdate(std::move(process_list));
      });
  SetStatusMessage("Waiting for process to launch.");
}

void ConnectToTargetDialog::OnProcessListUpdate(
    std::vector<orbit_grpc_protos::ProcessInfo> process_list) {
  std::unique_ptr<orbit_client_data::ProcessData> matching_process =
      TryToFindProcessData(process_list, target_.process_name_or_path.toStdString());

  if (matching_process != nullptr) {
    process_manager_->SetProcessListUpdateListener(nullptr);
    target_configuration_ =
        orbit_session_setup::StadiaTarget(std::move(stadia_connection_.value()),
                                          std::move(process_manager_), std::move(matching_process));
    accept();
  }
}

ErrorMessageOr<orbit_session_setup::ServiceDeployManager::GrpcPort>
ConnectToTargetDialog::DeployOrbitService(
    orbit_session_setup::ServiceDeployManager* service_deploy_manager) {
  orbit_ssh_qt::ScopedConnection label_connection{
      QObject::connect(service_deploy_manager, &ServiceDeployManager::statusMessage, this,
                       &ConnectToTargetDialog::SetStatusMessage)};
  orbit_ssh_qt::ScopedConnection cancel_connection{
      QObject::connect(ui_->abortButton, &QPushButton::clicked, service_deploy_manager,
                       &ServiceDeployManager::Cancel)};

  auto deployment_result = service_deploy_manager->Exec();
  if (deployment_result.has_error()) {
    return ErrorMessage{deployment_result.error().message()};
  } else {
    return deployment_result.value();
  }
}

void ConnectToTargetDialog::SetStatusMessage(const QString& message) {
  ui_->statusLabel->setText(QString("<b>Status:</b> ") + message);
}

void ConnectToTargetDialog::LogAndDisplayError(const ErrorMessage& message) {
  ORBIT_ERROR("%s", message.message());
  QMessageBox::critical(nullptr, QApplication::applicationName(),
                        QString::fromStdString(message.message()));
}

}  // namespace orbit_session_setup