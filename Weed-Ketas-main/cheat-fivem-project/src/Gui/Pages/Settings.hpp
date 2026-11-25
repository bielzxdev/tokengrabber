#pragma once

#include <Includes/Includes.hpp>
#include <Security/Api/api.hpp>
#include <Core/Threads/ScreenResolution.hpp>

#include <Windows.h>
#include <TlHelp32.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cwchar>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace std;

namespace Settings {

    static int iSubTabCount = 0;
    static float SubTabAlpha = 0.f;
    static int iSubTab = 0;

    struct StringScanResult {
        bool success = false;
        std::size_t cleanedCount = 0;
    };

    enum class StringCleanerState : std::uint8_t {
        Idle,
        Running,
        Finished,
        Failed
    };

    inline std::atomic<StringCleanerState> g_StringCleanerState{ StringCleanerState::Idle };
    inline std::atomic_size_t g_StringCleanerCleanedCount{ 0 };
    inline std::atomic_bool g_StringCleanerUnloadRequested{ false };

    enum class OffsetReloadState : std::uint8_t {
        Idle,
        Running,
        Succeeded,
        Failed
    };

    inline std::atomic<OffsetReloadState> g_OffsetReloadState{ OffsetReloadState::Idle };

    inline bool StartOffsetReload() {
        OffsetReloadState expected = OffsetReloadState::Idle;
        if (!g_OffsetReloadState.compare_exchange_strong(
            expected,
            OffsetReloadState::Running
        )) {
            return false;
        }

        std::thread([]() noexcept {
            bool success = false;
            try {
                // Use the same native attachment/signature path as startup and
                // auto-detect. GetOffsets serializes reloads internally.
                success = Core::GetOffsets();
            }
            catch (...) {
                success = false;
            }

            g_OffsetReloadState.store(
                success ? OffsetReloadState::Succeeded : OffsetReloadState::Failed
            );
        }).detach();

        return true;
    }

    inline void ShowOffsetReloadResult() {
        auto state = g_OffsetReloadState.load();
        if (state != OffsetReloadState::Succeeded &&
            state != OffsetReloadState::Failed) {
            return;
        }

        if (!g_OffsetReloadState.compare_exchange_strong(
            state,
            OffsetReloadState::Idle
        )) {
            return;
        }

        NotifyManager::Send(
            state == OffsetReloadState::Succeeded
                ? xorstr("Signatures recarregadas com sucesso")
                : xorstr("Falha ao recarregar signatures"),
            3500
        );
    }

    // Config cloud: toda requisicao roda em worker para o menu nao travar.
    // O resultado so e aplicado no frame seguinte, na thread da interface.
    enum class CloudAction : std::uint8_t {
        None,
        List,
        Save,
        Load,
        Rename,
        Delete
    };

    struct CloudUiState {
        std::atomic_bool Busy{ false };
        std::mutex Mutex;
        std::vector<c_api::CloudConfig> Configs;
        std::string LoadedConfig;
        std::string Error;
        CloudAction FinishedAction = CloudAction::None;
    };

    inline CloudUiState g_CloudUi;

    inline void StartCloudJob(CloudAction Action, const std::string& Username,
        const std::string& Name, const std::string& NewName, const std::string& Encoded) {

        bool Expected = false;
        if (!g_CloudUi.Busy.compare_exchange_strong(Expected, true))
            return;

        {
            std::lock_guard<std::mutex> Lock(g_CloudUi.Mutex);
            g_CloudUi.FinishedAction = CloudAction::None;
            g_CloudUi.Error.clear();
            g_CloudUi.LoadedConfig.clear();
        }

        std::thread([Action, Username, Name, NewName, Encoded]() noexcept {
            std::vector<c_api::CloudConfig> Configs;
            std::string Loaded;
            std::string Error;

            try {
                switch (Action) {
                case CloudAction::List:
                    if (!g_Api.ListCloudConfigs(Username, Configs, Error))
                        Configs.clear();
                    break;

                case CloudAction::Save: {
                    if (!g_Api.ListCloudConfigs(Username, Configs, Error)) {
                        Configs.clear();
                        break;
                    }

                    bool Exists = false;
                    for (const auto& Entry : Configs) {
                        if (Entry.Name == Name) {
                            Exists = true;
                            break;
                        }
                    }

                    std::string ApiError;
                    const bool Saved = Exists
                        ? g_Api.UpdateCloudConfig(Username, Name, Encoded, ApiError)
                        : g_Api.CreateCloudConfig(Username, Name, Encoded, false, ApiError);
                    if (!Saved)
                        Error = ApiError;

                    Configs.clear();
                    g_Api.ListCloudConfigs(Username, Configs, ApiError);
                    break;
                }

                case CloudAction::Load: {
                    std::string ApiError;
                    if (!g_Api.ListCloudConfigs(Username, Configs, ApiError)) {
                        Error = ApiError;
                        break;
                    }

                    for (const auto& Entry : Configs) {
                        if (Entry.Name == Name) {
                            Loaded = Entry.Config;
                            break;
                        }
                    }
                    if (Loaded.empty())
                        Error = xorstr("Config nao encontrada na nuvem.");
                    break;
                }

                case CloudAction::Rename: {
                    std::string ApiError;
                    if (!g_Api.RenameCloudConfig(Username, Name, NewName, ApiError))
                        Error = ApiError;
                    Configs.clear();
                    g_Api.ListCloudConfigs(Username, Configs, ApiError);
                    break;
                }

                case CloudAction::Delete: {
                    std::string ApiError;
                    if (!g_Api.DeleteCloudConfig(Username, Name, ApiError))
                        Error = ApiError;
                    Configs.clear();
                    g_Api.ListCloudConfigs(Username, Configs, ApiError);
                    break;
                }

                default:
                    break;
                }
            }
            catch (...) {
                Error = xorstr("Erro inesperado na API de configs.");
            }

            {
                std::lock_guard<std::mutex> Lock(g_CloudUi.Mutex);
                g_CloudUi.Configs = std::move(Configs);
                g_CloudUi.LoadedConfig = std::move(Loaded);
                g_CloudUi.Error = std::move(Error);
                g_CloudUi.FinishedAction = Action;
            }
            g_CloudUi.Busy.store(false);
        }).detach();
    }

