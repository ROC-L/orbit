//-----------------------------------
// Copyright Pierric Gimmig 2013-2017
//-----------------------------------
#pragma once

#include <atomic>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "../OrbitPlugin/OrbitUserData.h"
#include "Message.h"
#include "TcpForward.h"
#include "Threading.h"
#include "Utils.h"

//-----------------------------------------------------------------------------
class TcpPacket {
 public:
  TcpPacket() {}
  explicit TcpPacket(const Message& a_Message, const void* a_Payload)
      : m_Data(new std::vector<char>()) {
    m_Data->resize(sizeof(Message) + a_Message.m_Size + 4);
    memcpy(m_Data->data(), &a_Message, sizeof(Message));

    if (a_Payload) {
      memcpy(m_Data->data() + sizeof(Message), a_Payload, a_Message.m_Size);
    }

    // Footer
    const unsigned int footer = MAGIC_FOOT_MSG;
    memcpy(m_Data->data() + sizeof(Message) + a_Message.m_Size, &footer, 4);
  }

  void Dump() const {
    std::cout << "TcpPacket [" << std::dec << m_Data->size() << " bytes]"
              << std::endl;
    PrintBuffer(m_Data->data(), m_Data->size());
  }

  std::shared_ptr<std::vector<char>> Data() { return m_Data; };

 private:
  std::shared_ptr<std::vector<char>> m_Data;
};

//-----------------------------------------------------------------------------
class TcpEntity {
 public:
  TcpEntity();
  virtual ~TcpEntity();

  virtual void Start();
  virtual void Stop();
  void FlushSendQueue();

  // Note: All Send methods can be called concurrently from multiple threads
  inline void Send(MessageType a_Type) {
    Message msg(a_Type);
    SendMsg(msg, nullptr);
  }

  inline void Send(Message& a_Message);
  inline void Send(const std::string& a_String);
  inline void Send(OrbitLogEntry& a_Entry);
  inline void Send(Orbit::UserData& a_Entry);
  inline void Send(MessageType type, const void* data, size_t size);
  inline void Send(Message& message, const void* data, size_t size);

  template <class T>
  void Send(Message& a_Message, const std::vector<T>& a_Vector);
  template <class T>
  void Send(MessageType a_Type, const std::vector<T>& a_Vector);
  template <class T>
  void Send(Message& a_Message, const T& a_Item);
  template <class T>
  void Send(MessageType a_Type, const T& a_Item);

  // Explicitly remove pointer variant of the method
  template <typename T>
  void Send(Message&, T* const&) = delete;

  inline void Send(MessageType a_Type, const std::string& a_Item);

  typedef std::function<void(const Message&)> MsgCallback;
  void AddCallback(MessageType a_MsgType, MsgCallback a_Callback) {
    m_Callbacks[a_MsgType].push_back(a_Callback);
  }
  void AddMainThreadCallback(MessageType a_MsgType, MsgCallback a_Callback) {
    m_MainThreadCallbacks[a_MsgType].push_back(a_Callback);
  }
  void Callback(const Message& a_Message);
  void ProcessMainThreadCallbacks();
  bool IsValid() const { return m_IsValid; }

 protected:
  void SendMsg(Message& a_Message, const void* a_Payload);
  virtual TcpSocket* GetSocket() = 0;
  void SendData();

 protected:
  TcpService* m_TcpService;
  TcpSocket* m_TcpSocket;
  std::thread senderThread_;
  AutoResetEvent m_ConditionVariable;
  LockFreeQueue<TcpPacket> m_SendQueue;
  std::atomic<uint32_t> m_NumQueuedEntries;
  std::atomic<bool> m_ExitRequested;
  std::atomic<bool> m_FlushRequested;
  std::atomic<uint32_t> m_NumFlushedItems;
  Mutex m_Mutex;
  std::atomic<bool> m_IsValid;

  std::unordered_map<int, std::vector<MsgCallback>> m_Callbacks;
  std::unordered_map<int, std::vector<MsgCallback>> m_MainThreadCallbacks;
  std::vector<std::shared_ptr<MessageOwner>> m_MainThreadMessages;
};

//-----------------------------------------------------------------------------
inline void TcpEntity::Send(Message& a_Message) {
  SendMsg(a_Message, a_Message.GetData());
}

//-----------------------------------------------------------------------------
inline void TcpEntity::Send(MessageType a_Type, const std::string& a_String) {
  Message msg(a_Type, (a_String.size() + 1) * sizeof(a_String[0]));
  SendMsg(msg, a_String.data());
}

//-----------------------------------------------------------------------------
inline void TcpEntity::Send(const std::string& a_String) {
  Send(Msg_String, a_String);
}

//-----------------------------------------------------------------------------
void TcpEntity::Send(OrbitLogEntry& a_Entry) {
  char stackBuffer[1024];
  size_t entrySize = a_Entry.GetBufferSize();
  bool needsAlloc = entrySize > 1024;
  char* buffer = !needsAlloc ? stackBuffer : new char[entrySize];

  memcpy(buffer, &a_Entry, a_Entry.GetSizeWithoutString());
  memcpy(buffer + a_Entry.GetSizeWithoutString(), a_Entry.m_Text.c_str(),
         a_Entry.GetStringSize());

  Message msg(Msg_OrbitLog, entrySize);
  SendMsg(msg, buffer);

  if (needsAlloc) {
    delete buffer;
  }
}

//-----------------------------------------------------------------------------
void TcpEntity::Send(Orbit::UserData& a_UserData) {
  Message msg(Msg_UserData);
  msg.m_Size = sizeof(Orbit::UserData) + a_UserData.m_NumBytes;

  char stackBuffer[1024];
  bool needsAlloc = msg.m_Size > 1024;
  char* buffer = !needsAlloc ? stackBuffer : new char[msg.m_Size];

  memcpy(buffer, &a_UserData, sizeof(Orbit::UserData));
  memcpy(buffer + sizeof(Orbit::UserData), a_UserData.m_Data,
         a_UserData.m_NumBytes);

  SendMsg(msg, buffer);

  if (needsAlloc) {
    delete buffer;
  }
}

//-----------------------------------------------------------------------------
template <class T>
void TcpEntity::Send(Message& a_Message, const std::vector<T>& a_Vector) {
  a_Message.m_Size = a_Vector.size() * sizeof(T);
  SendMsg(a_Message, a_Vector.data());
}

//-----------------------------------------------------------------------------
template <class T>
void TcpEntity::Send(MessageType a_Type, const std::vector<T>& a_Vector) {
  Message msg(a_Type);
  Send(msg, a_Vector);
}

//-----------------------------------------------------------------------------
template <class T>
void TcpEntity::Send(Message& a_Message, const T& a_Item) {
  a_Message.m_Size = sizeof(T);
  SendMsg(a_Message, &a_Item);
}

//-----------------------------------------------------------------------------
template <class T>
void TcpEntity::Send(MessageType a_Type, const T& a_Item) {
  Message msg(a_Type);
  Send(msg, a_Item);
}

void TcpEntity::Send(MessageType type, const void* data, size_t size) {
  Message msg(type);
  msg.m_Size = size;
  SendMsg(msg, data);
}

void TcpEntity::Send(Message& message, const void* data, size_t size) {
  message.m_Size = size;
  SendMsg(message, data);
}
