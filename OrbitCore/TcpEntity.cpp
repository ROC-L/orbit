//-----------------------------------
// Copyright Pierric Gimmig 2013-2017
//-----------------------------------

#include "Core.h"
#include "TcpEntity.h"
#include "Tcp.h"
#include "Log.h"
#include "OrbitAsio.h"

//-----------------------------------------------------------------------------
TcpEntity::TcpEntity() : m_NumQueuedEntries(0)
                       , m_ExitRequested(false)
                       , m_FlushRequested(false)
                       , m_NumFlushedItems(0)
{
    PRINT_FUNC;
    m_TcpSocket = new TcpSocket();
    m_TcpService = new TcpService();
}

//-----------------------------------------------------------------------------
TcpEntity::~TcpEntity()
{
}

//-----------------------------------------------------------------------------
void TcpEntity::Start()
{
    PRINT_FUNC;
    m_SenderThread = new std::thread( [&](){ SendData(); } );
}

//-----------------------------------------------------------------------------
void TcpEntity::Stop()
{
    PRINT_FUNC;
    if( !m_ExitRequested )
    {
        m_ExitRequested = true;
    }

    if (m_SenderThread) {
        m_ConditionVariable.signal();
        m_SenderThread->join();
    }
    
    if( m_TcpSocket && m_TcpSocket->m_Socket )
    {
        if( m_TcpSocket->m_Socket->is_open() )
        {
            m_TcpSocket->m_Socket->close();
        }
    }
    if( m_TcpService && m_TcpService->m_IoService )
    {
        m_TcpService->m_IoService->stop();
    }
}

//-----------------------------------------------------------------------------
void TcpEntity::SendMsg( Message & a_Message, const void* a_Payload )
{
    TcpPacket buffer( a_Message, a_Payload );
    m_SendQueue.enqueue( buffer );
    ++m_NumQueuedEntries;
    m_ConditionVariable.signal();
}

//-----------------------------------------------------------------------------
void TcpEntity::FlushSendQueue()
{
    m_FlushRequested = true;

    const size_t numItems = 4096;
    TcpPacket Timers[numItems];
    m_NumFlushedItems = 0;

    while( !m_ExitRequested )
    {
        size_t numDequeued = m_SendQueue.try_dequeue_bulk( Timers, numItems );

        if( numDequeued == 0 )
            break;

        m_NumQueuedEntries -= (int)numDequeued;
        m_NumFlushedItems += (int)numDequeued;
    }

    m_FlushRequested = false;
    m_ConditionVariable.signal();
}

//-----------------------------------------------------------------------------
void TcpEntity::SendData()
{
    SetCurrentThreadName( L"TcpSender" );

    while( !m_ExitRequested )
    {
        // Wait for non-empty queue
        while( m_NumQueuedEntries <= 0 && !m_ExitRequested )
        {
            m_ConditionVariable.wait();
        }

        // Send messages
        TcpPacket buffer;
        while( !m_ExitRequested && !m_FlushRequested && m_SendQueue.try_dequeue( buffer ) )
        {
            --m_NumQueuedEntries;
            TcpSocket* socket = GetSocket();
            if( socket && socket->m_Socket && socket->m_Socket->is_open() )
            {
                asio::write( *socket->m_Socket, shared_const_buffer( buffer ) );
            }
            else
            {
                ORBIT_ERROR;
            }
        }
    }
}