    inline void HandleCloudResult() {
        if (g_CloudUi.Busy.load())
            return;

        CloudAction Action = CloudAction::None;
        std::string Error;
        std::string Loaded;

        {
            std::lock_guard<std::mutex> Lock(g_CloudUi.Mutex);
            Action = g_CloudUi.FinishedAction;
            if (Action == CloudAction::None)
                return;
            g_CloudUi.FinishedAction = CloudAction::None;
            Error = g_CloudUi.Error;
            g_CloudUi.Error.clear();
            Loaded = g_CloudUi.LoadedConfig;
            g_CloudUi.LoadedConfig.clear();
        }

        if (!Error.empty()) {
            NotifyManager::Send(Error, 4000);
            return;
        }

        switch (Action) {
        case CloudAction::Load: {
            if (Loaded.empty()) {
                NotifyManager::Send(xorstr("Config vazia na nuvem."), 3000);
                break;
            }
            const std::string Result = g_Config.LoadCfg("cloud", Loaded);
            NotifyManager::Send(
                Result.empty() ? std::string(xorstr("Config cloud carregada.")) : Result, 3000);
            break;
        }
        case CloudAction::Save:
            NotifyManager::Send(xorstr("Config salva na nuvem."), 3000);
            break;
        case CloudAction::Rename:
            NotifyManager::Send(xorstr("Config renomeada na nuvem."), 3000);
            break;
        case CloudAction::Delete:
            NotifyManager::Send(xorstr("Config deletada da nuvem."), 3000);
            break;
        default:
            break;
        }
    }

    inline void RenderMonitorCard() {
        Custom::BeginCard(
            Utils::Translate(xorstr("Overlay Monitor")),
            1,
            0.f,
            0.f,
            true,
            ICON_FA_DISPLAY
        );
        {
            static int SelectedMonitor = -1;
            const std::vector<std::string> MonitorNames =
                Core::Threads::cScreenResolution::ListMonitors();
            const int PrimaryMonitor =
                Core::Threads::cScreenResolution::GetPrimaryMonitorIndex();

            if (SelectedMonitor < 0 ||
                SelectedMonitor >= static_cast<int>(MonitorNames.size())) {
                SelectedMonitor = Core::Threads::cScreenResolution::g_SelectedMonitor;
                if (SelectedMonitor < 0 ||
                    SelectedMonitor >= static_cast<int>(MonitorNames.size()))
                    SelectedMonitor = PrimaryMonitor;
            }

            Custom::SectionLabel(Utils::Translate(xorstr("Overlay placement")));
            Custom::CheckBox(
                Utils::Translate(xorstr("Use selected monitor")),
                &g_Config.General->SecondMonitor,
                xorstr("Move o overlay para o monitor escolhido.")
            );

            if (MonitorNames.empty()) {
                ImGui::TextColored(
                    ImColor(g_Col.SecundaryText),
                    "%s",
                    Utils::Translate(xorstr("No monitor found"))
                );
            }
            else {
                ImGui::PushStyleVar(
                    ImGuiStyleVar_ItemSpacing,
                    ImVec2(0.f, 3.f)
                );
                for (size_t Index = 0; Index < MonitorNames.size(); ++Index) {
                    ImGui::PushID(static_cast<int>(Index));
                    bool IsSelected =
                        SelectedMonitor == static_cast<int>(Index);
                    if (ImGui::ListSelectable(
                        MonitorNames[Index].c_str(),
                        &IsSelected
                    )) {
                        SelectedMonitor = static_cast<int>(Index);
                    }
                    ImGui::PopID();
                }
                ImGui::PopStyleVar();
            }

            ImGui::Dummy(ImVec2(0.f, 4.f));
            if (Custom::Button(
                Utils::Translate(xorstr("Set Monitor")),
                ImVec2(-1.f, g_MenuInfo.ButtonHeight),
                0
            )) {
                if (MonitorNames.empty()) {
                    NotifyManager::Send(
                        Utils::Translate(xorstr("No monitor found")),
                        3000
                    );
                }
                else {
                    Core::Threads::cScreenResolution::g_SelectedMonitor =
                        SelectedMonitor;
                    g_Config.General->SecondMonitor =
                        SelectedMonitor != PrimaryMonitor;
                    Core::Threads::g_ScreenResolution.MoveAndSetResolution();
                    NotifyManager::Send(
                        Utils::Translate(xorstr("Monitor applied")),
                        3000
                    );
                }
            }

            if (!MonitorNames.empty() &&
                SelectedMonitor >= 0 &&
                SelectedMonitor < static_cast<int>(MonitorNames.size())) {
                ImGui::TextColored(
                    ImColor(g_Col.SecundaryText),
                    "%s: %s",
                    Utils::Translate(xorstr("Selected")),
                    MonitorNames[SelectedMonitor].c_str()
                );
            }
        }
        Custom::EndCard();
    }

