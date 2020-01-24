//-----------------------------------
// Copyright Pierric Gimmig 2013-2017
//-----------------------------------
#pragma once

#include <string>
#include <vector>

#include "OrbitDbgHelp.h"
#include "cvconst.h"
#include "BaseTypes.h"
#include "FunctionStats.h"
#include "SerializationMacros.h"
#include "Utils.h"

#ifdef _WIN32
#include "TypeInfoStructs.h"
#endif

class Pdb;

//-----------------------------------------------------------------------------
struct FunctionParam
{
    FunctionParam();
    std::wstring m_Name;
    std::wstring m_ParamType;
    std::wstring m_Type;
    std::wstring m_Address;

#ifdef _WIN32
    SYMBOL_INFO m_SymbolInfo;
#endif

    bool InRegister( int a_Index );
    bool IsPointer() { return m_Type.find( L"*" ) != std::wstring::npos; }
    bool IsRef() { return m_Type.find( L"&" ) != std::wstring::npos; }
    bool IsFloat();
};

//-----------------------------------------------------------------------------
struct Argument
{
    Argument() { memset( this, 0, sizeof( *this ) ); }
    DWORD      m_Index;
    CV_HREG_e  m_Reg;
    DWORD      m_Offset;
    DWORD      m_NumBytes;
};

//-----------------------------------------------------------------------------
struct FunctionArgInfo
{
    FunctionArgInfo() : m_NumStackBytes(0), m_ArgDataSize(0) {}
    int m_NumStackBytes;
    int m_ArgDataSize;
    std::vector< Argument > m_Args;
};

//-----------------------------------------------------------------------------
class Function
{
public:
    Function();
    ~Function();

    void Print();
    void SetAsMainFrameFunction();
    void AddParameter( const FunctionParam & a_Param ){ m_Params.push_back( a_Param ); }
    const std::string & PrettyName();
    inline const std::string & Lower() { if( m_PrettyNameLower.size() == 0 ) m_PrettyNameLower = ToLower( m_PrettyName ); return m_PrettyNameLower; }
    static const TCHAR* GetCallingConventionString( int a_CallConv );
    void ProcessArgumentInfo();
    bool IsMemberFunction();
    unsigned long long Hash() { if( m_NameHash == 0 ) { m_NameHash = StringHash( m_PrettyName ); } return m_NameHash; }
    bool Hookable();
    void Select();
    void PreHook();
    void UnSelect();
    void ToggleSelect() { /*if( Hookable() )*/ m_Selected = !m_Selected; }
    bool IsSelected() const { return m_Selected; }
    DWORD64 GetVirtualAddress() const;
    bool IsOrbitFunc() { return m_OrbitType != OrbitType::NONE; }
    bool IsOrbitZone() { return m_OrbitType == ORBIT_TIMER_START || m_OrbitType == ORBIT_TIMER_STOP; }
    bool IsOrbitStart(){ return m_OrbitType == ORBIT_TIMER_START; }
    bool IsOrbitStop() { return m_OrbitType == ORBIT_TIMER_STOP; }
    bool IsRealloc()   { return m_OrbitType == REALLOC; }
    bool IsAlloc()     { return m_OrbitType == ALLOC; }
    bool IsFree()      { return m_OrbitType == FREE; }
    bool IsMemoryFunc(){ return IsFree() || IsAlloc() || IsRealloc(); }
    std::wstring GetModuleName();
    class Type* GetParentType();
    void ResetStats();
    void GetDisassembly();
    void FindFile();

    enum MemberID
    {
        NAME,
        ADDRESS,
        MODULE,
        FILE,
        LINE,
        SELECTED,
        INDEX,
        SIZE,
        CALL_CONV,
        NUM_EXPOSED_MEMBERS
    };

    enum OrbitType
    {
        NONE,
        ORBIT_TIMER_START,
        ORBIT_TIMER_STOP,
        ORBIT_LOG,
        ORBIT_OUTPUT_DEBUG_STRING,
        UNREAL_ACTOR,
        ALLOC,
        FREE,
        REALLOC,
        ORBIT_DATA,
        NUM_TYPES
    };

    ORBIT_SERIALIZABLE;

public: // TODO...
    std::string   m_Name;
    std::string   m_PrettyName;
    std::string   m_PrettyNameLower;
    std::string   m_Module;
    std::string   m_File;
    std::string   m_Probe;
    uint64_t      m_Address = 0;
    uint64_t      m_ModBase = 0;
    uint32_t      m_Size = 0;
    uint32_t      m_Id = 0;
    uint32_t      m_ParentId = 0;
    int           m_Line = 0;
    int           m_CallConv = -1;
    std::vector<FunctionParam>     m_Params;
    std::vector<Argument>          m_ArgInfo;
    Pdb*                           m_Pdb = nullptr;
    uint64_t                       m_NameHash = 0;
    OrbitType                      m_OrbitType = NONE;
    std::shared_ptr<FunctionStats> m_Stats;

protected:
    bool     m_Selected = false;
};
