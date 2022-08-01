// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

int GgpVersion(int argc, char* argv[]) {
  if (argc != 2) {
    std::cout << "Wrong amount of arguments" << std::endl;
    return 1;
  }
  if (std::string_view{argv[1]} != "version") {
    std::cout << "arguments are formatted wrong" << std::endl;
    return 1;
  }

  std::cout << "SDK Version:  25157.1.74.0" << std::endl;
  return 0;
}

int GgpInstanceList(int argc, char* argv[]) {
  if (argc < 4 || argc > 7) {
    std::cout << "Wrong amount of arguments" << std::endl;
    return 1;
  }
  if (std::string_view{argv[1]} != "instance" || std::string_view{argv[2]} != "list" ||
      std::string_view{argv[3]} != "-s") {
    std::cout << "arguments are formatted wrong" << std::endl;
    return 1;
  }

  if (argc == 5 && std::string_view{argv[4]} != "--all-reserved") {
    std::cout << "arguments are formatted wrong" << std::endl;
    return 1;
  }

  if (argc == 6 && (std::string_view{argv[4]} != "--project" ||
                    std::string_view{argv[5]} != "project/test/id")) {
    std::cout << "arguments are formatted wrong" << std::endl;
    return 1;
  }

  if (argc == 7 &&
      (std::string_view{argv[4]} != "--all-reserved" || std::string_view{argv[5]} != "--project" ||
       std::string_view{argv[6]} != "project/test/id")) {
    std::cout << "arguments are formatted wrong" << std::endl;
    return 1;
  }

  std::cout << R"([
 {
  "displayName": "displayName-1",
  "id": "id/of/instance1",
  "ipAddress": "123.456.789.012",
  "lastUpdated": "2012-12-12T12:12:12Z",
  "owner": "owner@",
  "pool": "pool-of-test_instance_1",
  "state": "RESERVED"
 },
 {
  "displayName": "displayName-2",
  "id": "id/of/instance2",
  "ipAddress": "123.456.789.012",
  "lastUpdated": "2012-12-12T12:12:12Z",
  "owner": "owner@",
  "pool": "pool-of-test_instance_2",
  "state": "CONFIGURING"
 }
])" << std::endl;
  return 0;
}

int GgpSshInit(int argc, char* argv[]) {
  if (argc < 6 || argc == 7 || argc > 8) {
    std::cout << "Wrong amount of arguments" << std::endl;
    return 1;
  }
  if (std::string_view{argv[1]} != "ssh" || std::string_view{argv[2]} != "init" ||
      std::string_view{argv[3]} != "-s" || std::string_view{argv[4]} != "--instance" ||
      std::string_view{argv[5]} != "instance/test/id") {
    std::cout << "arguments are wrong" << std::endl;
    return 1;
  }

  if (argc == 8 && (std::string_view{argv[6]} != "--project" ||
                    std::string_view{argv[7]} != "project/test/id")) {
    std::cout << "arguments are wrong" << std::endl;
  }

  std::cout << R"({
 "host": "123.456.789.012",
 "keyPath": "example/path/to/a/key",
 "knownHostsPath": "example/path/to/known_hosts",
 "port": "12345",
 "user": "example_user"
})" << std::endl;
  return 0;
}

int GgpProjectList(int argc, char* argv[]) {
  if (argc != 4) {
    std::cout << "Wrong amount of arguments" << std::endl;
    return 1;
  }
  if (std::string_view{argv[1]} != "project" || std::string_view{argv[2]} != "list" ||
      std::string_view{argv[3]} != "-s") {
    std::cout << "arguments are formatted wrong" << std::endl;
    return 1;
  }
  std::cout << R"([
 {
  "displayName": "displayName-1",
  "id": "id/of/project1"
 },
 {
  "displayName": "displayName-2",
  "id": "id/of/project2"
 }
])" << std::endl;
  return 0;
}

int GgpConfig(int argc, char* argv[]) {
  if (argc != 4) {
    std::cout << "Wrong amount of arguments" << std::endl;
    return 1;
  }
  if (std::string_view{argv[1]} != "config" || std::string_view{argv[2]} != "describe" ||
      std::string_view{argv[3]} != "-s") {
    std::cout << "arguments are formatted wrong" << std::endl;
    return 1;
  }
  std::string output{R"({
 "chromeProfileDirectory": "",
 "environment": "Test env",
 "organization": "Test Org",
 "organizationId": "Test Org id",
 "poolId": "",
 "project": "Test Project",
 "projectId": "Test Project id",
 "renderdocLocalPath": "",
 "url": "http://someurl.com/"
})"};
  std::cout << output << std::endl;
  return 0;
}