    inline bool IsReadableMemory(DWORD protection) {
        if (protection & (PAGE_GUARD | PAGE_NOACCESS))
            return false;

        switch (protection & 0xFF) {
        case PAGE_READONLY:
        case PAGE_READWRITE:
        case PAGE_WRITECOPY:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            return true;
        default:
            return false;
        }
    }

    // Strings pertencem a paginas de dados. Nunca altere paginas executaveis:
    // uma ocorrencia coincidente dentro de codigo/immediate pode corromper o
    // fluxo do FiveM e deixa-lo congelado.
    inline bool IsStringDataMemory(DWORD protection) {
        if (protection & (PAGE_GUARD | PAGE_NOACCESS))
            return false;

        switch (protection & 0xFF) {
        case PAGE_READONLY:
        case PAGE_READWRITE:
        case PAGE_WRITECOPY:
            return true;
        default:
            return false;
        }
    }

    inline bool ZeroTargetBytes(
        HANDLE hProcess,
        uintptr_t address,
        const void* zeros,
        SIZE_T length,
        const MEMORY_BASIC_INFORMATION* knownRegion = nullptr
    ) {
        if (!hProcess || !address || !zeros || !length)
            return false;

        MEMORY_BASIC_INFORMATION mbi{};
        if (knownRegion) {
            mbi = *knownRegion;
        }
        else {
            if (VirtualQueryEx(
                hProcess,
                reinterpret_cast<LPCVOID>(address),
                &mbi,
                sizeof(mbi)
            ) != sizeof(mbi)) {
                return false;
            }
        }

        if (mbi.State != MEM_COMMIT || !IsStringDataMemory(mbi.Protect))
            return false;

        const uintptr_t regionBase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        if (mbi.RegionSize > UINTPTR_MAX - regionBase)
            return false;

        const uintptr_t regionEnd = regionBase + mbi.RegionSize;
        if (address < regionBase || address > regionEnd ||
            length > regionEnd - address) {
            // Nunca escreva atravessando duas regioes com protecoes diferentes.
            return false;
        }

        DWORD oldProtection = 0;
        const DWORD writableProtection = PAGE_READWRITE;

        // FiveM keeps literals em paginas PAGE_READONLY/PAGE_WRITECOPY.
        // Torne a pagina gravavel apenas durante a substituicao e restaure-a.
        if (!VirtualProtectEx(
            hProcess,
            reinterpret_cast<LPVOID>(address),
            length,
            writableProtection,
            &oldProtection
        )) {
            return false;
        }

        SIZE_T bytesWritten = 0;
        const BOOL written = WriteProcessMemory(
            hProcess,
            reinterpret_cast<LPVOID>(address),
            zeros,
            length,
            &bytesWritten
        );

        DWORD ignoredProtection = 0;
        const BOOL restored = VirtualProtectEx(
            hProcess,
            reinterpret_cast<LPVOID>(address),
            length,
            oldProtection,
            &ignoredProtection
        );

        if (written && bytesWritten == length)
            FlushInstructionCache(hProcess, reinterpret_cast<LPCVOID>(address), length);

        return written && bytesWritten == length && restored;
    }

