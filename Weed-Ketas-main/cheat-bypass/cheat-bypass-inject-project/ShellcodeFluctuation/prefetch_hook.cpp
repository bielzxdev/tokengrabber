#include "prefetch_hook.h"

#include <windows.h>
#include <winsvc.h>
#include <tlhelp32.h>

#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
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
    using fn_FlushInstructionCache = BOOL(WINAPI*)(HANDLE, LPCVOID, SIZE_T);
    using fn_WaitForSingleObject = DWORD(WINAPI*)(HANDLE, DWORD);
    using fn_CloseHandle = BOOL(WINAPI*)(HANDLE);
    using fn_VirtualAllocEx = LPVOID(WINAPI*)(HANDLE, LPVOID, SIZE_T, DWORD, DWORD);
    using fn_VirtualFreeEx = BOOL(WINAPI*)(HANDLE, LPVOID, SIZE_T, DWORD);
    using fn_CreateToolhelp32Snapshot = HANDLE(WINAPI*)(DWORD, DWORD);
    using fn_Module32FirstW = BOOL(WINAPI*)(HANDLE, LPMODULEENTRY32W);
    using fn_Module32NextW = BOOL(WINAPI*)(HANDLE, LPMODULEENTRY32W);
    using fn_OpenProcessToken = BOOL(WINAPI*)(HANDLE, DWORD, PHANDLE);
    using fn_LookupPrivilegeValueW = BOOL(WINAPI*)(LPCWSTR, LPCWSTR, PLUID);
    using fn_AdjustTokenPrivileges = BOOL(WINAPI*)(HANDLE, BOOL, PTOKEN_PRIVILEGES,
                                                    DWORD, PTOKEN_PRIVILEGES, PDWORD);
    using fn_GetLastError = DWORD(WINAPI*)();
    using fn_OpenSCManagerW = SC_HANDLE(WINAPI*)(LPCWSTR, LPCWSTR, DWORD);
    using fn_OpenServiceW = SC_HANDLE(WINAPI*)(SC_HANDLE, LPCWSTR, DWORD);
    using fn_QueryServiceStatusEx = BOOL(WINAPI*)(SC_HANDLE, SC_STATUS_TYPE,
                                                   LPBYTE, DWORD, LPDWORD);
    using fn_CloseServiceHandle = BOOL(WINAPI*)(SC_HANDLE);

    constexpr SIZE_T kPatchSize = 12;
    constexpr SIZE_T kRemoteStubSize = 0x1000;
    constexpr std::size_t kMaxIatPatches = 4;

    struct RemoteIatPatch
    {
        LPVOID slot = nullptr;
        ULONG_PTR original = 0;
        LPVOID replacement = nullptr;
    };

    enum class RemoteStubKind
    {
        NtStatusAccessDenied,
        InvalidHandle,
        BooleanFalse
    };

    DWORD g_lastError = ERROR_SUCCESS;
    DWORD g_targetPid = 0;

    HMODULE GetKernelBase();
    void CaptureLastError(HMODULE kernelBase);

    HMODULE GetKernel32()
    {
        HMODULE module = static_cast<HMODULE>(PebGetModuleBase(WENC(L"kernel32.dll")));
        if (!module)
            module = GetKernelBase();
        return module;
    }

    bool EqualAscii(const char* left, const char* right)
    {
        if (!left || !right)
            return false;

        while (*left && *right)
        {
            char a = *left++;
            char b = *right++;
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
            if (a != b)
                return false;
        }
        return *left == '\0' && *right == '\0';
    }

    uintptr_t FindRemoteModule(HMODULE kernelBase, DWORD pid,
                               const wchar_t* wantedName)
    {
        // WENC() decrypts into one thread-local buffer.  GetKernel32() below
        // also uses WENC(), so retaining wantedName directly would replace
        // "ntdll.dll"/"sysmain.dll" with "kernel32.dll" before the module
        // enumeration compares it.  Copy the requested name first.
        wchar_t wantedCopy[64]{};
        if (!wantedName)
            return 0;
        std::size_t wantedLength = 0;
        while (wantedName[wantedLength] && wantedLength + 1 <
               _countof(wantedCopy))
        {
            wantedCopy[wantedLength] = wantedName[wantedLength];
            ++wantedLength;
        }
        if (wantedName[wantedLength] != L'\0')
            return 0;

        HMODULE kernel32 = GetKernel32();
        if (!kernel32 || !kernelBase)
            return 0;

        auto createSnapshot = reinterpret_cast<fn_CreateToolhelp32Snapshot>(
            PebGetProcAddr(kernel32, ENC("CreateToolhelp32Snapshot")));
        auto moduleFirst = reinterpret_cast<fn_Module32FirstW>(
            PebGetProcAddr(kernel32, ENC("Module32FirstW")));
        auto moduleNext = reinterpret_cast<fn_Module32NextW>(
            PebGetProcAddr(kernel32, ENC("Module32NextW")));
        auto closeHandle = reinterpret_cast<fn_CloseHandle>(
            PebGetProcAddr(kernelBase, ENC("CloseHandle")));
        if (!createSnapshot || !moduleFirst || !moduleNext || !closeHandle)
            return 0;

        HANDLE snapshot = INVALID_HANDLE_VALUE;
        for (int attempt = 0; attempt != 3; ++attempt)
        {
            snapshot = createSnapshot(
                TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
            if (snapshot != INVALID_HANDLE_VALUE)
                break;
        }
        if (snapshot == INVALID_HANDLE_VALUE)
            return 0;

        uintptr_t result = 0;
        MODULEENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (moduleFirst(snapshot, &entry))
        {
            do
            {
                if (_wcsicmp(entry.szModule, wantedCopy) == 0)
                {
                    result = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                    break;
                }
            } while (moduleNext(snapshot, &entry));
        }

        closeHandle(snapshot);
        return result;
    }

    bool ReadRemote(fn_ReadProcessMemory readProcessMemory, HANDLE process,
                    uintptr_t address, void* buffer, SIZE_T size)
    {
        if (!readProcessMemory || !process || !address || !buffer || !size)
            return false;

        SIZE_T read = 0;
        return readProcessMemory(process, reinterpret_cast<LPCVOID>(address),
                                  buffer, size, &read) && read == size;
    }

    struct RemotePattern
    {
        const BYTE* bytes;
        const char* mask;
        std::size_t size;
    };

    bool MatchesPattern(const BYTE* data, const RemotePattern& pattern)
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

    uintptr_t FindRemotePattern(
        fn_ReadProcessMemory readProcessMemory,
        HANDLE process,
        uintptr_t moduleBase,
        const RemotePattern& pattern)
    {
        if (!readProcessMemory || !process || !moduleBase || pattern.size == 0)
            return 0;

        IMAGE_DOS_HEADER dos{};
        if (!ReadRemote(readProcessMemory, process, moduleBase, &dos,
                        sizeof(dos)) ||
            dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0)
        {
            return 0;
        }

        const uintptr_t ntAddress = moduleBase +
            static_cast<uintptr_t>(static_cast<DWORD>(dos.e_lfanew));
        IMAGE_NT_HEADERS64 nt{};
        if (!ReadRemote(readProcessMemory, process, ntAddress, &nt, sizeof(nt)) ||
            nt.Signature != IMAGE_NT_SIGNATURE ||
            nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
            nt.FileHeader.NumberOfSections == 0 ||
            nt.OptionalHeader.SizeOfImage == 0)
        {
            return 0;
        }

        const uintptr_t sectionHeaders = ntAddress + sizeof(DWORD) +
            sizeof(IMAGE_FILE_HEADER) + nt.FileHeader.SizeOfOptionalHeader;
        uintptr_t match = 0;

        for (WORD index = 0; index < nt.FileHeader.NumberOfSections; ++index)
        {
            IMAGE_SECTION_HEADER section{};
            const uintptr_t sectionAddress = sectionHeaders +
                static_cast<uintptr_t>(index) * sizeof(IMAGE_SECTION_HEADER);
            if (!ReadRemote(readProcessMemory, process, sectionAddress,
                            &section, sizeof(section)))
            {
                return 0;
            }

            if ((section.Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0)
                continue;

            const DWORD sectionSize = section.Misc.VirtualSize
                ? section.Misc.VirtualSize
                : section.SizeOfRawData;
            if (sectionSize < pattern.size ||
                section.VirtualAddress >= nt.OptionalHeader.SizeOfImage)
            {
                continue;
            }

            SIZE_T scanSize = sectionSize;
            const SIZE_T imageRemaining =
                nt.OptionalHeader.SizeOfImage - section.VirtualAddress;
            if (scanSize > imageRemaining)
                scanSize = imageRemaining;
            if (scanSize < pattern.size)
                continue;

            try
            {
                std::vector<BYTE> code(scanSize);
                const uintptr_t remoteCode = moduleBase + section.VirtualAddress;
                if (!ReadRemote(readProcessMemory, process, remoteCode,
                                code.data(), code.size()))
                {
                    return 0;
                }

                for (SIZE_T offset = 0;
                     offset + pattern.size <= code.size();
                     ++offset)
                {
                    if (!MatchesPattern(code.data() + offset, pattern))
                        continue;

                    const uintptr_t candidate = remoteCode + offset;
                    if (match != 0)
                    {
                        // Never patch an ambiguous match.  A future Windows
                        // build must provide a more specific signature.
                        return 0;
                    }
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

    bool FindRemoteIatSlot(fn_ReadProcessMemory readProcessMemory,
                           HANDLE process, uintptr_t moduleBase,
                           const char* functionName, LPVOID* slot,
                           ULONG_PTR* original)
    {
        if (!readProcessMemory || !process || !moduleBase ||
            !functionName || !slot || !original)
            return false;

        IMAGE_DOS_HEADER dos{};
        if (!ReadRemote(readProcessMemory, process, moduleBase, &dos, sizeof(dos)) ||
            dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0)
            return false;

        IMAGE_NT_HEADERS64 nt{};
        const uintptr_t ntAddress = moduleBase +
            static_cast<uintptr_t>(static_cast<DWORD>(dos.e_lfanew));
        if (!ReadRemote(readProcessMemory, process, ntAddress, &nt, sizeof(nt)) ||
            nt.Signature != IMAGE_NT_SIGNATURE ||
            nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            return false;

        const IMAGE_DATA_DIRECTORY& imports =
            nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!imports.VirtualAddress || !imports.Size)
            return false;

        const uintptr_t importBase = moduleBase + imports.VirtualAddress;
        const std::size_t descriptorCount =
            (imports.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR) < 1024)
                ? imports.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR)
                : 1024;

        for (std::size_t descriptorIndex = 0;
             descriptorIndex < descriptorCount; ++descriptorIndex)
        {
            IMAGE_IMPORT_DESCRIPTOR descriptor{};
            if (!ReadRemote(readProcessMemory, process,
                            importBase + descriptorIndex * sizeof(descriptor),
                            &descriptor, sizeof(descriptor)))
                return false;
            if (!descriptor.Name)
                break;
            if (!descriptor.FirstThunk)
                continue;

            const DWORD originalThunkRva = descriptor.OriginalFirstThunk
                ? descriptor.OriginalFirstThunk
                : descriptor.FirstThunk;
            for (std::size_t thunkIndex = 0; thunkIndex < 65536; ++thunkIndex)
            {
                ULONGLONG thunkValue = 0;
                const uintptr_t thunkAddress = moduleBase + originalThunkRva +
                    thunkIndex * sizeof(ULONGLONG);
                if (!ReadRemote(readProcessMemory, process, thunkAddress,
                                &thunkValue, sizeof(thunkValue)))
                    return false;
                if (!thunkValue)
                    break;
                if (IMAGE_SNAP_BY_ORDINAL64(thunkValue))
                    continue;

                char importedName[256]{};
                const uintptr_t nameAddress = moduleBase +
                    static_cast<uintptr_t>(thunkValue) + sizeof(WORD);
                if (!ReadRemote(readProcessMemory, process, nameAddress,
                                importedName, sizeof(importedName) - 1))
                    return false;
                importedName[sizeof(importedName) - 1] = '\0';

                if (!EqualAscii(importedName, functionName))
                    continue;

                const uintptr_t slotAddress = moduleBase + descriptor.FirstThunk +
                    thunkIndex * sizeof(ULONG_PTR);
                ULONG_PTR current = 0;
                if (!ReadRemote(readProcessMemory, process, slotAddress,
                                &current, sizeof(current)))
                    return false;
                *slot = reinterpret_cast<LPVOID>(slotAddress);
                *original = current;
                return true;
            }
        }
        return false;
    }

    LPVOID AllocateRemoteStub(HMODULE kernelBase, HANDLE process,
                              RemoteStubKind kind,
                              fn_WriteProcessMemory writeProcessMemory,
                              fn_VirtualAllocEx virtualAllocEx,
                              fn_FlushInstructionCache flushInstructionCache,
                              fn_VirtualFreeEx virtualFreeEx)
    {
        if (!kernelBase || !process || !writeProcessMemory ||
            !virtualAllocEx || !virtualFreeEx)
            return nullptr;

        BYTE code[16]{};
        SIZE_T codeSize = 0;
        switch (kind)
        {
        case RemoteStubKind::NtStatusAccessDenied:
            // mov eax, STATUS_ACCESS_DENIED; ret
            code[0] = 0xB8;
            code[1] = 0x22;
            code[2] = 0x00;
            code[3] = 0x00;
            code[4] = 0xC0;
            code[5] = 0xC3;
            codeSize = 6;
            break;
        case RemoteStubKind::InvalidHandle:
            // mov rax, INVALID_HANDLE_VALUE; ret
            code[0] = 0x48;
            code[1] = 0xC7;
            code[2] = 0xC0;
            code[3] = 0xFF;
            code[4] = 0xFF;
            code[5] = 0xFF;
            code[6] = 0xFF;
            code[7] = 0xC3;
            codeSize = 8;
            break;
        case RemoteStubKind::BooleanFalse:
            // xor eax, eax; ret
            code[0] = 0x31;
            code[1] = 0xC0;
            code[2] = 0xC3;
            codeSize = 3;
            break;
        }

        LPVOID remote = virtualAllocEx(process, nullptr, kRemoteStubSize,
                                       MEM_COMMIT | MEM_RESERVE,
                                       PAGE_EXECUTE_READWRITE);
        if (!remote)
            return nullptr;

        SIZE_T written = 0;
        if (!writeProcessMemory(process, remote, code, codeSize, &written) ||
            written != codeSize)
        {
            virtualFreeEx(process, remote, 0, MEM_RELEASE);
            return nullptr;
        }
        if (flushInstructionCache)
            flushInstructionCache(process, remote, codeSize);
        return remote;
    }

    bool WriteRemoteBytes(HANDLE process, LPVOID address, const void* bytes,
                          SIZE_T size,
                          fn_ReadProcessMemory readProcessMemory,
                          fn_WriteProcessMemory writeProcessMemory,
                          fn_VirtualProtectEx virtualProtectEx,
                          fn_FlushInstructionCache flushInstructionCache)
    {
        if (!process || !address || !bytes || size == 0 || size > 64 ||
            !readProcessMemory || !writeProcessMemory || !virtualProtectEx)
            return false;

        DWORD oldProtection = 0;
        if (!virtualProtectEx(process, address, size, PAGE_EXECUTE_READWRITE,
                              &oldProtection))
        {
            CaptureLastError(GetKernelBase());
            return false;
        }

        SIZE_T written = 0;
        const BOOL writeOk = writeProcessMemory(
            process, address, bytes, size, &written);
        DWORD ignored = 0;
        const BOOL protectionRestored = virtualProtectEx(
            process, address, size, oldProtection, &ignored);
        if (!writeOk || written != size || !protectionRestored)
        {
            CaptureLastError(GetKernelBase());
            return false;
        }

        if (flushInstructionCache)
            flushInstructionCache(process, address, size);

        std::array<BYTE, 64> verify{};
        SIZE_T read = 0;
        if (!readProcessMemory(process, address, verify.data(), size, &read) ||
            read != size || std::memcmp(verify.data(), bytes, size) != 0)
        {
            g_lastError = ERROR_WRITE_FAULT;
            return false;
        }
        return true;
    }

    bool PatchRemotePointer(HANDLE process, LPVOID slot, LPVOID replacement,
                            fn_ReadProcessMemory readProcessMemory,
                            fn_VirtualProtectEx virtualProtectEx,
                            fn_WriteProcessMemory writeProcessMemory)
    {
        const ULONG_PTR value = reinterpret_cast<ULONG_PTR>(replacement);
        return WriteRemoteBytes(process, slot, &value, sizeof(value),
                                readProcessMemory, writeProcessMemory,
                                virtualProtectEx, nullptr);
    }

    void CaptureLastError(HMODULE kernelBase)
    {
        auto getLastError = reinterpret_cast<fn_GetLastError>(
            PebGetProcAddr(kernelBase, ENC("GetLastError")));
        g_lastError = getLastError ? getLastError() : ERROR_GEN_FAILURE;
    }

    HMODULE GetKernelBase()
    {
        HMODULE module = static_cast<HMODULE>(PebGetModuleBase(WENC(L"kernelbase.dll")));
        if (!module)
            module = static_cast<HMODULE>(PebGetModuleBase(WENC(L"kernel32.dll")));
        return module;
    }

    HMODULE GetAdvapi32(HMODULE kernelBase)
    {
        HMODULE module = static_cast<HMODULE>(PebGetModuleBase(WENC(L"advapi32.dll")));
        if (module)
            return module;

        auto loadLibraryW = reinterpret_cast<fn_LoadLibraryW>(
            PebGetProcAddr(kernelBase, ENC("LoadLibraryW")));
        return loadLibraryW ? loadLibraryW(WENC(L"advapi32.dll")) : nullptr;
    }

    bool EnableDebugPrivilege(HMODULE kernelBase)
    {
        HMODULE advapi32 = GetAdvapi32(kernelBase);
        if (!advapi32)
        {
            g_lastError = ERROR_MOD_NOT_FOUND;
            return false;
        }

        auto openProcessToken = reinterpret_cast<fn_OpenProcessToken>(
            PebGetProcAddr(advapi32, ENC("OpenProcessToken")));
        auto lookupPrivilegeValue = reinterpret_cast<fn_LookupPrivilegeValueW>(
            PebGetProcAddr(advapi32, ENC("LookupPrivilegeValueW")));
        auto adjustTokenPrivileges = reinterpret_cast<fn_AdjustTokenPrivileges>(
            PebGetProcAddr(advapi32, ENC("AdjustTokenPrivileges")));
        auto closeHandle = reinterpret_cast<fn_CloseHandle>(
            PebGetProcAddr(kernelBase, ENC("CloseHandle")));
        if (!openProcessToken || !lookupPrivilegeValue ||
            !adjustTokenPrivileges || !closeHandle)
        {
            g_lastError = ERROR_PROC_NOT_FOUND;
            return false;
        }

        HANDLE token = nullptr;
        if (!openProcessToken(
                reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1)),
                TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                &token))
        {
            CaptureLastError(kernelBase);
            return false;
        }

        LUID luid{};
        if (!lookupPrivilegeValue(nullptr, WENC(L"SeDebugPrivilege"), &luid))
        {
            CaptureLastError(kernelBase);
            closeHandle(token);
            return false;
        }

        TOKEN_PRIVILEGES privileges{};
        privileges.PrivilegeCount = 1;
        privileges.Privileges[0].Luid = luid;
        privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        const BOOL adjusted = adjustTokenPrivileges(
            token, FALSE, &privileges, sizeof(privileges), nullptr, nullptr);
        CaptureLastError(kernelBase);
        closeHandle(token);
        return adjusted && g_lastError == ERROR_SUCCESS;
    }

    DWORD FindSysMainPid(HMODULE kernelBase)
    {
        HMODULE advapi32 = GetAdvapi32(kernelBase);
        if (!advapi32)
            return 0;

        auto openScm = reinterpret_cast<fn_OpenSCManagerW>(
            PebGetProcAddr(advapi32, ENC("OpenSCManagerW")));
        auto openService = reinterpret_cast<fn_OpenServiceW>(
            PebGetProcAddr(advapi32, ENC("OpenServiceW")));
        auto queryStatus = reinterpret_cast<fn_QueryServiceStatusEx>(
            PebGetProcAddr(advapi32, ENC("QueryServiceStatusEx")));
        auto closeService = reinterpret_cast<fn_CloseServiceHandle>(
            PebGetProcAddr(advapi32, ENC("CloseServiceHandle")));
        if (!openScm || !openService || !queryStatus || !closeService)
            return 0;

        SC_HANDLE scm = openScm(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!scm)
            return 0;

        SC_HANDLE service = openService(scm, WENC(L"SysMain"), SERVICE_QUERY_STATUS);
        if (!service)
        {
            closeService(scm);
            return 0;
        }

        SERVICE_STATUS_PROCESS status{};
        DWORD needed = 0;
        queryStatus(
            service,
            SC_STATUS_PROCESS_INFO,
            reinterpret_cast<LPBYTE>(&status),
            sizeof(status),
            &needed);

        const DWORD pid = status.dwCurrentState == SERVICE_RUNNING
            ? status.dwProcessId
            : 0;

        closeService(service);
        closeService(scm);
        return pid;
    }

}

namespace NtCreateFileHook
{
    HANDLE g_process = nullptr;
    LPVOID g_target = nullptr;
    BYTE g_original[kPatchSize]{};
    bool g_installed = false;
    uintptr_t g_remoteNtdll = 0;
    std::array<RemoteIatPatch, kMaxIatPatches> g_iatPatches{};
    std::size_t g_iatPatchCount = 0;
    std::array<LPVOID, kMaxIatPatches> g_remoteStubs{};
    std::size_t g_remoteStubCount = 0;

    enum class WindowsFamily
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

    using fn_RtlGetVersion = LONG(WINAPI*)(POSVERSIONINFOW);

    WindowsVersion DetectWindowsVersion(HMODULE ntdll)
    {
        if (!ntdll)
            return {};

        auto rtlGetVersion = reinterpret_cast<fn_RtlGetVersion>(
            PebGetProcAddr(ntdll, ENC("RtlGetVersion")));
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

        // Windows 11 keeps the 10.0 version tuple and starts at build 22000.
        if (version.dwBuildNumber >= 22000)
            return { WindowsFamily::Windows11, version.dwBuildNumber };
        if (version.dwBuildNumber >= 10240)
            return { WindowsFamily::Windows10, version.dwBuildNumber };

        return { WindowsFamily::Unknown, version.dwBuildNumber };
    }

    // IDA-derived signature for PfSvWriteBufferEx.  The stack-home offsets and
    // allocation size are wildcarded so the locator does not depend on a
    // version-specific RVA or one exact compiler frame layout.
    constexpr BYTE kPfSvWriteBufferExPattern[] = {
        0x48, 0x89, 0x5C, 0x24, 0x00,
        0x48, 0x89, 0x6C, 0x24, 0x00,
        0x48, 0x89, 0x74, 0x24, 0x00,
        0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
        0x48, 0x83, 0xEC, 0x00,
        0x45, 0x0F, 0xB7, 0xD1
    };
    constexpr char kPfSvWriteBufferExMask[] =
        "xxxx?xxxx?xxxx?xxxxxxxxxxxx?xxxx";
    constexpr RemotePattern kPfSvWriteBufferExSignature = {
        kPfSvWriteBufferExPattern,
        kPfSvWriteBufferExMask,
        sizeof(kPfSvWriteBufferExPattern)
    };

    // Windows 11 22H2/23H2 SysMain signature.  Keep the generic signature as
    // the servicing-update fallback and never rely on a fixed RVA.
    constexpr BYTE kPfSvWriteBufferExWin11Pattern[] = {
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
    constexpr char kPfSvWriteBufferExWin11Mask[] =
        "xxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxx";
    constexpr RemotePattern kPfSvWriteBufferExWin11Signature = {
        kPfSvWriteBufferExWin11Pattern,
        kPfSvWriteBufferExWin11Mask,
        sizeof(kPfSvWriteBufferExWin11Pattern)
    };

    // When a previous broker died after applying the patch, the first twelve
    // bytes are replaced but the remainder of the signature is still intact.
    // This recovery pattern is deliberately just as strict as the normal one.
    constexpr BYTE kPfSvWriteBufferExBlockedPattern[] = {
        0x31, 0xC0, 0xC3,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x74, 0x24, 0x00,
        0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
        0x48, 0x83, 0xEC, 0x00,
        0x45, 0x0F, 0xB7, 0xD1
    };
    constexpr char kPfSvWriteBufferExBlockedMask[] =
        "xxxxxxxxxxxxxx?xxxxxxxxxxxx?xxxx";
    constexpr RemotePattern kPfSvWriteBufferExBlockedSignature = {
        kPfSvWriteBufferExBlockedPattern,
        kPfSvWriteBufferExBlockedMask,
        sizeof(kPfSvWriteBufferExBlockedPattern)
    };

    constexpr BYTE kPfSvWriteBufferExLegacyPatch[kPatchSize] = {
        0x31, 0xC0, 0xC3,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    constexpr BYTE kPfSvWriteBufferExBlockedLegacyPattern[] = {
        0xB8, 0x05, 0x00, 0x00, 0x00,
        0xC3,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x74, 0x24, 0x00,
        0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
        0x48, 0x83, 0xEC, 0x00,
        0x45, 0x0F, 0xB7, 0xD1
    };
    constexpr char kPfSvWriteBufferExBlockedLegacyMask[] =
        "xxxxxxxxxxxxxx?xxxxxxxxxxxx?xxxx";
    constexpr RemotePattern kPfSvWriteBufferExBlockedLegacySignature = {
        kPfSvWriteBufferExBlockedLegacyPattern,
        kPfSvWriteBufferExBlockedLegacyMask,
        sizeof(kPfSvWriteBufferExBlockedLegacyPattern)
    };

    BYTE g_pfOriginal[kPatchSize]{};
    LPVOID g_pfTarget = nullptr;
    bool g_pfInstalled = false;

    constexpr BYTE kPfSvWriteBufferExPatch[kPatchSize] = {
        0xB8, 0x05, 0x00, 0x00, 0x00,
        0xC3,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };

    bool RestoreRemotePointer(const RemoteIatPatch& patch, bool force,
                              fn_ReadProcessMemory readProcessMemory,
                              fn_WriteProcessMemory writeProcessMemory,
                              fn_VirtualProtectEx virtualProtectEx)
    {
        if (!g_process || !patch.slot || !readProcessMemory ||
            !writeProcessMemory || !virtualProtectEx)
            return false;

        ULONG_PTR current = 0;
        SIZE_T read = 0;
        if (!readProcessMemory(g_process, patch.slot, &current,
                               sizeof(current), &read) ||
            read != sizeof(current))
            return false;

        if (current == patch.original)
            return true;
        if (!force &&
            current != reinterpret_cast<ULONG_PTR>(patch.replacement))
            return true;

        const ULONG_PTR original = patch.original;
        return WriteRemoteBytes(g_process, patch.slot, &original,
                                sizeof(original), readProcessMemory,
                                writeProcessMemory, virtualProtectEx, nullptr);
    }

    bool RemoveIatHooks(bool force,
                        fn_ReadProcessMemory readProcessMemory,
                        fn_VirtualProtectEx virtualProtectEx,
                        fn_WriteProcessMemory writeProcessMemory,
                        fn_VirtualFreeEx virtualFreeEx)
    {
        if (g_iatPatchCount == 0)
        {
            if (g_process && virtualFreeEx)
            {
                for (std::size_t i = 0; i < g_remoteStubCount; ++i)
                {
                    if (g_remoteStubs[i])
                        virtualFreeEx(g_process, g_remoteStubs[i], 0,
                                      MEM_RELEASE);
                }
                g_remoteStubs = {};
                g_remoteStubCount = 0;
            }
            return true;
        }

        if (!g_process)
            return false;

        std::array<RemoteIatPatch, kMaxIatPatches> remaining{};
        std::size_t remainingCount = 0;
        bool allRestored = true;
        for (std::size_t i = g_iatPatchCount; i != 0; --i)
        {
            const RemoteIatPatch& patch = g_iatPatches[i - 1];
            if (!RestoreRemotePointer(patch, force, readProcessMemory,
                                      writeProcessMemory, virtualProtectEx))
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

        if (virtualFreeEx)
        {
            for (std::size_t i = 0; i < g_remoteStubCount; ++i)
            {
                if (g_remoteStubs[i])
                    virtualFreeEx(g_process, g_remoteStubs[i], 0,
                                  MEM_RELEASE);
            }
        }
        g_remoteStubs = {};
        g_remoteStubCount = 0;
        return true;
    }

    bool InstallPfSvWriteBufferHook(
        uintptr_t sysmainBase,
        WindowsFamily windowsFamily,
        fn_ReadProcessMemory readProcessMemory,
        fn_WriteProcessMemory writeProcessMemory,
        fn_VirtualProtectEx virtualProtectEx,
        fn_FlushInstructionCache flushInstructionCache)
    {
        if (!g_process || !sysmainBase || !readProcessMemory ||
            !writeProcessMemory || !virtualProtectEx)
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
                readProcessMemory,
                g_process,
                sysmainBase,
                kPfSvWriteBufferExWin11Signature);
            if (!target)
            {
                target = FindRemotePattern(
                    readProcessMemory,
                    g_process,
                    sysmainBase,
                    kPfSvWriteBufferExSignature);
            }
        }

        if (!target)
        {
            target = FindRemotePattern(
                readProcessMemory,
                g_process,
                sysmainBase,
                kPfSvWriteBufferExSignature);
        }

        if (!target)
        {
            target = FindRemotePattern(
                readProcessMemory,
                g_process,
                sysmainBase,
                kPfSvWriteBufferExBlockedSignature);
            residualPatch = target != 0;
        }
        if (!target)
        {
            target = FindRemotePattern(
                readProcessMemory,
                g_process,
                sysmainBase,
                kPfSvWriteBufferExBlockedLegacySignature);
            residualPatch = target != 0;
        }
        if (!target)
        {
            g_lastError = ERROR_BAD_EXE_FORMAT;
            return false;
        }

        g_pfTarget = reinterpret_cast<LPVOID>(target);
        BYTE current[15]{};
        SIZE_T read = 0;
        if (!readProcessMemory(g_process, g_pfTarget, current,
                               sizeof(current), &read) ||
            read != sizeof(current))
        {
            g_lastError = ERROR_PARTIAL_COPY;
            g_pfTarget = nullptr;
            return false;
        }

        const bool isBlocked = std::memcmp(
            current, kPfSvWriteBufferExPatch,
            sizeof(kPfSvWriteBufferExPatch)) == 0 ||
            std::memcmp(current, kPfSvWriteBufferExLegacyPatch,
                        sizeof(kPfSvWriteBufferExLegacyPatch)) == 0;

        if (isBlocked)
        {
            // The third stack-home displacement remains visible after the
            // twelve-byte patch.  Use it to reconstruct the two overwritten
            // displacements instead of restoring one fixed Windows build.
            if (current[12] != 0x74 || current[13] != 0x24 ||
                current[14] < 0x10 || current[14] > 0xF8 ||
                (current[14] & 7) != 0)
            {
                g_lastError = ERROR_BAD_EXE_FORMAT;
                return false;
            }

            const BYTE expected[kPatchSize] = {
                0x48, 0x89, 0x5C, 0x24,
                static_cast<BYTE>(current[14] - 0x10),
                0x48, 0x89, 0x6C, 0x24,
                static_cast<BYTE>(current[14] - 0x08),
                0x48, 0x89
            };
            std::memcpy(g_pfOriginal, expected, sizeof(g_pfOriginal));
            g_pfInstalled = true;
            return true;
        }

        if (residualPatch)
        {
            g_lastError = ERROR_BAD_EXE_FORMAT;
            return false;
        }

        const BYTE expectedPrefix[kPatchSize] = {
            0x48, 0x89, 0x5C, 0x24, current[4],
            0x48, 0x89, 0x6C, 0x24, current[9],
            0x48, 0x89
        };
        if (std::memcmp(current, expectedPrefix,
                        sizeof(expectedPrefix)) != 0)
        {
            g_lastError = ERROR_BAD_EXE_FORMAT;
            return false;
        }

        std::memcpy(g_pfOriginal, current, sizeof(g_pfOriginal));
        g_pfInstalled = true; // also records a partial install for rollback
        if (!WriteRemoteBytes(g_process, g_pfTarget,
                              kPfSvWriteBufferExPatch, sizeof(kPfSvWriteBufferExPatch),
                              readProcessMemory, writeProcessMemory,
                              virtualProtectEx, flushInstructionCache))
            return false;

        return true;
    }

    bool RemovePfSvWriteBufferHook(
        bool force,
        fn_ReadProcessMemory readProcessMemory,
        fn_VirtualProtectEx virtualProtectEx,
        fn_WriteProcessMemory writeProcessMemory,
        fn_FlushInstructionCache flushInstructionCache)
    {
        if (!g_pfInstalled)
            return true;
        if (!g_process || !g_pfTarget || !readProcessMemory ||
            !virtualProtectEx || !writeProcessMemory)
            return false;

        BYTE current[kPatchSize]{};
        SIZE_T read = 0;
        if (!readProcessMemory(g_process, g_pfTarget, current,
                               sizeof(current), &read) ||
            read != sizeof(current))
            return false;

        if (std::memcmp(current, g_pfOriginal, sizeof(g_pfOriginal)) == 0)
        {
            g_pfInstalled = false;
            g_pfTarget = nullptr;
            return true;
        }

        if (!force &&
            std::memcmp(current, kPfSvWriteBufferExPatch,
                        sizeof(current)) != 0 &&
            std::memcmp(current, kPfSvWriteBufferExLegacyPatch,
                        sizeof(current)) != 0)
        {
            // Never overwrite a new/unknown function body with a stale
            // prologue from a previous SysMain image.
            g_lastError = ERROR_INVALID_DATA;
            g_pfInstalled = false;
            g_pfTarget = nullptr;
            return true;
        }

        if (!WriteRemoteBytes(g_process, g_pfTarget, g_pfOriginal,
                              sizeof(g_pfOriginal), readProcessMemory,
                              writeProcessMemory, virtualProtectEx,
                              flushInstructionCache))
            return false;

        g_pfInstalled = false;
        g_pfTarget = nullptr;
        return true;
    }

    bool InstallIatHooks(
        HMODULE kernelBase,
        HANDLE process,
        uintptr_t sysmainBase,
        fn_ReadProcessMemory readProcessMemory,
        fn_WriteProcessMemory writeProcessMemory,
        fn_VirtualProtectEx virtualProtectEx,
        fn_VirtualAllocEx virtualAllocEx,
        fn_VirtualFreeEx virtualFreeEx,
        fn_FlushInstructionCache flushInstructionCache,
        WindowsFamily windowsFamily)
    {
        UNREFERENCED_PARAMETER(kernelBase);
        UNREFERENCED_PARAMETER(process);
        UNREFERENCED_PARAMETER(virtualAllocEx);
        UNREFERENCED_PARAMETER(virtualFreeEx);

        // Do not intercept SysMain's IAT.  Blocking every CreateFileW/
        // WriteFile import was observed to corrupt unrelated SysMain state.
        // The signature-located final writer and the validated ntdll syscall
        // patch are the only hooks used on the remote service host.
        return InstallPfSvWriteBufferHook(
            sysmainBase, windowsFamily, readProcessMemory,
            writeProcessMemory, virtualProtectEx, flushInstructionCache);
    }

    constexpr BYTE kNtCreateFilePatch[kPatchSize] = {
        0xB8, 0x22, 0x00, 0x00, 0xC0,
        0xC3,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };

    constexpr SIZE_T kNtSyscallProbeSize = 32;

    bool LooksLikeNtSyscallStub(const BYTE* bytes, SIZE_T size)
    {
        if (!bytes || size < 11 || bytes[0] != 0x4C ||
            bytes[1] != 0x8B || bytes[2] != 0xD1 || bytes[3] != 0xB8)
            return false;

        // Windows can place a CFG/retpoline check before the syscall.
        // Match the instruction sequence, not its version-specific offset.
        for (SIZE_T i = 4; i + 2 < size; ++i)
        {
            if (bytes[i] == 0x0F && bytes[i + 1] == 0x05 &&
                bytes[i + 2] == 0xC3)
                return true;
        }
        return false;
    }

    bool HasPendingState()
    {
        return g_process || g_target || g_installed || g_pfInstalled ||
               g_iatPatchCount != 0 || g_remoteStubCount != 0;
    }

    void ResetHookState()
    {
        g_process = nullptr;
        g_target = nullptr;
        g_remoteNtdll = 0;
        g_targetPid = 0;
        g_installed = false;
        g_pfTarget = nullptr;
        g_pfInstalled = false;
        g_iatPatches = {};
        g_iatPatchCount = 0;
        g_remoteStubs = {};
        g_remoteStubCount = 0;
        std::memset(g_original, 0, sizeof(g_original));
        std::memset(g_pfOriginal, 0, sizeof(g_pfOriginal));
    }

    bool RestoreNtCreateFileHook(
        bool force,
        fn_ReadProcessMemory readProcessMemory,
        fn_WriteProcessMemory writeProcessMemory,
        fn_VirtualProtectEx virtualProtectEx,
        fn_FlushInstructionCache flushInstructionCache)
    {
        if (!g_installed)
            return true;
        if (!g_process || !g_target || !readProcessMemory ||
            !writeProcessMemory || !virtualProtectEx)
            return false;

        BYTE current[kPatchSize]{};
        SIZE_T read = 0;
        if (!readProcessMemory(g_process, g_target, current,
                               sizeof(current), &read) ||
            read != sizeof(current))
            return false;

        if (std::memcmp(current, g_original, sizeof(g_original)) == 0)
            return true;

        if (!force && std::memcmp(current, kNtCreateFilePatch,
                                  sizeof(kNtCreateFilePatch)) != 0)
        {
            // A restarted service host or another component may have
            // replaced the syscall stub.  Do not write stale bytes over an
            // unknown implementation; our patch is no longer active.
            return true;
        }

        return WriteRemoteBytes(g_process, g_target, g_original,
                                sizeof(g_original), readProcessMemory,
                                writeProcessMemory, virtualProtectEx,
                                flushInstructionCache);
    }

    void Remove(bool force);

    bool Install()
    {
        if (g_installed)
            return true;

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

        HMODULE kernelBase = GetKernelBase();
        HMODULE ntdll = static_cast<HMODULE>(PebGetModuleBase(WENC(L"ntdll.dll")));
        if (!kernelBase || !ntdll)
        {
            g_lastError = ERROR_MOD_NOT_FOUND;
            return false;
        }

        const WindowsVersion windows = DetectWindowsVersion(ntdll);
        if (windows.family == WindowsFamily::Unknown)
        {
            g_lastError = ERROR_OLD_WIN_VERSION;
            return false;
        }

        auto openProcess = reinterpret_cast<fn_OpenProcess>(
            PebGetProcAddr(kernelBase, ENC("OpenProcess")));
        auto readProcessMemory = reinterpret_cast<fn_ReadProcessMemory>(
            PebGetProcAddr(kernelBase, ENC("ReadProcessMemory")));
        auto writeProcessMemory = reinterpret_cast<fn_WriteProcessMemory>(
            PebGetProcAddr(kernelBase, ENC("WriteProcessMemory")));
        auto virtualProtectEx = reinterpret_cast<fn_VirtualProtectEx>(
            PebGetProcAddr(kernelBase, ENC("VirtualProtectEx")));
        auto virtualAllocEx = reinterpret_cast<fn_VirtualAllocEx>(
            PebGetProcAddr(kernelBase, ENC("VirtualAllocEx")));
        auto virtualFreeEx = reinterpret_cast<fn_VirtualFreeEx>(
            PebGetProcAddr(kernelBase, ENC("VirtualFreeEx")));
        auto flushInstructionCache = reinterpret_cast<fn_FlushInstructionCache>(
            PebGetProcAddr(kernelBase, ENC("FlushInstructionCache")));
        auto closeHandle = reinterpret_cast<fn_CloseHandle>(
            PebGetProcAddr(kernelBase, ENC("CloseHandle")));
        auto target = PebGetProcAddr(ntdll, ENC("NtCreateFile"));
        if (!openProcess || !readProcessMemory || !writeProcessMemory ||
            !virtualProtectEx || !virtualAllocEx || !virtualFreeEx ||
            !closeHandle || !target)
        {
            g_lastError = ERROR_PROC_NOT_FOUND;
            return false;
        }

        auto abandon = [&]()
        {
            if (g_process && closeHandle)
                closeHandle(g_process);
            ResetHookState();
        };

        EnableDebugPrivilege(kernelBase);
        const DWORD pid = FindSysMainPid(kernelBase);
        if (!pid)
        {
            CaptureLastError(kernelBase);
            return false;
        }
        g_targetPid = pid;

        g_process = openProcess(
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ |
                PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
            FALSE,
            pid);
        if (!g_process)
        {
            CaptureLastError(kernelBase);
            return false;
        }

        const uintptr_t localNtdll = reinterpret_cast<uintptr_t>(ntdll);
        const uintptr_t localTarget = reinterpret_cast<uintptr_t>(target);
        g_remoteNtdll = FindRemoteModule(kernelBase, pid, WENC(L"ntdll.dll"));
        const uintptr_t remoteSysmain =
            FindRemoteModule(kernelBase, pid, WENC(L"sysmain.dll"));
        if (!g_remoteNtdll || !remoteSysmain || localTarget < localNtdll)
        {
            g_lastError = ERROR_MOD_NOT_FOUND;
            abandon();
            return false;
        }

        // Use the target process' module bases instead of assuming that every
        // DLL is mapped at the same address in every process.
        g_target = reinterpret_cast<LPVOID>(
            g_remoteNtdll + (localTarget - localNtdll));
        SIZE_T read = 0;
        if (!readProcessMemory(g_process, g_target, g_original, kPatchSize, &read) ||
            read != kPatchSize)
        {
            CaptureLastError(kernelBase);
            abandon();
            return false;
        }

        BYTE remoteProbe[kNtSyscallProbeSize]{};
        SIZE_T probeRead = 0;
        if (!readProcessMemory(g_process, g_target, remoteProbe,
                               sizeof(remoteProbe), &probeRead) ||
            probeRead != sizeof(remoteProbe))
        {
            CaptureLastError(kernelBase);
            abandon();
            return false;
        }

        BYTE localOriginal[kNtSyscallProbeSize]{};
        std::memcpy(localOriginal, target, sizeof(localOriginal));
        if (!LooksLikeNtSyscallStub(remoteProbe, sizeof(remoteProbe)) &&
            std::memcmp(g_original, kNtCreateFilePatch,
                        sizeof(kNtCreateFilePatch)) != 0)
        {
            g_lastError = ERROR_BAD_EXE_FORMAT;
            abandon();
            return false;
        }

        if (std::memcmp(g_original, kNtCreateFilePatch,
                        sizeof(kNtCreateFilePatch)) == 0)
        {
            // Adopt a patch left by a previous helper only when the local
            // syscall stub gives us a trustworthy restore image.
            if (!LooksLikeNtSyscallStub(localOriginal,
                                        sizeof(localOriginal)))
            {
                g_lastError = ERROR_BAD_EXE_FORMAT;
                abandon();
                return false;
            }
            std::memcpy(g_original, localOriginal, sizeof(g_original));
            g_installed = true;
        }
        else
        {
            g_installed = true; // marks a partial write for rollback
            if (!WriteRemoteBytes(g_process, g_target, kNtCreateFilePatch,
                                  sizeof(kNtCreateFilePatch),
                                  readProcessMemory, writeProcessMemory,
                                  virtualProtectEx, flushInstructionCache))
            {
                Remove(true);
                return false;
            }
        }

        if (!InstallIatHooks(kernelBase, g_process, remoteSysmain,
                             readProcessMemory, writeProcessMemory,
                             virtualProtectEx, virtualAllocEx, virtualFreeEx,
                             flushInstructionCache, windows.family))
        {
            Remove(true);
            return false;
        }

        g_lastError = ERROR_SUCCESS;
        return true;
    }

    void Remove(bool force)
    {
        if (!HasPendingState())
            return;

        HMODULE kernelBase = GetKernelBase();
        auto readProcessMemory = kernelBase
            ? reinterpret_cast<fn_ReadProcessMemory>(
                PebGetProcAddr(kernelBase, ENC("ReadProcessMemory")))
            : nullptr;
        auto writeProcessMemory = kernelBase
            ? reinterpret_cast<fn_WriteProcessMemory>(
                PebGetProcAddr(kernelBase, ENC("WriteProcessMemory")))
            : nullptr;
        auto virtualProtectEx = kernelBase
            ? reinterpret_cast<fn_VirtualProtectEx>(
                PebGetProcAddr(kernelBase, ENC("VirtualProtectEx")))
            : nullptr;
        auto virtualFreeEx = kernelBase
            ? reinterpret_cast<fn_VirtualFreeEx>(
                PebGetProcAddr(kernelBase, ENC("VirtualFreeEx")))
            : nullptr;
        auto flushInstructionCache = kernelBase
            ? reinterpret_cast<fn_FlushInstructionCache>(
                PebGetProcAddr(kernelBase, ENC("FlushInstructionCache")))
            : nullptr;
        auto waitForSingleObject = kernelBase
            ? reinterpret_cast<fn_WaitForSingleObject>(
                PebGetProcAddr(kernelBase, ENC("WaitForSingleObject")))
            : nullptr;
        auto closeHandle = kernelBase
            ? reinterpret_cast<fn_CloseHandle>(
                PebGetProcAddr(kernelBase, ENC("CloseHandle")))
            : nullptr;

        if (!g_process)
            return;

        for (int attempt = 0; attempt < 8; ++attempt)
        {
            const DWORD processState = waitForSingleObject
                ? waitForSingleObject(g_process, 0)
                : WAIT_TIMEOUT;
            if (processState == WAIT_OBJECT_0 ||
                processState == WAIT_ABANDONED)
            {
                if (closeHandle)
                    closeHandle(g_process);
                ResetHookState();
                return;
            }

            const bool pfOk = RemovePfSvWriteBufferHook(
                force,
                readProcessMemory, virtualProtectEx, writeProcessMemory,
                flushInstructionCache);
            const bool iatOk = RemoveIatHooks(
                force,
                readProcessMemory, virtualProtectEx, writeProcessMemory,
                virtualFreeEx);
            const bool ntOk = RestoreNtCreateFileHook(
                force,
                readProcessMemory, writeProcessMemory, virtualProtectEx,
                flushInstructionCache);
            if (pfOk && iatOk && ntOk)
            {
                if (closeHandle)
                    closeHandle(g_process);
                ResetHookState();
                return;
            }

            if (attempt + 1 < 8)
                Sleep(100);
        }

        // Preserve the process handle, target and original bytes for a later
        // Remove() call.  The old implementation cleared this state even
        // when WriteProcessMemory failed, leaving SysMain permanently patched.
        g_lastError = ERROR_RETRY;
    }

    bool Pending()
    {
        return HasPendingState();
    }

    DWORD LastError()
    {
        return g_lastError;
    }

    DWORD TargetPid()
    {
        return g_targetPid;
    }
}
