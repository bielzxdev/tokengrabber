#include "../include/Common.h"
#include "../include/PEBWalker.h"

namespace Hollowing {

    bool APIResolver::ResolveAll() {
        // kernelbase.dll contem as implementacoes "reais" (kernel32 sao forwarders
        // pra kernelbase em Windows moderno). Usando kernelbase evitamos forwarders.
        PVOID kb = MyGetModuleHandle(L"KERNELBASE.dll");
        if (!kb) kb = MyGetModuleHandle(L"kernel32.dll");
        if (!kb) return false;

        CreateProcessA        = (pCreateProcessA)       MyGetProcAddress(kb, "CreateProcessA");
        TerminateProcess      = (pTerminateProcess)     MyGetProcAddress(kb, "TerminateProcess");
        Wow64GetThreadContext = (pWow64GetThreadContext)MyGetProcAddress(kb, "Wow64GetThreadContext");
        Wow64SetThreadContext = (pWow64SetThreadContext)MyGetProcAddress(kb, "Wow64SetThreadContext");
        GetFileAttributesA    = (pGetFileAttributesA)   MyGetProcAddress(kb, "GetFileAttributesA");

        return CreateProcessA && TerminateProcess && GetFileAttributesA;
    }

}
