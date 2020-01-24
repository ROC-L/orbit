#pragma once

extern "C" {
__declspec(dllexport) void __cdecl OrbitInit(void* a_Host);
__declspec(dllexport) void __cdecl OrbitInitRemote(void* a_Host);
__declspec(dllexport) bool __cdecl OrbitIsConnected();
__declspec(dllexport) bool __cdecl OrbitStart();
__declspec(dllexport) bool __cdecl OrbitStop();
}