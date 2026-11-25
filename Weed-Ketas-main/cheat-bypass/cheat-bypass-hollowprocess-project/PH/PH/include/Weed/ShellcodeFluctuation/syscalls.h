#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <windows.h>
#include "strenc.h"

#pragma warning(push)
#pragma warning(disable: 4005)

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS              ((NTSTATUS)0x00000000L)
#endif
#ifndef STATUS_INFO_LENGTH_MISMATCH
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)
#endif

#pragma warning(pop)

#define SystemProcessInformation 5

typedef LONG NTSTATUS;

#ifndef NT_SUCCESS
#define NT_SUCCESS(st) (((NTSTATUS)(st)) >= 0)
#endif

#ifndef MY_UNICODE_STRING_DEFINED
#define MY_UNICODE_STRING_DEFINED
typedef struct _MY_UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} MY_UNICODE_STRING, *PMY_UNICODE_STRING;
#endif

#ifndef MY_OBJECT_ATTRIBUTES_DEFINED
#define MY_OBJECT_ATTRIBUTES_DEFINED
typedef struct _MY_OBJECT_ATTRIBUTES {
    ULONG              Length;
    HANDLE             RootDirectory;
    PMY_UNICODE_STRING ObjectName;
    ULONG              Attributes;
    PVOID              SecurityDescriptor;
    PVOID              SecurityQualityOfService;
} MY_OBJECT_ATTRIBUTES, *PMY_OBJECT_ATTRIBUTES;
#endif

#ifndef MY_CLIENT_ID_DEFINED
#define MY_CLIENT_ID_DEFINED
typedef struct _MY_CLIENT_ID {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} MY_CLIENT_ID, *PMY_CLIENT_ID;
#endif

#ifndef MY_PS_ATTRIBUTE_DEFINED
#define MY_PS_ATTRIBUTE_DEFINED
typedef struct _MY_PS_ATTRIBUTE {
    ULONG_PTR Attribute;
    SIZE_T    Size;
    union {
        ULONG_PTR Value;
        PVOID     ValuePtr;
    };
    PSIZE_T ReturnLength;
} MY_PS_ATTRIBUTE, *PMY_PS_ATTRIBUTE;

typedef struct _MY_PS_ATTRIBUTE_LIST {
    SIZE_T          TotalLength;
    MY_PS_ATTRIBUTE Attributes[1];
} MY_PS_ATTRIBUTE_LIST, *PMY_PS_ATTRIBUTE_LIST;
#endif

#ifndef MY_SYSTEM_PROCESS_INFORMATION_DEFINED
#define MY_SYSTEM_PROCESS_INFORMATION_DEFINED

// Estrutura para informações de thread
typedef struct _MY_SYSTEM_THREAD_INFORMATION {
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER CreateTime;
    ULONG         WaitTime;
    PVOID         StartAddress;
    MY_CLIENT_ID  ClientId;
    LONG          Priority;
    LONG          BasePriority;
    ULONG         ContextSwitchCount;
    ULONG         ThreadState;      // 0 = Initialized, 1 = Ready, 2 = Running, etc
    ULONG         WaitReason;
    ULONG         Pad;
} MY_SYSTEM_THREAD_INFORMATION, *PMY_SYSTEM_THREAD_INFORMATION;