int GgpInstanceDescribe(int argc, char* argv[]) {
  if (argc != 5) {
    std::cout << "Wrong amount of arguments" << std::endl;
    return 1;
  }

  if (std::string_view{argv[1]} != "instance" || std::string_view{argv[2]} != "describe" ||
      std::string_view{argv[4]} != "-s") {
    std::cout << "arguments are formatted wrong" << std::endl;
    return 1;
  }

  if (std::string_view{argv[3]} == "id/of/instance1") {
    std::cout << R"(
 {
  "displayName": "displayName-1",
  "id": "id/of/instance1",
  "ipAddress": "123.456.789.012",
  "lastUpdated": "2012-12-12T12:12:12Z",
  "owner": "owner@",
  "pool": "pool-of-test_instance_1",
  "state": "RESERVED"
 })" << std::endl;
  } else {
    const std::string result = std::string("Error: instance [") + argv[3] + "] not found";
    std::cout << result << std::endl;
  }
  return 0;
}

int GgpInstance(int argc, char* argv[]) {
  if (argc < 3) {
    std::cout << "Wrong amount of arguments" << std::endl;
    return 1;
  }

  if (std::string_view{argv[2]} == "list") {
    return GgpInstanceList(argc, argv);
  }

  if (std::string_view{argv[2]} == "describe") {
    return GgpInstanceDescribe(argc, argv);
  }

  std::cout << "arguments are formatted wrong" << std::endl;
  return 1;
}

int GgpAuth(int argc, char* argv[]) {
  if (argc != 4) {
    std::cout << "Wrong amount of arguments" << std::endl;
    return 1;
  }
  if (std::string_view{argv[1]} != "auth" || std::string_view{argv[2]} != "list" ||
      std::string_view{argv[3]} != "-s") {
    std::cout << "arguments are formatted wrong" << std::endl;
    return 1;
  }

  std::cout << R"([{"default":"yes", "account":"username@email.com"}])" << std::endl;
  return 0;
}

int GgpCrashReport(int argc, char* argv[]) {
  if (argc < 7) {
    std::cout << "Wrong amount of arguments" << std::endl;
    return 1;
  }
  if (std::string_view{argv[1]} != "crash-report" ||
      std::string_view{argv[2]} != "download-symbols" || std::string_view{argv[3]} != "-s" ||
      std::string_view{argv[4]} != "--show-url") {
    std::cout << "Arguments are wrong" << std::endl;
    return 1;
  }

  const std::map<std::string, std::string> kValidKeyToSymbolDownloadInfo = {
      {"build_id_0/symbol_filename_0", R"(
  {
   "downloadUrl": "valid_url_for_symbol_0",
   "fileId": "symbolFiles/build_id_0/symbol_filename_0"
  })"},
      {"build_id_1/symbol_filename_1", R"(
  {
   "downloadUrl": "valid_url_for_symbol_1",
   "fileId": "symbolFiles/build_id_1/symbol_filename_1"
  })"}};

  std::vector<std::string> symbols_to_output;
  for (auto i = 5; i < argc;) {
    if (std::string_view{argv[i]} != "--module") {
      std::cout << "Arguments are wrong" << i << std::endl;
      return 1;
    }
    if (++i >= argc || std::string_view{argv[i]} == "--module") {
      std::cout << "Flag --module needs an argument" << std::endl;
      return 1;
    }
    std::string key = argv[i++];
    if (kValidKeyToSymbolDownloadInfo.find(key) != kValidKeyToSymbolDownloadInfo.end()) {
      symbols_to_output.push_back(kValidKeyToSymbolDownloadInfo.at(key));
    }
  }

  std::string output = R"(
{
 "symbols": [)";
  for (auto it = symbols_to_output.begin(); it != symbols_to_output.end();) {
    output += *it++;
    if (it != symbols_to_output.end()) output += ",";
  }
  output += R"(
 ]
})";
  std::cout << output;
  return 0;
}

int main(int argc, char* argv[]) {
  // This sleep is here for 2 reasons:
  // 1. The ggp cli which this program is mocking, does have quite a bit of delay, hence having a
  // delay in this mock program, mimics the behaviour of the real ggp cli more closely
  // 2. To test the timeout functionally in OrbitGgp::Client
  std::this_thread::sleep_for(std::chrono::milliseconds{50});

  if (argc <= 1) {
    std::cout << "Wrong amount of arguments" << std::endl;
    return 1;
  }

  if (std::string_view{argv[1]} == "version") {
    return GgpVersion(argc, argv);
  }

  if (std::string_view{argv[1]} == "ssh") {
    return GgpSshInit(argc, argv);
  }

  if (std::string_view{argv[1]} == "instance") {
    return GgpInstance(argc, argv);
  }

  if (std::string_view{argv[1]} == "project") {
    return GgpProjectList(argc, argv);
  }

  if (std::string_view{argv[1]} == "config") {
    return GgpConfig(argc, argv);
  }

  if (std::string_view{argv[1]} == "auth") {
    return GgpAuth(argc, argv);
  }

  if (std::string_view{argv[1]} == "crash-report") {
    return GgpCrashReport(argc, argv);
  }

  std::cout << "arguments are formatted wrong" << std::endl;
  return 1;
}