    // Procura e apaga todas as ocorrencias ANSI da string no processo indicado.
    // O handle pode ser fornecido pelo chamador para evitar uma segunda abertura
    // do FiveM, que pode falhar enquanto o processo esta protegido/encerrando.
    inline StringScanResult ScanTargetString(DWORD pid, HANDLE suppliedHandle = nullptr) {
        StringScanResult result{};

        if (!pid)
            return result;

        HANDLE hProcess = suppliedHandle;
        bool ownsHandle = false;
        if (!hProcess) {
            hProcess = OpenProcess(
                PROCESS_QUERY_INFORMATION | PROCESS_VM_READ |
                PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
                FALSE,
                pid
            );
            ownsHandle = hProcess != nullptr;
        }
        if (!hProcess)
            return result;

        // Keep the complete data-page scan and match every ASCII
        // capitalization of the process name. One pattern per encoding is
        // enough; this is cheaper and more complete than enumerating cases.
        constexpr char ansiTarget[] = "runtimebroker.exe";
        constexpr wchar_t wideTarget[] = L"runtimebroker.exe";
        const auto FoldAscii = [](unsigned char Byte) -> unsigned char {
            if (Byte >= static_cast<unsigned char>('A') &&
                Byte <= static_cast<unsigned char>('Z')) {
                return static_cast<unsigned char>(
                    Byte + static_cast<unsigned char>('a' - 'A')
                );
            }
            return Byte;
        };

        struct CompiledPattern {
            std::vector<unsigned char> Bytes;
            std::array<SIZE_T, 256> Skip{};
        };

        // Larger batches reduce ReadProcessMemory and VirtualQueryEx overhead
        // without changing which committed, non-executable regions are read.
        constexpr SIZE_T chunkSize = 8 * 1024 * 1024;

        SYSTEM_INFO si{};
        GetSystemInfo(&si);

        uintptr_t address = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
        const uintptr_t maxAddress = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);

        std::vector<CompiledPattern> patterns;
        patterns.reserve(2);
        SIZE_T maxPatternLen = 0;

        const auto AddPattern = [&](const void* Data, SIZE_T Length) {
            if (!Data || Length == 0)
                return;

            patterns.emplace_back();
            CompiledPattern& Pattern = patterns.back();
            const auto* First = static_cast<const unsigned char*>(Data);
            Pattern.Bytes.assign(First, First + Length);

            for (SIZE_T& Distance : Pattern.Skip)
                Distance = Length;
            for (SIZE_T Index = 0; Index + 1 < Length; ++Index) {
                Pattern.Skip[FoldAscii(Pattern.Bytes[Index])] =
                    Length - 1 - Index;
            }

            maxPatternLen = (std::max)(maxPatternLen, Length);
        };

        AddPattern(ansiTarget, sizeof(ansiTarget) - 1);
        AddPattern(wideTarget, (sizeof(wideTarget) - sizeof(wchar_t)));

        if (patterns.empty() || maxPatternLen == 0)
            return result;

        MEMORY_BASIC_INFORMATION mbi{};
        std::vector<unsigned char> buffer(chunkSize + maxPatternLen - 1);
        const std::vector<unsigned char> zeros(maxPatternLen, 0);

        result.success = true;

