#include "OrbitService.h"

#include "ConnectionManager.h"
#include "Core.h"
#include "CoreApp.h"
#include "TcpServer.h"

OrbitService::OrbitService(uint16_t port) {
  // TODO: These should be private fields.
  GTcpServer = std::make_unique<TcpServer>();
  GTcpServer->StartServer(port);

  ConnectionManager::Get().InitAsService();

  core_app_ = std::make_unique<CoreApp>();
  GCoreApp = core_app_.get();
  GCoreApp->InitializeManagers();
}

void OrbitService::Run(std::atomic<bool>* exit_requested) {
  while (!(*exit_requested)) {
    GTcpServer->ProcessMainThreadCallbacks();
    Sleep(16);
  }

  GCoreApp = nullptr;
}
