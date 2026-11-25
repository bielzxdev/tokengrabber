#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <windows.h>
#include <winternl.h>
#include <vector>
#include <string>

namespace Hollowing {

    // Info do PE parseado
    struct PE_INFO {
        bool is64Bit;
        PIMAGE_DOS_HEADER   dosHeader;
        PIMAGE_NT_HEADERS32 ntHeaders32;
        PIMAGE_NT_HEADERS64 ntHeaders64;
        std::vector<BYTE>   data;
        DWORD size;
    };

    // APIs Win32 minimas ainda necessarias (nao ha syscall direto equivalente).
    // Todas resolvidas via MyGetProcAddress do kernelbase.dll (nao usa GetProcAddress).
    typedef BOOL (WINAPI* pCreateProcessA)(LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES,
        LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCSTR,
        LPSTARTUPINFOA, LPPROCESS_INFORMATION);
    typedef BOOL (WINAPI* pTerminateProcess)(HANDLE, UINT);
    typedef BOOL (WINAPI* pWow64GetThreadContext)(HANDLE, PWOW64_CONTEXT);
    typedef BOOL (WINAPI* pWow64SetThreadContext)(HANDLE, PWOW64_CONTEXT);
    typedef DWORD(WINAPI* pGetFileAttributesA)(LPCSTR);

    class APIResolver {
    public:
        static APIResolver& GetInstance() { static APIResolver i; return i; }
        bool ResolveAll();

        pCreateProcessA         CreateProcessA;
        pTerminateProcess       TerminateProcess;
        pWow64GetThreadContext  Wow64GetThreadContext;
        pWow64SetThreadContext  Wow64SetThreadContext;
        pGetFileAttributesA     GetFileAttributesA;

    private:
        APIResolver()
            : CreateProcessA(nullptr), TerminateProcess(nullptr),
              Wow64GetThreadContext(nullptr), Wow64SetThreadContext(nullptr),
              GetFileAttributesA(nullptr) {}
    };
}