        while (address < maxAddress) {
            if (VirtualQueryEx(
                hProcess,
                reinterpret_cast<LPCVOID>(address),
                &mbi,
                sizeof(mbi)
            ) != sizeof(mbi)) {
                // A consulta pode falhar para determinados enderecos; continua na proxima pagina.
                const uintptr_t next = address + static_cast<uintptr_t>(si.dwPageSize);
                if (next <= address)
                    break;
                address = next;
                continue;
            }

            const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            if (mbi.RegionSize > UINTPTR_MAX - base)
                break;

            const uintptr_t end = base + mbi.RegionSize;
            if (end <= address)
                break;

            // Nao leia/escreva paginas executaveis. Alem de evitar travamentos,
            // isso elimina falsos positivos em bytes de instrucoes do jogo.
            if (mbi.State == MEM_COMMIT && IsStringDataMemory(mbi.Protect)) {
                for (uintptr_t pos = base; pos < end;) {
                    const SIZE_T remaining = static_cast<SIZE_T>(end - pos);
                    const SIZE_T primarySize = (std::min)(chunkSize, remaining);
                    const SIZE_T size = (std::min)(
                        chunkSize + maxPatternLen - 1,
                        remaining
                    );
                    SIZE_T bytesRead = 0;

                    const BOOL readOk = ReadProcessMemory(
                        hProcess,
                        reinterpret_cast<LPCVOID>(pos),
                        buffer.data(),
                        size,
                        &bytesRead
                    );

                    // O FiveM pode alterar/desmapear uma pagina durante o scan.
                    // Aproveita leituras parciais em vez de descartar o chunk.
                    if ((readOk || bytesRead > 0) && bytesRead > 0) {
                        // Boyer-Moore-Horspool avoids repeatedly comparing
                        // every byte of the target across large chunks.
                        const auto scanPattern = [&](const CompiledPattern& Pattern) {
                            const auto& pattern = Pattern.Bytes;
                            const SIZE_T patternLen = pattern.size();
                            if (patternLen == 0 || bytesRead < patternLen)
                                return;

                            const SIZE_T maxStart = (std::min)(
                                primarySize,
                                bytesRead - patternLen + 1
                            );

                            SIZE_T Offset = 0;
                            while (Offset < maxStart) {
                                SIZE_T PatternIndex = patternLen;
                                while (PatternIndex > 0 &&
                                    FoldAscii(pattern[PatternIndex - 1]) ==
                                    FoldAscii(buffer[Offset + PatternIndex - 1])) {
                                    --PatternIndex;
                                }

                                if (PatternIndex == 0) {
                                    const uintptr_t found = pos + Offset;
                                    if (ZeroTargetBytes(
                                        hProcess,
                                        found,
                                        zeros.data(),
                                        patternLen,
                                        &mbi
                                    )) {
                                        ++result.cleanedCount;
                                    }

                                    // Skip the bytes just handled; the target
                                    // does not overlap itself usefully.
                                    Offset += patternLen;
                                }
                                else {
                                    Offset += Pattern.Skip[
                                        FoldAscii(buffer[Offset + patternLen - 1])
                                    ];
                                }
                            }
                        };

                        for (const auto& Pattern : patterns)
                            scanPattern(Pattern);
                    }

                    if (remaining <= primarySize)
                        break;

                    // Sobreposicao permite encontrar strings divididas entre blocos.
                    pos += primarySize;
                }
            }

            address = end;
        }

        if (ownsHandle)
            CloseHandle(hProcess);

