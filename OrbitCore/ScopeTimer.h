//-----------------------------------
// Copyright Pierric Gimmig 2013-2017
//-----------------------------------
#pragma once

#include <string>

#include "Profiling.h"

#define SCOPE_TIMER_LOG(msg) LocalScopeTimer UNIQUE_ID(msg)
#define SCOPE_TIMER_FUNC SCOPE_TIMER_LOG(__FUNCTION__)
extern thread_local int CurrentDepth;

//-----------------------------------------------------------------------------
#pragma pack(push, 1)
class Timer {
 public:
  Timer()
      : m_TID(0),
        m_Depth(0),
        m_SessionID(-1),
        m_Type(NONE),
        m_Processor(-1),
        m_CallstackHash(0),
        m_FunctionAddress(0) {
    m_UserData[0] = 0;
    m_UserData[1] = 0;
  }

  void Start();
  void Stop();
  void Reset() {
    Stop();
    Start();
  }

  inline double ElapsedMicros() const;
  inline double ElapsedMillis() const;
  inline double ElapsedSeconds() const;

  inline double QueryMillis() {
    Stop();
    return ElapsedMillis();
  }
  inline double QuerySeconds() {
    Stop();
    return ElapsedSeconds();
  }

  static inline int GetCurrentDepthTLS() { return CurrentDepth; }
  static inline void ClearThreadDepthTLS() { CurrentDepth = 0; }

  static const int Version = 0;

  enum Type : uint8_t {
    NONE,
    CORE_ACTIVITY,
    THREAD_ACTIVITY,
    HIGHLIGHT,
    UNREAL_OBJECT,
    ZONE,
    ALLOC,
    FREE
  };

  Type GetType() const { return m_Type; }
  void SetType(Type a_Type) { m_Type = a_Type; }
  bool IsType(Type a_Type) const { return m_Type == a_Type; }
  bool IsCoreActivity() const { return m_Type == CORE_ACTIVITY; }

 public:
  // Needs to have to exact same layout in win32/x64, debug/release
  uint32_t m_TID;
  uint8_t m_Depth;
  uint8_t m_SessionID;
  Type m_Type;
  uint8_t m_Processor;
  uint64_t m_CallstackHash;
  uint64_t m_FunctionAddress;
  uint64_t m_UserData[2];
  TickType m_Start;
  TickType m_End;
};
#pragma pack(pop)
// static_assert(sizeof(Timer) == 56);
// static_assert(offsetof(Timer, m_TID) == 0);
// static_assert(offsetof(Timer, m_Depth) == 4);
// static_assert(offsetof(Timer, m_SessionID) == 5);
// static_assert(offsetof(Timer, m_Type) == 6);
// static_assert(offsetof(Timer, m_Processor) == 7);
// static_assert(offsetof(Timer, m_CallstackHash) == 8);
// static_assert(offsetof(Timer, m_FunctionAddress) == 16);
// static_assert(offsetof(Timer, m_UserData) == 24);
// static_assert(offsetof(Timer, m_Start) == 40);
// static_assert(offsetof(Timer, m_End) == 48);

//-----------------------------------------------------------------------------
class ScopeTimer {
 public:
  ScopeTimer() {}
  ScopeTimer(const char* a_Name);
  ~ScopeTimer();

 protected:
  Timer m_Timer;
};

//-----------------------------------------------------------------------------
class LocalScopeTimer {
 public:
  LocalScopeTimer();
  LocalScopeTimer(const std::wstring& a_Message);
  LocalScopeTimer(const char* a_Message);
  LocalScopeTimer(double* a_Millis);
  ~LocalScopeTimer();

 protected:
  Timer m_Timer;
  double* m_Millis;
  std::wstring m_Msg;
};

//-----------------------------------------------------------------------------
class ConditionalScopeTimer {
 public:
  ConditionalScopeTimer() : m_Active(false) {}
  ~ConditionalScopeTimer();
  void Start(const char* a_Name);

 protected:
  enum { NameSize = 64 };

  Timer m_Timer;
  bool m_Active;
  char m_Name[NameSize];
};

//-----------------------------------------------------------------------------
inline double Timer::ElapsedMicros() const {
  return MicroSecondsFromTicks(m_Start, m_End);
}

//-----------------------------------------------------------------------------
inline double Timer::ElapsedMillis() const { return ElapsedMicros() * 0.001; }

//-----------------------------------------------------------------------------
inline double Timer::ElapsedSeconds() const {
  return ElapsedMicros() * 0.000001;
}
