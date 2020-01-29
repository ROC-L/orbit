//-----------------------------------
// Copyright Pierric Gimmig 2013-2017
//-----------------------------------
#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "Message.h"

class ConnectionManager {
 public:
  ConnectionManager();
  ~ConnectionManager();
  static ConnectionManager& Get();
  void Init();
  void InitAsService();
  void ConnectToRemote(std::string a_RemoteAddress);
  void SetSelectedFunctionsOnRemote(const Message& a_Msg);
  bool IsService() { return m_IsService; }
  void StartCaptureAsRemote();
  void StopCaptureAsRemote();
  void Stop();

 protected:
  void ConnectionThread();
  void RemoteThread();
  void TerminateThread();
  void SetupClientCallbacks();
  void SetupServerCallbacks();
  void SendProcesses(class TcpEntity* a_TcpEntity);

 protected:
  std::unique_ptr<std::thread> m_Thread;
  std::string m_RemoteAddress;
  std::atomic<bool> m_ExitRequested;
  bool m_IsService;
  std::shared_ptr<class BpfTrace> m_BpfTrace;
};