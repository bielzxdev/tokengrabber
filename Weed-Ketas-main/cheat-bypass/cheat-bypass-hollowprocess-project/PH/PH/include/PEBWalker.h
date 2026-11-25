#pragma once
#include "Common.h"

// =============================================================================
// PEBWalker - Substituto para GetModuleHandle/GetProcAddress via PEB walking.
//
// Bootstrap: nao precisamos importar GetProcAddress do kernel32. Em vez disso,
// caminhamos PEB.Ldr.InMemoryOrderModuleList (populado pelo loader do NT antes
// do nosso codigo rodar) e parseamos a IMAGE_EXPORT_DIRECTORY manualmente.
//
// x64 apenas (usa __readgsqword pra pegar o TEB.PEB).
// =============================================================================

namespace Hollowing {

    // winternl.h expoe uma LDR_DATA_TABLE_ENTRY reduzida (sem BaseDllName).
    // Definimos a versao completa aqui.
    typedef struct _FULL_LDR_DATA_TABLE_ENTRY {
        LIST_ENTRY     InLoadOrderLinks;
        LIST_ENTRY     InMemoryOrderLinks;
        LIST_ENTRY     InInitializationOrderLinks;
        PVOID          DllBase;
        PVOID          EntryPoint;
        ULONG          SizeOfImage;
        UNICODE_STRING FullDllName;
        UNICODE_STRING BaseDllName;
    } FULL_LDR_DATA_TABLE_ENTRY, *PFULL_LDR_DATA_TABLE_ENTRY;

    // Case-insensitive ASCII compare de UNICODE_STRING vs wchar_t*.
    static inline bool _peb_wstr_ieq(UNICODE_STRING* us, const wchar_t* name) {
        size_t nameLen = 0;
        while (name[nameLen]) nameLen++;
        if ((size_t)(us->Length / sizeof(WCHAR)) != nameLen) return false;
        for (size_t i = 0; i < nameLen; i++) {
            wchar_t a = us->Buffer[i];
            wchar_t b = name[i];
            if (a >= L'A' && a <= L'Z') a = (wchar_t)(a + 32);
            if (b >= L'A' && b <= L'Z') b = (wchar_t)(b + 32);
            if (a != b) return false;
        }
        return true;
    }

    static inline int _peb_strcmp(const char* a, const char* b) {
        while (*a && *b && *a == *b) { a++; b++; }
        return (unsigned char)*a - (unsigned char)*b;
    }

    // Retorna base address do modulo carregado, ou NULL se nao encontrado.
    static inline PVOID MyGetModuleHandle(const wchar_t* name) {
#ifdef _M_X64
        PPEB peb = (PPEB)__readgsqword(0x60);
#else
        PPEB peb = (PPEB)__readfsdword(0x30);
#endif
        if (!peb || !peb->Ldr) return NULL;

        PLIST_ENTRY head = &peb->Ldr->InMemoryOrderModuleList;
        for (PLIST_ENTRY curr = head->Flink; curr != head; curr = curr->Flink) {
            PFULL_LDR_DATA_TABLE_ENTRY entry = (PFULL_LDR_DATA_TABLE_ENTRY)
                ((PBYTE)curr - offsetof(FULL_LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks));

            if (_peb_wstr_ieq(&entry->BaseDllName, name)) {
                return entry->DllBase;
            }
        }
        return NULL;
    }

    // Parseia IMAGE_EXPORT_DIRECTORY manualmente e retorna endereco absoluto
    // do export. Retorna NULL se for forwarder (RVA cai dentro da export dir).
    static inline PVOID MyGetProcAddress(PVOID module, const char* name) {
        if (!module || !name) return NULL;
        PBYTE base = (PBYTE)module;
        PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
        PIMAGE_NT_HEADERS nt  = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
        IMAGE_DATA_DIRECTORY expDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!expDir.VirtualAddress || !expDir.Size) return NULL;

        PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)(base + expDir.VirtualAddress);
        PDWORD names = (PDWORD)(base + exports->AddressOfNames);
        PDWORD funcs = (PDWORD)(base + exports->AddressOfFunctions);
        PWORD  ords  = (PWORD )(base + exports->AddressOfNameOrdinals);

        for (DWORD i = 0; i < exports->NumberOfNames; i++) {
            const char* n = (const char*)(base + names[i]);
            if (_peb_strcmp(n, name) == 0) {
                DWORD funcRVA = funcs[ords[i]];
                if (funcRVA >= expDir.VirtualAddress &&
                    funcRVA <  expDir.VirtualAddress + expDir.Size) {
                    return NULL;
                }
                return base + funcRVA;
            }
        }
        return NULL;
    }
}
