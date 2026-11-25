#pragma once
#include "Common.h"

// =============================================================================
// SyscallResolver - Indirect syscalls (Hell's Gate style, x64)
//
// Resolve numeros de syscall lendo o prologo das funcoes Nt* no ntdll e acha
// um `syscall; ret` (bytes 0F 05 C3) dentro do .text do ntdll para servir como
// gadget. Os stubs em syscalls.asm pulam pra esse gadget - assim o RIP no
// momento do syscall aponta pra dentro do ntdll.dll.
// =============================================================================

extern "C" {
    // Globais lidas pelos stubs em syscalls.asm
    extern ULONG g_NtAllocateVirtualMemory_num;
    extern ULONG g_NtWriteVirtualMemory_num;
    extern ULONG g_NtReadVirtualMemory_num;
    extern ULONG g_NtSetContextThread_num;
    extern ULONG g_NtGetContextThread_num;
    extern ULONG g_NtResumeThread_num;
    extern ULONG g_NtUnmapViewOfSection_num;
    extern ULONG g_NtCreateFile_num;
    extern ULONG g_NtReadFile_num;
    extern ULONG g_NtClose_num;
    extern PVOID g_syscall_gadget;

#ifdef _M_X64
    // Prototipos dos stubs (definidos em syscalls.asm)
    NTSTATUS NTAPI NtAllocateVirtualMemory_indirect(
        HANDLE ProcessHandle, PVOID* BaseAddress, ULONG_PTR ZeroBits,
        PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect);

    NTSTATUS NTAPI NtWriteVirtualMemory_indirect(
        HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer,
        SIZE_T NumberOfBytesToWrite, PSIZE_T NumberOfBytesWritten);

    NTSTATUS NTAPI NtReadVirtualMemory_indirect(
        HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer,
        SIZE_T NumberOfBytesToRead, PSIZE_T NumberOfBytesRead);

    NTSTATUS NTAPI NtSetContextThread_indirect(HANDLE ThreadHandle, PCONTEXT ThreadContext);
    NTSTATUS NTAPI NtGetContextThread_indirect(HANDLE ThreadHandle, PCONTEXT ThreadContext);
    NTSTATUS NTAPI NtResumeThread_indirect(HANDLE ThreadHandle, PULONG PreviousSuspendCount);
    NTSTATUS NTAPI NtUnmapViewOfSection_indirect(HANDLE ProcessHandle, PVOID BaseAddress);

    // File I/O
    NTSTATUS NTAPI NtCreateFile_indirect(
        PHANDLE FileHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes,
        PIO_STATUS_BLOCK IoStatusBlock, PLARGE_INTEGER AllocationSize, ULONG FileAttributes,
        ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions,
        PVOID EaBuffer, ULONG EaLength);

    NTSTATUS NTAPI NtReadFile_indirect(
        HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine, PVOID ApcContext,
        PIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length,
        PLARGE_INTEGER ByteOffset, PULONG Key);

    NTSTATUS NTAPI NtClose_indirect(HANDLE Handle);
#endif
}

namespace Hollowing {

    class SyscallResolver {
    public:
        static SyscallResolver& GetInstance() {
            static SyscallResolver instance;
            return instance;
        }

        bool ResolveAll();

    private:
        SyscallResolver() {}

        static DWORD ExtractSyscallNumber(PVOID funcPtr);
        static PVOID FindSyscallGadget(PVOID ntdllBase);
    };

}