typedef struct _MY_SYSTEM_PROCESS_INFORMATION {
    ULONG             NextEntryOffset;          // 0x00
    ULONG             NumberOfThreads;          // 0x04
    LARGE_INTEGER     WorkingSetPrivateSize;    // 0x08
    ULONG             HardFaultCount;           // 0x10
    ULONG             NumberOfThreadsHighWatermark; // 0x14
    ULONGLONG         CycleTime;                // 0x18
    LARGE_INTEGER     CreateTime;               // 0x20
    LARGE_INTEGER     UserTime;                 // 0x28
    LARGE_INTEGER     KernelTime;               // 0x30
    MY_UNICODE_STRING ImageName;                // 0x38
    LONG              BasePriority;             // 0x48
    HANDLE            UniqueProcessId;          // 0x50
    HANDLE            InheritedFromUniqueProcessId; // 0x58
    ULONG             HandleCount;              // 0x60
    ULONG             SessionId;                // 0x64
    ULONG_PTR         UniqueProcessKey;         // 0x68
    SIZE_T            PeakVirtualSize;          // 0x70
    SIZE_T            VirtualSize;              // 0x78
    ULONG             PageFaultCount;           // 0x80
    ULONG             Pad1;                     // 0x84
    SIZE_T            PeakWorkingSetSize;       // 0x88
    SIZE_T            WorkingSetSize;           // 0x90
    SIZE_T            QuotaPeakPagedPoolUsage;  // 0x98
    SIZE_T            QuotaPagedPoolUsage;      // 0xA0
    SIZE_T            QuotaPeakNonPagedPoolUsage; // 0xA8
    SIZE_T            QuotaNonPagedPoolUsage;   // 0xB0
    SIZE_T            PagefileUsage;            // 0xB8
    SIZE_T            PeakPagefileUsage;        // 0xC0
    SIZE_T            PrivatePageCount;         // 0xC8
    LARGE_INTEGER     ReadOperationCount;       // 0xD0
    LARGE_INTEGER     WriteOperationCount;      // 0xD8
    LARGE_INTEGER     OtherOperationCount;      // 0xE0
    LARGE_INTEGER     ReadTransferCount;        // 0xE8
    LARGE_INTEGER     WriteTransferCount;       // 0xF0
    LARGE_INTEGER     OtherTransferCount;       // 0xF8
    // Threads aparecem logo apos esta estrutura
} MY_SYSTEM_PROCESS_INFORMATION, *PMY_SYSTEM_PROCESS_INFORMATION;
#endif

typedef struct _SYSCALL_ENTRY {
    DWORD ssn;
    PVOID syscallAddress;
} SYSCALL_ENTRY, *PSYSCALL_ENTRY;

inline PVOID PebGetModuleBase(const wchar_t* modName)
{
    // Estruturas internas do Windows para navegacao PEB
    typedef struct _PEB_LDR_DATA_INTERNAL {
        ULONG      Length;
        BOOLEAN    Initialized;
        PVOID      SsHandle;
        LIST_ENTRY InLoadOrderModuleList;
        LIST_ENTRY InMemoryOrderModuleList;
        LIST_ENTRY InInitializationOrderModuleList;
    } PEB_LDR_DATA_INTERNAL;

    typedef struct _LDR_DATA_TABLE_ENTRY_INTERNAL {
        LIST_ENTRY        InLoadOrderLinks;           // 0x00
        LIST_ENTRY        InMemoryOrderLinks;         // 0x10
        LIST_ENTRY        InInitializationOrderLinks; // 0x20
        PVOID             DllBase;                    // 0x30
        PVOID             EntryPoint;                 // 0x38
        ULONG             SizeOfImage;                // 0x40
        MY_UNICODE_STRING FullDllName;                // 0x48
        MY_UNICODE_STRING BaseDllName;                // 0x58
        // ... mais campos existem mas não precisamos deles
    } LDR_DATA_TABLE_ENTRY_INTERNAL;

#if defined(_WIN64)
    // x64: PEB em gs:[0x60]
    BYTE* peb = (BYTE*)__readgsqword(0x60);
    if (!peb) return nullptr;
    
    // PEB->Ldr em offset 0x18
    PEB_LDR_DATA_INTERNAL* ldr = *(PEB_LDR_DATA_INTERNAL**)(peb + 0x18);
#else
    // x86: PEB em fs:[0x30]
    BYTE* peb = (BYTE*)__readfsdword(0x30);
    if (!peb) return nullptr;
    
    PEB_LDR_DATA_INTERNAL* ldr = *(PEB_LDR_DATA_INTERNAL**)(peb + 0x0C);
#endif

    if (!ldr) return nullptr;

    // Usar InLoadOrderModuleList para navegacao
    LIST_ENTRY* head = &ldr->InLoadOrderModuleList;
    LIST_ENTRY* cur = head->Flink;

    while (cur && cur != head)
    {
        // CONTAINING_RECORD: cur aponta para InLoadOrderLinks, que e o primeiro campo
        LDR_DATA_TABLE_ENTRY_INTERNAL* entry = (LDR_DATA_TABLE_ENTRY_INTERNAL*)cur;
        
        if (entry->BaseDllName.Buffer && entry->BaseDllName.Length > 0 && entry->DllBase)
        {
            const wchar_t* a = modName;
            const wchar_t* b = entry->BaseDllName.Buffer;
            BOOL match = TRUE;
            
            while (*a && *b)
            {
                wchar_t ca = (*a >= L'A' && *a <= L'Z') ? (*a + 32) : *a;
                wchar_t cb = (*b >= L'A' && *b <= L'Z') ? (*b + 32) : *b;
                if (ca != cb) { match = FALSE; break; }
                a++; b++;
            }
            
            // Match completo: modName terminou e b terminou ou tem extensao
            if (match && *a == 0)
            {
                return entry->DllBase;
            }
        }
        cur = cur->Flink;
    }
    return nullptr;
}

