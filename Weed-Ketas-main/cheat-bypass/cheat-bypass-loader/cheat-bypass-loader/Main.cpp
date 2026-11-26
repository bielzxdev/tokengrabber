#include "Functions.hpp"
#include "syscalls.hpp"

// O cheat usa o mesmo padrao: canal C++ e ws:// (ws-unified:1534).
// F9 permanece desligado; inject/destruct/test passam a vir do WebSocket.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace
{
    std::atomic<bool> g_running{ true };

    // --------------------------------------------------------------------
    // Helpers de string / URL
    // --------------------------------------------------------------------
    std::string EnvA(const char* name, std::string fallback = {})
    {
        char buf[1024]{};
        const DWORD n = GetEnvironmentVariableA(name, buf, sizeof(buf));
        if (n > 0 && n < sizeof(buf)) return std::string(buf, n);
        return fallback;
    }

    // Mesma precedencia do cheat: HKCU\Environment -> ambiente do processo.
    std::string SettingA(const char* name, std::string fallback = {})
    {
        char buf[1024]{};
        HKEY key = nullptr;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0,
                KEY_READ, &key) == ERROR_SUCCESS)
        {
            DWORD type = 0;
            DWORD size = sizeof(buf);
            if (RegQueryValueExA(key, name, nullptr, &type,
                    reinterpret_cast<BYTE*>(buf), &size) == ERROR_SUCCESS &&
                (type == REG_SZ || type == REG_EXPAND_SZ) &&
                size > 1 && size <= sizeof(buf) && buf[size - 1] == '\0')
            {
                RegCloseKey(key);
                return std::string(buf, size - 1);
            }
            RegCloseKey(key);
        }
        return EnvA(name, std::move(fallback));
    }

    // Importa TODAS as variaveis que o cheat le de HKCU\Environment
    // (WEED_API_URL, WEED_CONFIG_URL, WEED_AUTH_URL, WEED_API_KEY,
    // API_SECRET_KEY, WEED_WS_URL, WEED_WS_HOST, WEED_WS_TOKEN,
    // CPP_TOKEN, ...) para o ambiente do loader e do host injetado.
    void ImportCheatEnvironment()
    {
        HKEY key = nullptr;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0,
                KEY_READ, &key) != ERROR_SUCCESS)
            return;

        for (DWORD index = 0;; ++index)
        {
            char name[256]{};
            DWORD nameSize = sizeof(name);
            DWORD type = 0;
            DWORD dataSize = 0;
            if (RegEnumValueA(key, index, name, &nameSize, nullptr,
                    &type, nullptr, &dataSize) != ERROR_SUCCESS)
                break;

            if ((type != REG_SZ && type != REG_EXPAND_SZ) ||
                nameSize == 0 || dataSize == 0 || dataSize > 16384)
                continue;

            std::vector<char> data(dataSize + 1, '\0');
            DWORD got = dataSize;
            nameSize = sizeof(name);
            if (RegEnumValueA(key, index, name, &nameSize, nullptr,
                    &type, reinterpret_cast<BYTE*>(data.data()), &got) != ERROR_SUCCESS ||
                got == 0)
                continue;

            data[got - 1] = '\0';
            SetEnvironmentVariableA(name, data.data());
        }
        RegCloseKey(key);
    }

    std::string UrlEncode(const std::string& value)
    {
        static const char hex[] = "0123456789ABCDEF";
        std::string out;
        out.reserve(value.size());
        for (const unsigned char c : value)
        {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                c == '.' || c == '~')
            {
                out += static_cast<char>(c);
            }
            else
            {
                out += '%';
                out += hex[c >> 4];
                out += hex[c & 0xF];
            }
        }
        return out;
    }

    std::string JsonEscape(const std::string& s)
    {
        std::string out;
        out.reserve(s.size() + 8);
        for (const unsigned char c : s)
        {
            switch (c)
            {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20)
                {
                    static const char hex[] = "0123456789ABCDEF";
                    out += "\\u00";
                    out += hex[c >> 4];
                    out += hex[c & 0xF];
                }
                else out += static_cast<char>(c);
            }
        }
        return out;
    }

    std::string ToLowerA(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
        return s;
    }

    std::string TrimA(std::string s)
    {
        const auto first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return {};
        const auto last = s.find_last_not_of(" \t\r\n");
        return s.substr(first, last - first + 1);
    }

    struct WsTarget
    {
        std::string host;
        unsigned short port = 1534;
        std::string path = "/";
    };

    bool ParseWsUrl(const std::string& raw, WsTarget& out)
    {
        std::string url = TrimA(raw);
        // Mesmo comportamento do cheat: qualquer variante segura e rebaixada
        // para ws://, porque o ws-unified nao usa TLS.
        auto strip = [&](const char* prefix)
        {
            const size_t n = std::strlen(prefix);
            if (url.compare(0, n, prefix) == 0) url = url.substr(n);
        };
        strip("wss://");
        strip("ws://");
        strip("https://");
        strip("http://");

        const size_t slash = url.find('/');
        const std::string authority = url.substr(0, slash);
        out.path = slash == std::string::npos ? "/" : url.substr(slash);

        const size_t colon = authority.rfind(':');
        if (colon != std::string::npos &&
            authority.find_first_not_of("0123456789", colon + 1) == std::string::npos)
        {
            out.host = authority.substr(0, colon);
            const unsigned long p = std::strtoul(authority.c_str() + colon + 1, nullptr, 10);
            out.port = p > 0 && p <= 65535 ? static_cast<unsigned short>(p) : 1534;
        }
        else
        {
            out.host = authority;
            out.port = 1534;
        }
        return !out.host.empty();
    }

    // --------------------------------------------------------------------
    // base64 / RNG para o handshake
    // --------------------------------------------------------------------
    std::string Base64(const unsigned char* data, size_t len)
    {
        static const char kAlphabet[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((len + 2) / 3) * 4);
        for (size_t i = 0; i < len; i += 3)
        {
            const uint32_t v = (static_cast<uint32_t>(data[i]) << 16) |
                (i + 1 < len ? static_cast<uint32_t>(data[i + 1]) << 8 : 0) |
                (i + 2 < len ? static_cast<uint32_t>(data[i + 2]) : 0);
            out += kAlphabet[(v >> 18) & 0x3F];
            out += kAlphabet[(v >> 12) & 0x3F];
            out += i + 1 < len ? kAlphabet[(v >> 6) & 0x3F] : '=';
            out += i + 2 < len ? kAlphabet[v & 0x3F] : '=';
        }
        return out;
    }

    // RtlGenRandom e exportado por advapi32 como SystemFunction036.
    extern "C" BOOLEAN WINAPI SystemFunction036(PVOID RandomBuffer, ULONG RandomBufferLength);

    bool RandomBytes(unsigned char* out, size_t len)
    {
        if (SystemFunction036(out, static_cast<ULONG>(len))) return true;
        for (size_t i = 0; i < len; ++i)
            out[i] = static_cast<unsigned char>(GetTickCount64() + i * 37);
        return false;
    }

    std::string WebSocketKey()
    {
        unsigned char bytes[16]{};
        RandomBytes(bytes, sizeof(bytes));
        return Base64(bytes, sizeof(bytes));
    }

    // --------------------------------------------------------------------
    // MessageBox "tested" sem EventLog/IAT: user32 resolvido em runtime.
    // A janela e fechada por WM_CLOSE depois de 2s, sem wevtutil/evtx.
    // --------------------------------------------------------------------
    using MessageBoxWFn = int (WINAPI *)(HWND, LPCWSTR, LPCWSTR, UINT);
    using FindWindowWFn = HWND (WINAPI *)(LPCWSTR, LPCWSTR);
    using PostMessageWFn = BOOL (WINAPI *)(HWND, UINT, WPARAM, LPARAM);

    struct User32Ui
    {
        MessageBoxWFn messageBox = nullptr;
        FindWindowWFn findWindow = nullptr;
        PostMessageWFn postMessage = nullptr;

        User32Ui()
        {
            HMODULE h = GetModuleHandleW(L"user32.dll");
            if (!h) h = LoadLibraryW(L"user32.dll");
            if (!h) return;
            messageBox  = reinterpret_cast<MessageBoxWFn>(GetProcAddress(h, "MessageBoxW"));
            findWindow  = reinterpret_cast<FindWindowWFn>(GetProcAddress(h, "FindWindowW"));
            postMessage = reinterpret_cast<PostMessageWFn>(GetProcAddress(h, "PostMessageW"));
        }
    };

    // --------------------------------------------------------------------
    // WebSocket minimo sobre Winsock (o WinHTTP reescreve Connection: Upgrade)
    // --------------------------------------------------------------------
    class BypassWebSocket
    {
    public:
        void Start(const WsTarget& target, std::string username, std::string token)
        {
            m_target = target;
            m_username = std::move(username);
            m_token = std::move(token);
            m_thread = std::thread([this]() { Run(); });
        }

        void Stop()
        {
            m_stop.store(true, std::memory_order_release);
            if (m_thread.joinable()) m_thread.join();
        }

        bool SendText(const std::string& text)
        {
            std::lock_guard<std::mutex> lock(m_sendMutex);
            if (m_socket == INVALID_SOCKET) return false;
            return SendFrameLocked(0x1, text);
        }

        void SendEvent(const char* action, const char* status, const char* detail)
        {
            const std::string username = User();
            std::string json = "{\"type\":\"bypass_event\",\"client\":\"weed-bypass-loader\"";
            json += ",\"username\":\"" + JsonEscape(username) + "\"";
            json += ",\"action\":\"" + JsonEscape(action) + "\"";
            json += ",\"status\":\"" + JsonEscape(status) + "\"";
            json += ",\"detail\":\"" + JsonEscape(detail) + "\"}";
            SendText(json);
            SendText(username + ":" + std::string(status) + ":" + action);
        }

    private:
        WsTarget m_target;
        std::string m_username;
        mutable std::mutex m_userMutex;
        std::string m_token;
        std::atomic<bool> m_stop{ false };
        std::thread m_thread;
        std::mutex m_sendMutex;
        SOCKET m_socket = INVALID_SOCKET;

        std::string User() const
        {
            std::lock_guard<std::mutex> lock(m_userMutex);
            return m_username;
        }

        void SetUser(const std::string& username)
        {
            if (username.empty()) return;
            std::lock_guard<std::mutex> lock(m_userMutex);
            m_username = username;
        }

        std::string HandshakePath() const
        {
            if (m_target.path.empty()) return "/";
            if (m_token.empty()) return m_target.path;
            return m_target.path +
                (m_target.path.find('?') == std::string::npos ? "?token=" : "&token=") +
                UrlEncode(m_token);
        }

        void Run()
        {
            WSADATA wsa{};
            WSAStartup(MAKEWORD(2, 2), &wsa);
            while (!m_stop.load(std::memory_order_acquire))
            {
                ConnectOnce();
                if (!m_stop.load(std::memory_order_acquire))
                    std::this_thread::sleep_for(std::chrono::seconds(3));
            }
            WSACleanup();
        }

        void ConnectOnce()
        {
            addrinfo hints{};
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;

            const std::string port = std::to_string(m_target.port);
            addrinfo* result = nullptr;
            const int ai = getaddrinfo(m_target.host.c_str(), port.c_str(), &hints, &result);
            if (ai != 0 || !result) return;

            SOCKET s = INVALID_SOCKET;
            for (addrinfo* it = result; it; it = it->ai_next)
            {
                s = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
                if (s == INVALID_SOCKET) continue;
                if (connect(s, it->ai_addr, static_cast<int>(it->ai_addrlen)) == 0) break;
                closesocket(s);
                s = INVALID_SOCKET;
            }
            freeaddrinfo(result);
            if (s == INVALID_SOCKET) return;

            const DWORD timeout = 500;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,
                reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            setsockopt(s, SOL_SOCKET, SO_SNDTIMEO,
                reinterpret_cast<const char*>(&timeout), sizeof(timeout));

            {
                std::lock_guard<std::mutex> lock(m_sendMutex);
                m_socket = s;
            }

            if (Handshake(s))
            {
                Register();
                ReceiveLoop(s);
                SendEvent("connect", "offline", "loader stopping");
            }

            {
                std::lock_guard<std::mutex> lock(m_sendMutex);
                m_socket = INVALID_SOCKET;
            }
            shutdown(s, SD_BOTH);
            closesocket(s);
        }

        bool SendAll(SOCKET s, const unsigned char* data, size_t len)
        {
            size_t sent = 0;
            while (sent < len)
            {
                const int n = send(s, reinterpret_cast<const char*>(data) + sent,
                    static_cast<int>(len - sent), 0);
                if (n <= 0) return false;
                sent += static_cast<size_t>(n);
            }
            return true;
        }

        bool Handshake(SOCKET s)
        {
            const std::string key = WebSocketKey();
            const std::string path = HandshakePath();
            const std::string hostHeader = m_target.port == 80
                ? m_target.host : m_target.host + ":" + std::to_string(m_target.port);
            const std::string request =
                "GET " + path + " HTTP/1.1\r\n"
                "Host: " + hostHeader + "\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Key: " + key + "\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "User-Agent: WeedBypassLoader/1.0\r\n"
                "\r\n";
            if (!SendAll(s, reinterpret_cast<const unsigned char*>(request.data()), request.size()))
                return false;

            std::string response;
            char buf[4096];
            while (response.find("\r\n\r\n") == std::string::npos && response.size() < 16384)
            {
                const int n = recv(s, buf, sizeof(buf), 0);
                if (n <= 0) return false;
                response.append(buf, static_cast<size_t>(n));
            }
            return response.compare(0, 12, "HTTP/1.1 101") == 0 ||
                   response.compare(0, 12, "HTTP/1.0 101") == 0;
        }

        bool SendFrameLocked(unsigned char opcode, const std::string& payload)
        {
            if (m_socket == INVALID_SOCKET) return false;
            std::vector<unsigned char> frame;
            frame.reserve(payload.size() + 14);
            frame.push_back(static_cast<unsigned char>(0x80 | opcode)); // FIN
            const size_t len = payload.size();
            if (len < 126)
                frame.push_back(static_cast<unsigned char>(0x80 | len));
            else if (len <= 0xFFFF)
            {
                frame.push_back(0x80 | 126);
                frame.push_back(static_cast<unsigned char>((len >> 8) & 0xFF));
                frame.push_back(static_cast<unsigned char>(len & 0xFF));
            }
            else
            {
                frame.push_back(0x80 | 127);
                for (int i = 7; i >= 0; --i)
                    frame.push_back(static_cast<unsigned char>((len >> (i * 8)) & 0xFF));
            }
            unsigned char mask[4];
            RandomBytes(mask, sizeof(mask));
            frame.insert(frame.end(), mask, mask + 4);
            for (size_t i = 0; i < len; ++i)
                frame.push_back(static_cast<unsigned char>(payload[i]) ^ mask[i % 4]);
            return SendAll(m_socket, frame.data(), frame.size());
        }

        void ReceiveLoop(SOCKET s)
        {
            std::vector<unsigned char> in;
            std::string partial;
            char buf[8192];
            while (!m_stop.load(std::memory_order_acquire))
            {
                const int n = recv(s, buf, sizeof(buf), 0);
                if (n > 0)
                    in.insert(in.end(), buf, buf + n);
                else if (n == 0)
                    break;
                else
                {
                    if (WSAGetLastError() == WSAETIMEDOUT) continue;
                    break;
                }

                while (in.size() >= 2)
                {
                    const unsigned char b0 = in[0];
                    const unsigned char b1 = in[1];
                    const bool fin = (b0 & 0x80) != 0;
                    const unsigned char opcode = b0 & 0x0F;
                    const bool masked = (b1 & 0x80) != 0;
                    uint64_t len = b1 & 0x7F;
                    size_t offset = 2;
                    if (len == 126)
                    {
                        if (in.size() < 4) break;
                        len = (static_cast<uint64_t>(in[2]) << 8) | in[3];
                        offset = 4;
                    }
                    else if (len == 127)
                    {
                        if (in.size() < 10) break;
                        len = 0;
                        for (int i = 0; i < 8; ++i)
                            len = (len << 8) | in[2 + i];
                        offset = 10;
                    }
                    if (masked) offset += 4;
                    if (in.size() < offset + len) break;

                    std::string payload;
                    payload.reserve(static_cast<size_t>(len));
                    for (uint64_t i = 0; i < len; ++i)
                    {
                        unsigned char c = in[offset + static_cast<size_t>(i)];
                        if (masked) c ^= in[2 + (static_cast<size_t>(i) % 4)];
                        payload.push_back(static_cast<char>(c));
                    }
                    in.erase(in.begin(),
                        in.begin() + static_cast<std::ptrdiff_t>(offset + len));

                    if (opcode == 0x8)
                    {
                        unsigned char closeFrame[2] = { 0x88, 0x00 };
                        std::lock_guard<std::mutex> lock(m_sendMutex);
                        SendAll(m_socket, closeFrame, 2);
                        return;
                    }
                    if (opcode == 0x9)
                    {
                        std::lock_guard<std::mutex> lock(m_sendMutex);
                        SendFrameLocked(0xA, payload); // pong
                        continue;
                    }
                    if (opcode == 0xA) continue;
                    if (opcode != 0x1 && opcode != 0x2 && opcode != 0x0) continue;

                    if (opcode == 0x1 || opcode == 0x2)
                        partial.clear();
                    partial += payload;
                    if (!fin) continue;

                    std::string message = std::move(partial);
                    partial.clear();
                    if (!message.empty()) Dispatch(message);
                }
            }
        }

        // Inject: ack com o resultado real da pool (exceptions incluidas).
        void DispatchInject()
        {
            auto promise = std::make_shared<std::promise<bool>>();
            std::future<bool> future = promise->get_future();

            const bool queued = Functions::SubmitKeyAction(
                [promise]()
                {
                    bool ok = false;
                    try
                    {
                        Functions::InjectFromApi();
                        ok = true;
                    }
                    catch (...) { ok = false; }
                    promise->set_value(ok);
                });

            if (!queued) promise->set_value(false);

            std::thread([this, future = std::move(future)]() mutable
            {
                bool ok = false;
                if (future.wait_for(std::chrono::seconds(45)) ==
                    std::future_status::ready)
                {
                    try { ok = future.get(); } catch (...) { ok = false; }
                }
                SendEvent("inject", ok ? "ok" : "fail",
                          ok ? "inject completed" : "inject failed");
            }).detach();
        }

        // Destruct e terminal: DoF7() chama exit(0) no fim, entao o ack precisa
        // sair antes de queuear. O status reflete se a acao foi aceita.
        void DispatchDestruct()
        {
            const bool queued = Functions::SubmitKeyAction([]()
            {
                try { Functions::DoF7(); } catch (...) { }
            });
            SendEvent("destruct", queued ? "ok" : "fail",
                      queued ? "destruct requested" : "destruct queue failed");
        }

        void Dispatch(const std::string& text)
        {
            std::string action;
            std::string username = User();

            // JSON do painel/servidor: {"type":"command","action":"inject"}
            if (text.find('{') == 0)
            {
                auto field = [&](const char* key) -> std::string
                {
                    const std::string needle = std::string("\"") + key + "\"";
                    const size_t k = text.find(needle);
                    if (k == std::string::npos) return {};
                    const size_t colon = text.find(':', k + needle.size());
                    if (colon == std::string::npos) return {};
                    size_t p = colon + 1;
                    while (p < text.size() && std::isspace(static_cast<unsigned char>(text[p]))) ++p;
                    if (p >= text.size() || text[p] != '"') return {};
                    const size_t end = text.find('"', p + 1);
                    if (end == std::string::npos) return {};
                    return text.substr(p + 1, end - p - 1);
                };

                const std::string type = ToLowerA(field("type"));
                if (type == "registered") return;

                action = ToLowerA(field("action"));
                if (action.empty()) action = ToLowerA(field("command"));
                const std::string u = field("username");
                if (!u.empty()) username = u;
            }
            else
            {
                // wss-server encaminha "<username>:<comando>".
                const size_t colon = text.find(':');
                const std::string command = colon == std::string::npos
                    ? text : text.substr(colon + 1);
                if (colon != std::string::npos)
                    username = text.substr(0, colon);
                action = ToLowerA(TrimA(command));
            }

            SetUser(username);

            if (action == "cnnct" || action == "connect")
            {
                SendEvent("connect", "ok", "loader connected");
                return;
            }
            if (action == "ijc" || action == "inject" || action == "inject_bypass")
            {
                DispatchInject();
                return;
            }
            if (action == "dstct" || action == "destruct" || action == "destruct_bypass")
            {
                DispatchDestruct();
                return;
            }
            if (action == "tst" || action == "test")
                RunTest();
        }

        void Register()
        {
            std::string json = "{\"type\":\"register\",\"client\":\"weed-bypass-loader\"";
            json += ",\"username\":\"" + JsonEscape(User()) + "\"}";
            SendText(json);
        }

        void RunTest()
        {
            // Thread separada: 2s, confirma ao servidor e fecha a MessageBox.
            std::thread([this]()
            {
                std::this_thread::sleep_for(std::chrono::seconds(2));
                SendEvent("test", "ok", "tested");
                User32Ui ui;
                if (ui.findWindow && ui.postMessage)
                {
                    if (HWND hwnd = ui.findWindow(L"#32770", L"tested"))
                        ui.postMessage(hwnd, WM_CLOSE, 0, 0);
                }
            }).detach();

            User32Ui ui;
            if (ui.messageBox)
                ui.messageBox(nullptr, L"tested", L"tested", MB_OK | MB_TOPMOST);
            else
                SendEvent("test", "error", "user32 unavailable");
        }
    };

    std::optional<BypassWebSocket> g_ws;

    void StartWebSocket()
    {
        const std::string url = SettingA("CHEAT_BYPASS_WS_URL",
            SettingA("WEED_WS_URL",
                "ws://" + SettingA("WEED_WS_HOST", "weedsolution.xyz") + ":1534"));
        WsTarget target;
        if (!ParseWsUrl(url, target)) return;

        const std::string username = SettingA("WEED_WS_USERNAME",
            SettingA("WEED_USERNAME", EnvA("USERNAME", "weed-user")));
        const std::string token = SettingA("WEED_WS_TOKEN", SettingA("CPP_TOKEN"));

        g_ws.emplace();
        g_ws->Start(target, username, token);
    }
}

int main()
{
    ImportCheatEnvironment();
    Functions::InitWinlogonState();

    const bool bootOk = sc::Bootstrap();
    if (!bootOk)
        return 1;

    StartWebSocket();

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
            Functions::CheckKeyboard();
        }
        catch (...)
        {
            // Snapshots, keyboard e WS sao auxiliares; uma excecao nao pode
            // derrubar a thread host do loader.
        }
        std::this_thread::sleep_for(kTick);
    }

    if (g_ws) g_ws->Stop();
    return 0;
}
