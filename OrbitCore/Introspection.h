#ifndef ORBIT_CORE_INTROSPECTION_H_
#define ORBIT_CORE_INTROSPECTION_H_

#include "OrbitBase/Tracing.h"

#include "LinuxTracingSession.h"
#include "ScopeTimer.h"
#include "StringManager.h"

#if ORBIT_TRACING_ENABLED

namespace orbit {
namespace introspection {

class Handler : public orbit::tracing::Handler {
 public:
  Handler(std::shared_ptr<StringManager> string_manager,
          LinuxTracingSession* tracing_session);

  void Begin(const char* name) final;
  void End() final;
  void Track(const char* name, int) final;
  void Track(const char* name, float) final;

 private:
  void SendKeyAndString(uint64_t hash, const std::string& name);

  std::shared_ptr<StringManager> string_manager_;
  LinuxTracingSession* tracing_session_;
};

struct Scope {
  Timer timer_;
  std::string name_;
};

}  // namespace introspection
}  // namespace orbit

#endif  // ORBIT_TRACING_ENABLED

#endif  // ORBIT_CORE_INTROSPECTION_H_
