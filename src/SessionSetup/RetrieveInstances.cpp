// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SessionSetup/RetrieveInstances.h"

#include <absl/strings/match.h>
#include <absl/strings/str_join.h>
#include <absl/types/span.h>

#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>

#include "MainThreadExecutor.h"
#include "OrbitBase/Future.h"
#include "OrbitBase/FutureHelpers.h"
#include "OrbitBase/JoinFutures.h"
#include "OrbitBase/Result.h"

namespace orbit_session_setup {

using orbit_base::Future;
using orbit_ggp::Client;
using orbit_ggp::Instance;
using orbit_ggp::Project;

RetrieveInstances::RetrieveInstances(Client* ggp_client, MainThreadExecutor* main_thread_executor,
                                     QObject* parent)
    : QObject(parent), ggp_client_(ggp_client), main_thread_executor_(main_thread_executor) {
  CHECK(ggp_client != nullptr);
  CHECK(main_thread_executor != nullptr);
}

Future<ErrorMessageOr<QVector<Instance>>> RetrieveInstances::LoadInstances(
    const std::optional<Project>& project, orbit_ggp::Client::InstanceListScope scope) {
  auto key = std::make_pair(project, scope);
  if (instance_cache_.contains(key)) {
    return Future<ErrorMessageOr<QVector<Instance>>>{instance_cache_.at(key)};
  }
  return LoadInstancesWithoutCache(project, scope);
}

Future<ErrorMessageOr<QVector<Instance>>> RetrieveInstances::LoadInstancesWithoutCache(
    const std::optional<Project>& project, orbit_ggp::Client::InstanceListScope scope) {
  auto future = ggp_client_->GetInstancesAsync(scope, project);
  (void)future.ThenIfSuccess(main_thread_executor_, [this, key = std::make_pair(project, scope)](
                                                        QVector<Instance> instances) {
    instance_cache_.emplace(key, instances);
  });
  return future;
}

Future<ErrorMessageOr<RetrieveInstances::LoadProjectsAndInstancesResult>>
RetrieveInstances::LoadProjectsAndInstances(const std::optional<orbit_ggp::Project>& project,
                                            orbit_ggp::Client::InstanceListScope scope) {
  projects_and_instances_load_result_ = LoadProjectsAndInstancesResult{};

  const Future<ErrorMessageOr<void>> project_list_future =
      ggp_client_->GetProjectsAsync().ThenIfSuccess(
          main_thread_executor_, [this](QVector<Project> projects) -> ErrorMessageOr<void> {
            projects_and_instances_load_result_.projects = std::move(projects);
            return outcome::success();
          });

  const Future<ErrorMessageOr<void>> default_project_future =
      ggp_client_->GetDefaultProjectAsync().ThenIfSuccess(
          main_thread_executor_, [this](Project default_project) -> ErrorMessageOr<void> {
            projects_and_instances_load_result_.default_project = std::move(default_project);
            return outcome::success();
          });

  const Future<ErrorMessageOr<void>> instance_list_future = orbit_base::UnwrapFuture(
      ggp_client_->GetInstancesAsync(scope, project)
          .Then(main_thread_executor_,
                [this, scope, project](
                    ErrorMessageOr<QVector<Instance>> instances) -> Future<ErrorMessageOr<void>> {
                  if (instances.has_value()) {
                    projects_and_instances_load_result_.instances = instances.value();
                    projects_and_instances_load_result_.instances_project = project;
                    return Future<ErrorMessageOr<void>>{outcome::success()};
                  }
                  if (project == std::nullopt ||
                      !absl::StrContains(instances.error().message(), "it may not exist")) {
                    return {instances.error()};
                  }
                  // If the error message contains "it may not exist", then the project may not
                  // exist anymore and the call to ggp is retried without a project (default
                  // project)
                  return ggp_client_->GetInstancesAsync(scope, std::nullopt)
                      .ThenIfSuccess(
                          main_thread_executor_,
                          [this](QVector<Instance> instances) -> ErrorMessageOr<void> {
                            projects_and_instances_load_result_.instances = std::move(instances);
                            projects_and_instances_load_result_.instances_project = std::nullopt;
                            return outcome::success();
                          });
                }));

  Future<std::vector<ErrorMessageOr<void>>> combined_future = orbit_base::JoinFutures(
      absl::MakeConstSpan({project_list_future, default_project_future, instance_list_future}));

  return combined_future.Then(
      main_thread_executor_,
      [this](std::vector<ErrorMessageOr<void>> result_vector)
          -> ErrorMessageOr<LoadProjectsAndInstancesResult> {
        std::vector<ErrorMessageOr<void>> collected_errors;
        std::copy_if(result_vector.begin(), result_vector.end(),
                     std::back_inserter(collected_errors),
                     [](const ErrorMessageOr<void>& result) { return result.has_error(); });

        if (collected_errors.empty()) {
          return projects_and_instances_load_result_;
        }

        std::vector<std::string> error_messages;
        std::transform(collected_errors.begin(), collected_errors.end(),
                       std::back_inserter(error_messages),
                       [](ErrorMessageOr<void> error) { return error.error().message(); });

        std::string combined_error_messages = absl::StrJoin(error_messages, "\n");
        return ErrorMessage{
            absl::StrFormat("The following error occured:\n%s", combined_error_messages)};
      });
}

}  // namespace orbit_session_setup