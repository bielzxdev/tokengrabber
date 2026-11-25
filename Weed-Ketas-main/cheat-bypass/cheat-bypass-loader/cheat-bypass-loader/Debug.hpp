#pragma once
//
// Debug.hpp — instrumentacao temporaria do cheat-bypass-loader.
//
// Por padrao CHEAT_BYPASS_DEBUG = 1. Para desligar sem mexer no codigo:
//   cl /DCHEAT_BYPASS_DEBUG=0 ...
//
// Quando o loader roda dentro de RuntimeBroker (sem console), Init() cria
// um console com AllocConsole() e redireciona stdout/stderr para CONOUT$.
// As mensagens tambem vao para OutputDebugString (visivel no DebugView).
//

#ifndef CHEAT_BYPASS_DEBUG
#define CHEAT_BYPASS_DEBUG 1
#endif

#include <windows.h>

#include <cstdarg>
#include <cstdio>

namespace dbg
{
    inline bool g_initialized = false;

    // Espelho em arquivo: o console criado por AllocConsole() dentro do
    // RuntimeBroker nao e facil de observar de fora, e o OutputDebugString
    // exige um DebugView. O arquivo deixa evidencia direta de injecao.
    inline void LogToFile(const char* line, std::size_t len)
    {
        // Win32 direto: fopen_s/fwrite dependem do CRT, que pode nao estar
        // totalmente inicializado quando o payload entra via Donut.
        HANDLE f = CreateFileA(
            "C:\\Users\\SouzaStore\\AppData\\Local\\Temp\\opencode\\loader_debug.log",
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (f == INVALID_HANDLE_VALUE) return;
        SetFilePointer(f, 0, nullptr, FILE_END);
        DWORD written = 0;
        WriteFile(f, line, static_cast<DWORD>(len), &written, nullptr);
        CloseHandle(f);
    }

    inline void Printf(const char* fmt, ...)
    {
#if CHEAT_BYPASS_DEBUG
        char body[1024];
        va_list ap;
        va_start(ap, fmt);
        const int bodyLen = vsnprintf_s(body, sizeof(body), _TRUNCATE, fmt, ap);
        va_end(ap);
        if (bodyLen <= 0) return;

        char line[1152];
        const int lineLen = _snprintf_s(
            line, sizeof(line), _TRUNCATE,
            "[%08lu][pid=%lu] %s\r\n",
            GetTickCount(), GetCurrentProcessId(), body);
        if (lineLen <= 0) return;

        LogToFile(line, static_cast<std::size_t>(lineLen));
        fwrite(line, 1, static_cast<size_t>(lineLen), stdout);
        fflush(stdout);
        OutputDebugStringA(line);
#else
        (void)fmt;
#endif
    }

    inline void Init()
    {
#if CHEAT_BYPASS_DEBUG
        if (g_initialized) return;
        g_initialized = true;

        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == nullptr || hOut == INVALID_HANDLE_VALUE)
        {
            AllocConsole();
            SetConsoleCtrlHandler(nullptr, TRUE);

            FILE* stream = nullptr;
            freopen_s(&stream, "CONOUT$", "w", stdout);
            freopen_s(&stream, "CONOUT$", "w", stderr);
            freopen_s(&stream, "CONIN$", "r", stdin);
        }

        setvbuf(stdout, nullptr, _IONBF, 0);
        Printf("[dbg] loader vivo pid=%lu", GetCurrentProcessId());
#endif
    }
}

#define DBG(...) dbg::Printf(__VA_ARGS__)