inline PVOID PebGetProcAddr(PVOID moduleBase, const char* funcName)
{
    if (!moduleBase || !funcName) return nullptr;

    BYTE* base = (BYTE*)moduleBase;
    IMAGE_DOS_HEADER*   dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS64* nt  = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);

    DWORD expRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!expRva) return nullptr;

    IMAGE_EXPORT_DIRECTORY* exp = (IMAGE_EXPORT_DIRECTORY*)(base + expRva);
    DWORD* names  = (DWORD*)(base + exp->AddressOfNames);
    WORD*  ords   = (WORD*) (base + exp->AddressOfNameOrdinals);
    DWORD* funcs  = (DWORD*)(base + exp->AddressOfFunctions);

    for (DWORD i = 0; i < exp->NumberOfNames; i++)
    {
        const char* name = (const char*)(base + names[i]);
        const char* a = funcName;
        const char* b = name;
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == 0 && *b == 0)
            return (PVOID)(base + funcs[ords[i]]);
    }
    return nullptr;
}

extern "C" NTSTATUS SysNtAllocateVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress, ULONG_PTR ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect);
extern "C" NTSTATUS SysNtWriteVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, SIZE_T NumberOfBytesToWrite, PSIZE_T NumberOfBytesWritten);
extern "C" NTSTATUS SysNtProtectVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress, PSIZE_T RegionSize, ULONG NewProtect, PULONG OldProtect);
extern "C" NTSTATUS SysNtCreateThreadEx(PHANDLE ThreadHandle, ACCESS_MASK DesiredAccess, PMY_OBJECT_ATTRIBUTES ObjectAttributes, HANDLE ProcessHandle, PVOID StartRoutine, PVOID Argument, ULONG CreateFlags, SIZE_T ZeroBits, SIZE_T StackSize, SIZE_T MaximumStackSize, PMY_PS_ATTRIBUTE_LIST AttributeList);
extern "C" NTSTATUS SysNtOpenProcess(PHANDLE ProcessHandle, ACCESS_MASK DesiredAccess, PMY_OBJECT_ATTRIBUTES ObjectAttributes, PMY_CLIENT_ID ClientId);
extern "C" NTSTATUS SysNtOpenThread(PHANDLE ThreadHandle, ACCESS_MASK DesiredAccess, PMY_OBJECT_ATTRIBUTES ObjectAttributes, PMY_CLIENT_ID ClientId);
extern "C" NTSTATUS SysNtClose(HANDLE Handle);
extern "C" NTSTATUS SysNtQuerySystemInformation(ULONG SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);

extern "C" NTSTATUS SysNtCreateSection(PHANDLE SectionHandle, ACCESS_MASK DesiredAccess, PMY_OBJECT_ATTRIBUTES ObjectAttributes, PLARGE_INTEGER MaximumSize, ULONG SectionPageProtection, ULONG AllocationAttributes, HANDLE FileHandle);
extern "C" NTSTATUS SysNtMapViewOfSection(HANDLE SectionHandle, HANDLE ProcessHandle, PVOID* BaseAddress, ULONG_PTR ZeroBits, SIZE_T CommitSize, PLARGE_INTEGER SectionOffset, PSIZE_T ViewSize, ULONG InheritDisposition, ULONG AllocationType, ULONG Win32Protect);
extern "C" NTSTATUS SysNtUnmapViewOfSection(HANDLE ProcessHandle, PVOID BaseAddress);
extern "C" NTSTATUS SysNtDelayExecution(BOOLEAN Alertable, PLARGE_INTEGER DelayInterval);

BOOL InitializeSyscalls();

#endif