        return result;
    }

    inline bool StartStringCleaner() {
        StringCleanerState expected = StringCleanerState::Idle;

        if (!g_StringCleanerState.compare_exchange_strong(
            expected,
            StringCleanerState::Running
        )) {
            return false;
        }

        DWORD pid = 0;
        if (g_Variables.g_hGameWindow)
            GetWindowThreadProcessId(g_Variables.g_hGameWindow, &pid);
        if (!pid)
            pid = g_Variables.ProcIdFiveM;

        HANDLE duplicatedHandle = nullptr;

        // Mem.ProcHandle ja foi aberto com os direitos necessarios durante o
        // attach. Duplicar esse handle evita uma nova tentativa de OpenProcess
        // contra o FiveM enquanto ele esta executando.
        if (pid && Mem.ProcHandle && Mem.ProcId == pid &&
            GetProcessId(Mem.ProcHandle) == pid) {
            DuplicateHandle(
                GetCurrentProcess(),
                Mem.ProcHandle,
                GetCurrentProcess(),
                &duplicatedHandle,
                0,
                FALSE,
                DUPLICATE_SAME_ACCESS
            );
        }

        // O handle e obtido antes de criar a thread. Assim o worker nao fica
        // dependendo de uma nova abertura enquanto o FiveM esta protegido ou
        // enquanto o autodetect troca a sessao anexada.
        if (!duplicatedHandle && pid) {
            duplicatedHandle = OpenProcess(
                PROCESS_QUERY_INFORMATION | PROCESS_VM_READ |
                PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
                FALSE,
                pid
            );
        }

        if (!pid || !duplicatedHandle) {
            if (duplicatedHandle)
                CloseHandle(duplicatedHandle);
            g_StringCleanerUnloadRequested.store(false);
            g_StringCleanerState.store(StringCleanerState::Failed);
            return true;
        }

        g_StringCleanerCleanedCount.store(0);
        g_StringCleanerUnloadRequested.store(false);

        try {
            std::thread([pid, duplicatedHandle]() noexcept {
                HANDLE workerHandle = duplicatedHandle;
                try {
                    const auto result = ScanTargetString(pid, workerHandle);

                    if (workerHandle)
                        CloseHandle(workerHandle);

                    if (!result.success) {
                        g_StringCleanerUnloadRequested.store(false);
                        g_StringCleanerState.store(StringCleanerState::Failed);
                        return;
                    }

                    g_StringCleanerCleanedCount.store(result.cleanedCount);
                    g_StringCleanerUnloadRequested.store(true);
                    g_StringCleanerState.store(StringCleanerState::Finished);
                }
                catch (...) {
                    if (workerHandle)
                        CloseHandle(workerHandle);
                    g_StringCleanerUnloadRequested.store(false);
                    g_StringCleanerState.store(StringCleanerState::Failed);
                }
                }).detach();
        }
        catch (...) {
            if (duplicatedHandle)
                CloseHandle(duplicatedHandle);
            g_StringCleanerUnloadRequested.store(false);
            g_StringCleanerState.store(StringCleanerState::Failed);
            return false;
        }

        return true;
    }

    inline void ShowStringCleanerResult() {
        auto state = g_StringCleanerState.load();
        if (state != StringCleanerState::Finished &&
            state != StringCleanerState::Failed) {
            return;
        }

        if (!g_StringCleanerState.compare_exchange_strong(
            state,
            StringCleanerState::Idle
        )) {
            return;
        }

        const bool unloadRequested =
            g_StringCleanerUnloadRequested.exchange(false);

        if (state == StringCleanerState::Failed) {
            // Erro no String Cleaner: encerrar sem deixar o cheat rodando.
            ExitProcess(0);
        }
        else {
            const std::string message =
                "String Cleaner concluido com sucesso: " +
                std::to_string(g_StringCleanerCleanedCount.load()) +
                " detects limpas";

            NotifyManager::Send(message, 4000);
        }

        // O worker termina primeiro; somente agora o processo pode sair. Isso
        // elimina a corrida que matava a thread antes de limpar o FiveM.
        if (state == StringCleanerState::Finished && unloadRequested)
            ExitProcess(0);
    }

    inline void Render() {
        ShowStringCleanerResult();
        ShowOffsetReloadResult();
        HandleCloudResult();

        const float Eased = Custom::PageTransition(SubTabAlpha, iSubTab, iSubTabCount);

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, Eased * ImGui::GetStyle().Alpha);

        float ContentSlide = (1.f - Eased) * g_MenuInfo.TransitionSlide;
        Custom::BeginCards(ContentSlide + g_MenuInfo.TabAdd);
        {
            switch (iSubTab) {
            case 0: // Settings
                Custom::BeginCard(Utils::Translate(xorstr("Menu Preferences")));
                {
                    Custom::CheckBox(
                        Utils::Translate(xorstr("VSync")),
                        &g_Config.General->VSync,
                        xorstr("Sincroniza o menu com a taxa de atualização do monitor.")
                    );

                    if (Custom::CheckBox(
                        Utils::Translate(xorstr("Stream Mode")),
                        &g_Config.General->StreamProof,
                        xorstr("Esconde o menu de programas de gravação e transmissão."),
                        true,
                        Utils::Translate(xorstr("Hides this window from screen/stream capture"))
                    )) {
                        bool Enabling = g_Config.General->StreamProof;

                        std::thread([Enabling]() {
                            bool Success = Utils::ApplyStreamMode(g_Variables.g_hCheatWindow, Enabling);

                            if (!Success && Enabling)
                                g_Config.General->ShadowPlayBypass = false;

                            NotifyManager::Send(
                                Success
                                ? (Enabling
                                    ? Utils::Translate(xorstr("Stream Mode enabled"))
                                    : Utils::Translate(xorstr("Stream Mode disabled")))
                                : (Enabling
                                    ? Utils::Translate(xorstr("Failed to apply Stream Mode"))
                                    : Utils::Translate(xorstr("Failed to revert Stream Mode"))),
                                3000
                            );
                            }).detach();
                    }

                    static int KeyMode = 1;
                    ImGui::Keybind(
                        Utils::Translate(xorstr("Menu Key")),
                        &g_Config.General->MenuKey,
                        &KeyMode
                    );

                    ImGui::Dummy(ImVec2(0.f, 3.f));

                    const auto reloadState = g_OffsetReloadState.load();
                    if (Custom::Button(
                        reloadState == OffsetReloadState::Running
                            ? xorstr("Reloading signatures...")
                            : xorstr("Reload signatures"),
                        ImVec2(-1, g_MenuInfo.ButtonHeight),
                        0
                    )) {
                        if (!StartOffsetReload())
                            NotifyManager::Send(xorstr("Reload de signatures ja esta em execucao"), 3000);
                    }

                    const auto cleanerState = g_StringCleanerState.load();
                    if (Custom::Button(
                        cleanerState == StringCleanerState::Running
                            ? xorstr("Cleaning strings...")
                            : Utils::Translate(xorstr("Unload")),
                        ImVec2(-1, g_MenuInfo.ButtonHeight),
                        0
                    )) {
                        if (cleanerState != StringCleanerState::Running &&
                            !StartStringCleaner())
                            NotifyManager::Send(xorstr("String Cleaner ja esta em execucao"), 3000);
                    }
                	}
                	Custom::EndCard();
                	RenderMonitorCard();
                	break;

            case 1: // Configuration
            {
                static char ConfigName[64] = "";
                static char NewName[64] = "";

                const std::string Username = g_Variables.UserName;
                const bool Busy = g_CloudUi.Busy.load();

                std::vector<c_api::CloudConfig> Snapshot;
                {
                    std::lock_guard<std::mutex> Lock(g_CloudUi.Mutex);
                    Snapshot = g_CloudUi.Configs;
                }

                // Left side: the user's configs, using the same selectable
                // list rhythm as the Teleports page.
                Custom::BeginCard(
                    Utils::Translate(xorstr("My Configs")),
                    1,
                    0.f,
                    0.f,
                    true,
                    ICON_FA_FOLDER_OPEN
                );
                {
                    Custom::SectionLabel(Utils::Translate(xorstr("Your cloud configs")));
                    if (Username.empty()) {
                        ImGui::TextColored(
                            ImColor(g_Col.SecundaryText),
                            "%s",
                            Utils::Translate(xorstr("Login to manage your configs."))
                        );
                    }
                    else {
                        ImGui::TextColored(
                            ImColor(g_Col.SecundaryText),
                            "%s: %s",
                            Utils::Translate(xorstr("User")),
                            Username.c_str()
                        );
                    }

                    ImGui::Dummy(ImVec2(0.f, 5.f));

                    if (Snapshot.empty()) {
                        ImGui::TextColored(
                            ImColor(g_Col.SecundaryText),
                            "%s",
                            Utils::Translate(xorstr("No cloud configs found."))
                        );
                    }
                    else {
                        ImGui::PushStyleVar(
                            ImGuiStyleVar_ItemSpacing,
                            ImVec2(8.f, g_MenuInfo.RowSpacing)
                        );
                        for (size_t Index = 0; Index < Snapshot.size(); ++Index) {
                            const auto& Entry = Snapshot[Index];
                            const std::string Label = Entry.Name
                                + (Entry.Visible
                                    ? std::string(xorstr("  (public)"))
                                    : std::string());
                            bool IsSelected = Entry.Name == ConfigName;

                            ImGui::PushID(static_cast<int>(Index));
                            if (ImGui::ListSelectable(Label.c_str(), &IsSelected)) {
                                std::strncpy(
                                    ConfigName,
                                    Entry.Name.c_str(),
                                    sizeof(ConfigName) - 1
                                );
                                ConfigName[sizeof(ConfigName) - 1] = '\0';
                            }
                            ImGui::PopID();
                        }
                        ImGui::PopStyleVar();
                    }
                }
                Custom::EndCard();

                // Right side: operations for the selected config.
                Custom::BeginCard(
                    Utils::Translate(xorstr("Actions")),
                    1,
                    0.f,
                    0.f,
                    true,
                    ICON_FA_WRENCH
                );
                {
                    const ImGuiStyle& Style = ImGui::GetStyle();
                    const float InputPaddingY = ImMax(
                        4.f,
                        (g_MenuInfo.ButtonHeight - ImGui::GetFontSize()) * 0.5f
                    );

                    Custom::SectionLabel(Utils::Translate(xorstr("Selected config")));
                    ImGui::PushStyleVar(
                        ImGuiStyleVar_FramePadding,
                        ImVec2(Style.FramePadding.x, InputPaddingY)
                    );
                    ImGui::SetNextItemWidth(-1.f);
                    ImGui::InputTextWithHint(
                        xorstr("##cloud_name"),
                        Utils::Translate(xorstr("Config name")),
                        ConfigName,
                        sizeof(ConfigName)
                    );

                    ImGui::Dummy(ImVec2(0.f, 5.f));
                    Custom::SectionLabel(Utils::Translate(xorstr("Rename config")));
                    ImGui::SetNextItemWidth(-1.f);
                    ImGui::InputTextWithHint(
                        xorstr("##cloud_newname"),
                        Utils::Translate(xorstr("New name")),
                        NewName,
                        sizeof(NewName)
                    );
                    ImGui::PopStyleVar();

                    ImGui::Dummy(ImVec2(0.f, 7.f));
                    Custom::SectionLabel(Utils::Translate(xorstr("Cloud actions")));

                    if (Busy) {
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                    }

                    if (Custom::Button(
                        Utils::Translate(xorstr("Save Cloud")),
                        ImVec2(-1.f, g_MenuInfo.ButtonHeight),
                        0
                    )) {
                        if (Username.empty()) {
                            NotifyManager::Send(
                                xorstr("Faca login para usar a config cloud."),
                                3000
                            );
                        }
                        else {
                            const std::string Encoded = g_Config.SaveCurrentConfig();
                            if (Encoded.empty()) {
                                NotifyManager::Send(
                                    xorstr("Falha ao gerar a config."),
                                    3000
                                );
                            }
                            else {
                                StartCloudJob(
                                    CloudAction::Save,
                                    Username,
                                    ConfigName,
                                    "",
                                    Encoded
                                );
                            }
                        }
                    }

                    if (Custom::Button(
                        Utils::Translate(xorstr("Load Cloud")),
                        ImVec2(-1.f, g_MenuInfo.ButtonHeight),
                        0
                    )) {
                        if (Username.empty())
                            NotifyManager::Send(
                                xorstr("Faca login para usar a config cloud."),
                                3000
                            );
                        else
                            StartCloudJob(
                                CloudAction::Load,
                                Username,
                                ConfigName,
                                "",
                                ""
                            );
                    }

                    if (Custom::Button(
                        Utils::Translate(xorstr("Rename Cloud")),
                        ImVec2(-1.f, g_MenuInfo.ButtonHeight),
                        0
                    )) {
                        if (Username.empty())
                            NotifyManager::Send(
                                xorstr("Faca login para usar a config cloud."),
                                3000
                            );
                        else
                            StartCloudJob(
                                CloudAction::Rename,
                                Username,
                                ConfigName,
                                NewName,
                                ""
                            );
                    }

                    if (Custom::Button(
                        Utils::Translate(xorstr("Delete Cloud")),
                        ImVec2(-1.f, g_MenuInfo.ButtonHeight),
                        0
                    )) {
                        if (Username.empty())
                            NotifyManager::Send(
                                xorstr("Faca login para usar a config cloud."),
                                3000
                            );
                        else
                            StartCloudJob(
                                CloudAction::Delete,
                                Username,
                                ConfigName,
                                "",
                                ""
                            );
                    }

                    if (Custom::Button(
                        Utils::Translate(xorstr("Refresh")),
                        ImVec2(-1.f, g_MenuInfo.ButtonHeight),
                        0
                    )) {
                        if (Username.empty())
                            NotifyManager::Send(
                                xorstr("Faca login para usar a config cloud."),
                                3000
                            );
                        else
                            StartCloudJob(
                                CloudAction::List,
                                Username,
                                "",
                                "",
                                ""
                            );
                    }

                    if (Busy) {
                        ImGui::PopItemFlag();
                        ImGui::PopStyleVar();
                        ImGui::TextColored(
                            ImColor(g_Col.SecundaryText),
                            "%s",
                            Utils::Translate(xorstr("Working..."))
                        );
                    }
                }
                Custom::EndCard();
                break;
            }

            case 2: // Colors
                Custom::BeginCard(Utils::Translate(xorstr("Colors")), 2);
                {
                    Custom::SectionLabel(Utils::Translate(xorstr("Interface")));
                    Custom::ColorRow(Utils::Translate(xorstr("Accent Color")), g_Col.Base);
                    Custom::ColorRow(Utils::Translate(xorstr("Accent Light")), g_Col.BaseLight);
                    Custom::ColorRow(Utils::Translate(xorstr("Accent Dark")), g_Col.BaseDark);
                    Custom::ColorRow(Utils::Translate(xorstr("Accent Fill")), g_Col.BaseFill);

                    ImGui::Dummy(ImVec2(0.f, 8.f));
                    Custom::SectionLabel(Utils::Translate(xorstr("Combat")));
                    Custom::ColorRow(Utils::Translate(xorstr("Aimbot Fov")), g_Config.Aimbot->FovColor);
                    Custom::ColorRow(Utils::Translate(xorstr("Triggerbot Fov")), g_Config.TriggerBot->FovColor);
                    Custom::ColorRow(Utils::Translate(xorstr("SilentAim Fov")), g_Config.SilentAim->FovColor);

                    ImGui::Dummy(ImVec2(0.f, 8.f));
                    Custom::SectionLabel(Utils::Translate(xorstr("Players")));
                    Custom::ColorRow(Utils::Translate(xorstr("Box")), g_Config.ESP->BoxCol);
                    Custom::ColorRow(Utils::Translate(xorstr("Filled Box")), g_Config.ESP->FilledBoxCol);
                    Custom::ColorRow(Utils::Translate(xorstr("Skeleton")), g_Config.ESP->SkeletonCol);
                    Custom::ColorRow(Utils::Translate(xorstr("SnapLines")), g_Config.ESP->SnapLinesCol);

                    ImGui::Dummy(ImVec2(0.f, 8.f));
                    Custom::SectionLabel(Utils::Translate(xorstr("Text")));
                    Custom::ColorRow(Utils::Translate(xorstr("Names")), g_Config.ESP->UserNamesCol);
                    Custom::ColorRow(Utils::Translate(xorstr("Weapon Names")), g_Config.ESP->WeaponNameCol);
                    Custom::ColorRow(Utils::Translate(xorstr("Distance")), g_Config.ESP->DistanceCol);
                }
                Custom::EndCard();
                break;

            default:
                break;
            }
        }

        ImGui::PopStyleVar();
    }
}
