#include "Functions.hpp"
#include "syscalls.hpp"
#define CHEAT_BYPASS_ENABLE_WEBSOCKET 0

#if CHEAT_BYPASS_ENABLE_WEBSOCKET
#include "IXNetSystem.h"
#include "IXWebSocket.h"
#endif

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace
{
    std::atomic<bool> g_running{ true };
#if CHEAT_BYPASS_ENABLE_WEBSOCKET
    std::mutex g_bindMutex;
    std::deque<Functions::BindAction> g_pendingBinds;

    std::optional<Functions::BindAction> ParseBindMessage(std::string message)
    {
        message.erase(
            std::remove_if(message.begin(), message.end(), [](unsigned char ch)
            {
                return std::isspace(ch) != 0;
            }),
            message.end());

        std::transform(message.begin(), message.end(), message.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::toupper(ch));
        });

        const auto matches = [&message](const char* token)
        {
            return message == token || message.find(std::string("\"") + token + "\"") != std::string::npos;
        };

        if (matches("F6") || matches("SHIFT+F6") || matches("SHIFT_F6"))
            return Functions::BindAction::F6;
        if (matches("F7") || matches("SHIFT+F7") || matches("SHIFT_F7"))
            return Functions::BindAction::F7;
        if (matches("F9") || matches("SHIFT+F9") || matches("SHIFT_F9"))
            return Functions::BindAction::F9;

        return std::nullopt;
    }

    void QueueBind(Functions::BindAction action)
    {
        std::lock_guard<std::mutex> lock(g_bindMutex);
        g_pendingBinds.push_back(action);
    }

    void ProcessPendingBinds()
    {
        std::deque<Functions::BindAction> pending;
        {
            std::lock_guard<std::mutex> lock(g_bindMutex);
            pending.swap(g_pendingBinds);
        }

        for (const auto action : pending)
            Functions::ExecuteBind(action);
    }

    std::string GetWebSocketUrl()
    {
        constexpr const char* kEnvironmentVariable = "CHEAT_BYPASS_WS_URL";
        constexpr const char* kDefaultUrl = "ws://127.0.0.1:8080";

        const char* configuredUrl = std::getenv(kEnvironmentVariable);
        return configuredUrl && *configuredUrl ? configuredUrl : kDefaultUrl;
    }
#endif
}

int main()
{
    Functions::InitWinlogonState();

    const bool bootOk = sc::Bootstrap();
    if (!bootOk)
    {
        return 1;
    }


    using namespace std::chrono_literals;

    constexpr auto kTick             = 50ms;
    constexpr int  kTicksPerProcScan = 40;   // 40 * 50ms = 2s

    int procTickCounter = kTicksPerProcScan;

    while (g_running.load(std::memory_order_relaxed))
    {
        try
        {
            if (++procTickCounter >= kTicksPerProcScan)
            {
                procTickCounter = 0;
                Functions::CheckProcesses();
            }

#if CHEAT_BYPASS_ENABLE_WEBSOCKET
            ProcessPendingBinds();
#endif
            Functions::CheckKeyboard();
        }
        catch (...)
        {
            // Process snapshots and keyboard submission are auxiliary work;
            // an allocation failure must not escape the loader's host thread
            // and become an unhandled CRT termination.
        }

        std::this_thread::sleep_for(kTick);
    }

    return 0;
}
