// deps_check_test.cpp
// -----------------------------------------------------------------------------
// Harness de validacao para Dependencies.hpp.
//
// Uso:
//   deps_check_test.exe [diretorio_com_dlls_baixados]
//
// Testes:
//   1. Lista presenca das 6 DLLs em System32.
//   2. EnsureRuntimeDependencies(false) -> apenas check, sem instalar.
//   3. MissingInDirectory() em um diretorio vazio -> deve detectar os 6.
//   4. Se um diretorio for passado: valida PE x64 + SHA-256 pinado de cada DLL.
// -----------------------------------------------------------------------------

#include "../cheat-bypass-loader/Dependencies.hpp"

#include <cstdio>
#include <string>

static int g_failures = 0;

static void Check(bool condition, const wchar_t* label)
{
    wprintf(L"[%s] %s\n", condition ? L"PASS" : L"FAIL", label);
    if (!condition) ++g_failures;
}

int wmain(int argc, wchar_t** argv)
{
    const std::wstring sys = deps::System32Dir();

    wprintf(L"System32: %s\n", sys.c_str());
    wprintf(L"Elevated: %s\n\n", deps::IsElevated() ? L"yes" : L"no");

    std::wstring described = deps::DescribeSystemDependencies();
    wprintf(L"== Dependencias em System32 ==\n%s\n\n", described.c_str());
    deps::detail::SecureWipe(described);

    Check(deps::EnsureRuntimeDependencies(false),
          L"EnsureRuntimeDependencies(false) reports all present");

    // Diretorio vazio: a deteccao precisa acusar os 6 ausentes.
    const std::wstring emptyDir =
        deps::JoinPath(deps::TempDir(), L"cheat-bypass-deps-empty-test");
    CreateDirectoryW(emptyDir.c_str(), nullptr);
    std::wstring missing;
    const bool anyMissing = deps::MissingInDirectory(emptyDir, missing);
    wprintf(L"empty-dir detection: %s\n", missing.c_str());
    Check(anyMissing, L"MissingInDirectory() detects missing files in empty dir");
    Check(missing.find(L"msvcp140.dll") != std::wstring::npos &&
          missing.find(L"d3dx9_43.dll") != std::wstring::npos,
          L"empty-dir detection lists msvcp140.dll and d3dx9_43.dll");

    // End-to-end: download + SHA-256 + PE check + copy para um diretorio
    // temporario (nao toca em System32).
    {
        const std::wstring name = L"cheat-bypass-deps-install-" +
                                  std::to_wstring(GetTickCount64());
        const std::wstring installDir = deps::JoinPath(deps::TempDir(), name.c_str());
        CreateDirectoryW(installDir.c_str(), nullptr);

        std::wstring installLog;
        const bool installed = deps::EnsureDirectSourcesIn(installDir, &installLog);
        wprintf(L"\n== direct install test (%s) ==\n%s\n",
                installDir.c_str(), installLog.c_str());
        Check(installed, L"download + verify into temporary directory (no copy)");
        deps::detail::SecureWipe(installLog);

        std::wstring stillMissing;
        Check(!deps::MissingInDirectory(installDir, stillMissing),
              L"temporary directory now contains all six DLLs");

        // Segunda chamada: tudo presente -> deve ser no-op e continuar true.
        Check(deps::EnsureDirectSourcesIn(installDir, nullptr),
              L"second call is a no-op and still reports present");
    }

    // Validacao dos arquivos baixados (PE x64 + SHA-256 pinado).
    if (argc > 1)
    {
        const std::wstring dir = argv[1];
        deps::DirectSourceList sources = deps::DirectSourceTable();
        for (std::size_t i = 0; i < sources.items.size(); ++i)
        {
            const std::wstring path = deps::JoinPath(dir, sources.items[i].name.c_str());
            if (!deps::FileExists(path))
            {
                wprintf(L"[SKIP] %s (not in %s)\n", sources.items[i].name.c_str(), dir.c_str());
                continue;
            }

            Check(deps::PeMachineIs(path, IMAGE_FILE_MACHINE_AMD64),
                  (std::wstring(L"PE is x64: ") + sources.items[i].name).c_str());

            unsigned char digest[32]{};
            const bool hashed = deps::sha256::HashFile(path, digest);
            Check(hashed, (std::wstring(L"SHA-256 computed: ") + sources.items[i].name).c_str());
            if (hashed)
            {
                std::string hex = deps::sha256::ToHex(digest);
                const bool match = hex == sources.items[i].sha256;
                wprintf(L"       %s  %s\n",
                        match ? L"MATCH " : L"MISMATCH",
                        std::wstring(hex.begin(), hex.end()).c_str());
                deps::detail::SecureWipe(hex);
                Check(match, (std::wstring(L"SHA-256 pin: ") + sources.items[i].name).c_str());
            }
        }
    }

    wprintf(L"\n%s (%d failure%s)\n",
            g_failures == 0 ? L"RESULT: PASS" : L"RESULT: FAIL",
            g_failures, g_failures == 1 ? L"" : L"s");
    return g_failures == 0 ? 0 : 1;
}
