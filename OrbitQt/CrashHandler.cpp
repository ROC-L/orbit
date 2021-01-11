// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CrashHandler.h"

#include <base/files/file_path.h>
#include <client/crash_report_database.h>
#include <util/misc/capture_context.h>

#include <algorithm>
#include <map>

#include "CoreUtils.h"
#include "OrbitBase/Logging.h"
#include "OrbitVersion/OrbitVersion.h"
#include "client/settings.h"

namespace {
template <typename StringType = base::FilePath::StringType>
struct StringTypeConverter {
  StringType operator()(const std::string& source_string) const {
    return StringType(source_string);
  }
};

template <>
struct StringTypeConverter<std::wstring> {
  std::wstring operator()(const std::string& source_string) const { return s2ws(source_string); }
};
}  // namespace

CrashHandler::CrashHandler(const std::string& dump_path, const std::string& handler_path,
                           const std::string& crash_server_url,
                           const std::vector<std::string>& attachments) {
  CHECK(!is_init_);
  is_init_ = true;

  // Creates a new CrashpadClient instance that directs crashes to crashpad
  // handler. Minidump files will be written to dump_path and sent to
  // crash_server.

  const base::FilePath dump_file_path(StringTypeConverter<>()(dump_path));
  const base::FilePath handler_file_path(StringTypeConverter<>()(handler_path));

  const std::map<std::string, std::string> annotations = {{"product", "OrbitProfiler"},
                                                          {"version", orbit_core::GetVersion()}};

  const std::vector<std::string> arguments = {"--no-rate-limit"};

  std::vector<base::FilePath> attachments_paths;
  for (const std::string& attachment : attachments) {
    base::FilePath attachment_path(StringTypeConverter<>()(attachment));
    attachments_paths.push_back(attachment_path);
  }

  crash_report_db_ = crashpad::CrashReportDatabase::Initialize(dump_file_path);
  SetUploadsEnabled(true);

  crashpad_client_.StartHandler(handler_file_path,
                                /*database=*/dump_file_path,
                                /*metrics_dir=*/dump_file_path, crash_server_url, annotations,
                                arguments,
                                /*restartable=*/true,
                                /*asynchronous_start=*/false, attachments_paths);
}

void CrashHandler::SetUploadsEnabled(bool is_upload_enabled) {
  // set user preferences for dumps submission to collection server
  if (crash_report_db_ != nullptr) {
    crashpad::Settings* db_settings = crash_report_db_->GetSettings();
    if (db_settings != nullptr) {
      db_settings->SetUploadsEnabled(is_upload_enabled);
    }
  }
}

void CrashHandler::DumpWithoutCrash() const {
  crashpad::NativeCPUContext cpu_context;
  crashpad::CaptureContext(&cpu_context);
#ifdef _WIN32
  crashpad_client_.DumpWithoutCrash(cpu_context);
#else
  crashpad_client_.DumpWithoutCrash(&cpu_context);
#endif
}
