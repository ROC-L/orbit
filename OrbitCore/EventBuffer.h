//-----------------------------------
// Copyright Pierric Gimmig 2013-2017
//-----------------------------------
#pragma once

#include <set>

#include "BlockChain.h"
#include "Callstack.h"
#include "Core.h"
#include "SerializationMacros.h"

#ifdef __linux
#include "LinuxUtils.h"
#endif

//-----------------------------------------------------------------------------
struct CallstackEvent {
  CallstackEvent() = default;
  CallstackEvent(uint64_t a_Time, CallstackID a_Id, ThreadID a_TID)
      : m_Time(a_Time), m_Id(a_Id), m_TID(a_TID) {}

  uint64_t m_Time = 0;
  CallstackID m_Id = 0;
  ThreadID m_TID = 0;

  ORBIT_SERIALIZABLE;
};

//-----------------------------------------------------------------------------
class EventBuffer {
 public:
  EventBuffer() : m_MaxTime(0), m_MinTime(LLONG_MAX) {}

  void Print();
  void Reset() {
    m_CallstackEvents.clear();
    m_MinTime = LLONG_MAX;
    m_MaxTime = 0;
  }
  std::map<ThreadID, std::map<uint64_t, CallstackEvent> >& GetCallstacks() {
    return m_CallstackEvents;
  }
  Mutex& GetMutex() { return m_Mutex; }
  std::vector<CallstackEvent> GetCallstackEvents(uint64_t a_TimeBegin,
                                                 uint64_t a_TimeEnd,
                                                 ThreadID a_ThreadId = 0);
  uint64_t GetMaxTime() const { return m_MaxTime; }
  uint64_t GetMinTime() const { return m_MinTime; }
  bool HasEvent() {
    ScopeLock lock(m_Mutex);
    return !m_CallstackEvents.empty();
  }
  bool HasEvent(ThreadID a_TID) {
    ScopeLock lock(m_Mutex);
    return m_CallstackEvents.find(a_TID) != m_CallstackEvents.end();
  }

#ifdef __linux__
  size_t GetNumEvents() const;
#endif

  //-----------------------------------------------------------------------------
  void RegisterTime(uint64_t a_Time) {
    if (a_Time > m_MaxTime) m_MaxTime = a_Time;
    if (a_Time > 0 && a_Time < m_MinTime) m_MinTime = a_Time;
  }

  //-----------------------------------------------------------------------------
  void AddCallstackEvent(uint64_t time, CallstackID cs_hash,
                         ThreadID thread_id);

  ORBIT_SERIALIZABLE;

 private:
  Mutex m_Mutex;
  std::map<ThreadID, std::map<uint64_t, CallstackEvent> > m_CallstackEvents;
  std::atomic<uint64_t> m_MaxTime;
  std::atomic<uint64_t> m_MinTime;
};
