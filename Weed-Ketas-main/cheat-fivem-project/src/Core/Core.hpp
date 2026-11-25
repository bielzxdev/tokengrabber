#pragma once

#include <Includes/Includes.hpp>
#include <Core/Offsets.hpp>
#include <Core/SDK/SDK.hpp>
#include <Core/Features/Silent/SilentAim.hpp>
#include <Core/Features/VehicleWheelVisibility.hpp>

#include <string>
#include <regex>
#include <atomic>
#include <mutex>

namespace Core
{
    inline static bool ThreadsStarted = false;


    inline bool IsFiveMGameName(const std::wstring& name)
    {
        static const std::wregex pattern(
            []() -> std::wstring
            {
                const std::string text = xorstr("^FiveM(_b[0-9]{4})?_(GTA|Game)Process\\.exe$");

                return std::wstring(text.begin(), text.end());
            }(),
                std::regex::icase
                );

        return std::regex_match(name, pattern);
    }

    inline std::pair<DWORD, std::wstring> FindFiveMProcess(DWORD requestedPid = 0)
    {
        if (requestedPid)
        {
            const auto name = Mem.GetNameByPid(requestedPid);
            const std::wstring wideName(name.begin(), name.end());
            return IsFiveMGameName(wideName)
                ? std::make_pair(requestedPid, wideName)
                : std::make_pair(DWORD{ 0 }, std::wstring{});
        }

        std::pair<DWORD, std::wstring> selected;
        // Enumerate all game windows: FindWindow alone may return a standalone GTA window.
        EnumWindows([](HWND window, LPARAM context) -> BOOL {
            wchar_t className[64]{};
            if (!GetClassNameW(window, className, 64) || _wcsicmp(className, L"grcWindow") != 0)
                return TRUE;
            DWORD pid = 0;
            GetWindowThreadProcessId(window, &pid);
            if (!pid) return TRUE;
            auto candidate = FindFiveMProcess(pid);
            if (!candidate.first)
                return TRUE;
            *reinterpret_cast<std::pair<DWORD, std::wstring>*>(context) = std::move(candidate);
            g_Variables.g_hGameWindow = window;
            return FALSE;
            }, reinterpret_cast<LPARAM>(&selected));
        if (selected.first)
            return selected;

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return {};
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry))
        {
            do {
                if (IsFiveMGameName(entry.szExeFile))
                {
                    selected = { entry.th32ProcessID, entry.szExeFile };
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
        return selected;
    }

    inline void ClearGamePointers()
    {
        SDK::Pointers::pWorld = nullptr;
        SDK::Pointers::pLocalPlayer = nullptr;
        SDK::Pointers::pReplayInterFace = nullptr;
        SDK::Pointers::pBlipList = 0;
        SDK::Pointers::pViewPort = 0;
        SDK::Pointers::pCamGamePlayDirector = 0;
    }

    inline void ResetGameAttachment()
    {
        // Hooks/caves are process-local. Tear them down before closing the
        // old handle so the SilentAim thread cannot reuse their addresses in
        // the next FiveM instance.
        Features::g_SilentAim.Shutdown();
        g_LastFiveMPid.store(0);
        ClearGamePointers();
        g_Variables.ProcIdFiveM = 0;
        if (Mem.ProcHandle)
            CloseHandle(Mem.ProcHandle);
        Mem.ProcHandle = nullptr;
        Mem.ProcId = 0;
        Mem.ProcName.clear();
        Mem.ModBase = Mem.ModBaseSize = 0;
        Mem.ModBasePlayerNames = Mem.ModBasePlayerNamesSize = 0;
        g_Offsets = OffsetsClass{};
    }

    inline std::mutex OffsetAttachMutex;

    inline std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(' ');
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(' ');
        return str.substr(first, last - first + 1);
    }

    // change to CoreRt for best perfomace
    inline int GetBuild() {
        const wchar_t* localAppData = _wgetenv(xorstr(L"LOCALAPPDATA"));
        if (!localAppData) return -1;
        std::wstring appDataPath = localAppData;
        std::wstring iniFilePath = appDataPath + xorstr(L"\\FiveM\\FiveM.app\\CitizenFX.ini");

        std::ifstream iniFile(iniFilePath);
        if (!iniFile.is_open()) {
            return -1;
        }

        std::string line;
        while (std::getline(iniFile, line)) {
            line = trim(line);

            if (line == xorstr("ReplaceExecutable=0")) {
                return 3258;
            }
        }

        iniFile.clear();
        iniFile.seekg(0);
        while (std::getline(iniFile, line)) {
            line = trim(line);

            if (line.find((xorstr("SavedBuildNumber="))) != std::string::npos) {
                std::string buildNumberStr = line.substr(line.find("=") + 1);
                buildNumberStr = trim(buildNumberStr);
                try { return std::stoi(buildNumberStr); }
                catch (const std::exception&) { return -1; }
            }
        }

        return -1;
    }

    inline bool GetOffsets(DWORD requestedPid = 0)
    {
        std::lock_guard<std::mutex> lock(OffsetAttachMutex);
        const auto [pid, name] = FindFiveMProcess(requestedPid);
        if (!pid)
            return false;

        int build = 0;
        std::wsmatch match;
        static const std::wregex buildPattern(L"^FiveM_b([0-9]{4})_", std::regex::icase);
        if (std::regex_search(name, match, buildPattern))
            build = std::stoi(match[1].str());
        else
        {
            build = GetBuild();
            if (build < 0) build = 3258;
        }
        ResetGameAttachment();
        g_Variables.ProcIdFiveM = pid;
        Mem.ProcName = name;
        g_Offsets.CurrentBuild = build;
        if (!Mem.OpenProcByPid())
        {
            ResetGameAttachment();
            return false;
        }

        Mem.ApplyBypass();

        // Resolve fields from the attached image; never reuse a previous build layout.
        g_Offsets.m_LastVehicle = 0;
        g_Offsets.m_PlayerInfo = 0;
        g_Offsets.m_WeaponManager = 0;
        g_Offsets.m_FragInst = 0;
        g_Offsets.m_PlayerId = 0;
        g_Offsets.m_ClonePedToTarget = 0;
        g_Offsets.m_CopyPedVariation = 0;
        g_Offsets.m_VehicleState = 0;
        g_Offsets.m_VehicleExtras = 0x58B; // Existing legacy field; no signature changed here.

        auto ResolveOffsetSig = [&](const char* name, const char* pattern,
            int dispOffset) -> uintptr_t {
                const uintptr_t sigAddr = Mem.FindSignatureStr(pattern);
                int32_t disp = 0;
                if (!sigAddr || !Mem.ReadBuffer(sigAddr + dispOffset, &disp, sizeof(disp))
                    || disp <= 0 || disp >= 0x10000) return 0;
                return static_cast<uintptr_t>(disp);
            };

        // Repeated references are valid only when every site resolves the same global.
        auto ResolveSharedGlobal = [&](const char* pattern) -> uintptr_t {
            uintptr_t cursor = Mem.ModBase, target = 0;
            const uintptr_t end = Mem.ModBase + Mem.ModBaseSize;
            for (size_t count = 0; count < 32 && cursor < end; ++count) {
                const uintptr_t site = Mem.FindSignatureStr(pattern, cursor, end - cursor, false);
                if (!site) return target;
                const uintptr_t candidate = Mem.GetRipRelativeAddress(site);
                if (!candidate || (target && candidate != target)) return 0;
                target = candidate;
                cursor = site + 1;
            }
            return 0;
            };

        if (g_Offsets.CurrentBuild != 0)
        {
            // === CPed layout offsets resolvidos via signature (independente de build) ===
            // Essas instruções preservam o disp32 do offset nos bytes seguintes ao modrm.
            // Padrão: `48 8B 81 ?? ?? ?? ??` = mov rax, [rcx+disp32]. disp32 começa em sig+3.

            // CPed+0x10A8 = CPlayerInfo* — usado em getters de player
            // IDA b3258 @ 0x7ff76af6e5ab: mov rax,[rcx+10A8h]; shl r15b,2
            g_Offsets.m_PlayerInfo = ResolveOffsetSig(xorstr("m_PlayerInfo"), xorstr("48 8B 81 ? ? ? ? 41 C0 E7 02"), 3);

            // CPed+0x10B8 = CPedWeaponManager* — usado em GetCurrentWeapon
            // IDA b3258 @ 0x7ff76b09704f: mov rax,[rcx+10B8h]; test rax,rax; je; mov rcx,[rax+?]; jmp
            g_Offsets.m_WeaponManager = ResolveOffsetSig(xorstr("m_WeaponManager"), xorstr("48 8B 81 ? ? ? ? 48 85 C0 74 ? 48 8B 48 ? EB"), 3);

            // CPed+0x1430 = CFragInst* — usado em CPhysical::GetFragInst
            // IDA b3258 @ 0x7ff76b4a4d4f: mov rax,[rcx+1430h]; xor esi,esi; mov rdi,rcx; test rax,rax; je ...
            g_Offsets.m_FragInst = ResolveOffsetSig(xorstr("m_FragInst"), xorstr("48 8B 81 ? ? ? ? C3 48 89 5C 24 ? 56"), 3);

            // CommandGetVehiclePedIsIn: load CPed::m_pMyVehicle.
            g_Offsets.m_LastVehicle = ResolveOffsetSig(xorstr("m_LastVehicle"), xorstr("4C 8B 80 ? ? ? ? 40 84 F6"), 3);

            // CNetGamePlayer::GetGamerInfo identifies its CPlayerInfo subobject.
            // rlGamerInfo::operator= copies gamer ID and the first handle bytes together.
            // Extract the source/store displacements; do not match CVehicleGlassComponent.
            const uintptr_t GamerInfoSig = Mem.FindSignatureStr(
                xorstr("48 8B 81 ? ? ? ? 48 83 C0 20 C3"));
            // b3889: rlGamerInfo::operator= usa disp8 (0F 10 47 / F3 0F 7F 43)
            // em vez do disp32 das builds antigas (0F 10 87 / F3 0F 7F 83).
            const uintptr_t GamerCopySig = Mem.FindSignatureStr(
                xorstr("BA 11 00 00 00 0F 10 47 ? F3 0F 7F 43 ? 8B 87 ? ? ? ? 89 83"));
            uint8_t infoOffset = 0;
            uint8_t sourceOffset = 0, destinationOffset = 0;
            if (GamerInfoSig && GamerCopySig
                && Mem.ReadBuffer(GamerInfoSig + 10, &infoOffset, sizeof(infoOffset))
                && Mem.ReadBuffer(GamerCopySig + 8, &sourceOffset, sizeof(sourceOffset))
                && Mem.ReadBuffer(GamerCopySig + 13, &destinationOffset, sizeof(destinationOffset))
                && sourceOffset > 0 && sourceOffset < 0x1000 && sourceOffset == destinationOffset)
                g_Offsets.m_PlayerId = infoOffset + sourceOffset;
            // Resolve Offsets
            {
                // F3 0F 11 80 ? ? ? ? F3 0F 10 8F 90 01 00 00 48 8B 85 A8 10 00 00
                g_Offsets.m_Godmod = ResolveOffsetSig(xorstr("m_Godmod"), xorstr("F3 0F 11 80 ? ? ? ? F3 0F 10 8F 90 01 00 00 48 8B 85 A8 10 00 00"), 4);
                //48 8B 15 ? ? ? ? 48 C1 E1
                g_Offsets.m_CitizenNamesModBase = Mem.GetRipRelativeAddress(Mem.FindSignatureBypass({ 0x48, 0x8B, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0xC1, 0xE1 }, Mem.ModBasePlayerNames, Mem.ModBasePlayerNamesSize));

                // b3889: `IsPedVisible` lê o id em [fxDrawData+2Bh] e chama
                // OcclusionQueries::OQGetQueryResult, que indexa
                // s_OcclusionQueryArray com stride 0x80 e lê o resultado em +0x78.
                // O lea em OQAllocate aponta para o início do array; ScanOffset
                // devolve o RVA esperado por IsVisible().
                g_Offsets.m_IsEntityVisible = Mem.ScanOffset(xorstr("48 8D 05 ? ? ? ? 48 63 CB 48 C1 E1 07 48 03 C8 E8 ? ? ? ? FF 05 ? ? ? ?"), true, false);

                // 48 8B 05 ? ? ? ? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B
                g_Offsets.m_World = Mem.GetRipRelativeAddress(Mem.FindSignatureStr(xorstr("48 8B 05 ? ? ? ? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B"))
                );

                // 48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? 8A D8 E8 ? ? ? ? 84 DB 75 13 48 8D 0D ? ? ? ? 48 8B D7 E8 ? ? ? ? 84 C0 74 BC 8B 8F
                g_Offsets.m_ReplayInterFace = ResolveSharedGlobal(xorstr("48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? 8A D8 E8 ? ? ? ? 84 DB 75 13 48 8D 0D ? ? ? ? 48 8B D7 E8 ? ? ? ? 84 C0 74 BC 8B 8F"));

                // 48 8B 15 ?? ?? ?? ?? 48 8D 2D ?? ?? ?? ?? 48 8B CD
                g_Offsets.m_ViewPort = Mem.GetRipRelativeAddress(Mem.FindSignatureStr(xorstr("48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD")));

                // b3889: camInterface::GetGameplayDirectorSafe — mov rax,[rip+...]; ret;
                // O prólogo seguinte torna o padrão único e resolve camInterface::ms_GameplayDirector.
                g_Offsets.m_CamGameplayDirector = Mem.GetRipRelativeAddress(Mem.FindSignatureStr(xorstr("48 8B 05 ? ? ? ? C3 48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 49 8B F1")));
            }

            //Functions Signatures
            {
                // CPedIntelligence::FindTaskByType. The supplied call-site
                g_Offsets.m_FindTaskByType = Mem.FindSignatureStr(xorstr("48 89 5C 24 08 57 48 83 EC 20 48 8B F9 48 8B 89 ? ? ? ? 41 B8 04 ? ? ? 8B DA E8 ? ? ? ? 48 85 C0"));

                // CPedIntelligence immediately precedes CPlayerInfo in CPed.
                g_Offsets.m_PedIntelligence = g_Offsets.m_PlayerInfo ? g_Offsets.m_PlayerInfo - sizeof(uintptr_t) : 0;
                g_Offsets.m_AimTaskType = 4;
                g_Offsets.m_AimTaskShootFlagOffset = 0x12C;
                g_Offsets.m_AimTaskIsShootingBit = 0x2;

                // E8 ? ? ? ? 48 83 C3 60 48 FF CF 75 E6 48 8B 5C 24
                g_Offsets.m_ArmsKinematics = Mem.FindSignatureStr(xorstr("E8 ? ? ? ? 48 83 C3 60 48 FF CF 75 E6 48 8B 5C 24"));

                // E8 ? ? ? ? 48 83 C3 60 48 FF CF 75 DF 48 8B 5C 24 ? 48 8B 6C 24 ? 48 8B 74 24
                g_Offsets.m_LegsKinematics = Mem.FindSignatureStr(xorstr("E8 ? ? ? ? 48 83 C3 60 48 FF CF 75 DF 48 8B 5C 24 ? 48 8B 6C 24 ? 48 8B 74 24"));

                // 48 8D 45 ? F3 0F 10 ? F3 0F 10 48 ? F3 0F 11 45
                g_Offsets.m_SilentAim = Mem.FindSignatureStr(xorstr("48 8D 45 ? F3 0F 10 ? F3 0F 10 48 ? F3 0F 11 45"));

                // 0F 29 4F ? 83 8F ? ? ? ? ? 48 8B 4F
                g_Offsets.m_MagicBulletsPatch = Mem.FindSignatureStr(xorstr("0F 29 4F ? 83 8F ? ? ? ? ? 48 8B 4F"));

                // 08 4F 38 F3 41 0F 10 19
                g_Offsets.m_HandleBullet = Mem.FindSignatureStr(xorstr("08 4F 38 F3 41 0F 10 19"));

                // 0F B7 CA 83 F9 07 7F 5E
                g_Offsets.m_OffsetPool = Mem.FindSignatureStr(xorstr("0F B7 CA 83 F9 07 7F 5E"));

                // 48 8D 4D 20 44 89 75 20 E8 ? ? ? ? 48 85 C0
                g_Offsets.m_FreecanPatch = Mem.FindSignatureStr(xorstr("48 8D 4D 20 44 89 75 20 E8 ? ? ? ? 48 85 C0"));

                // 89 81 ? ? ? ? 8B 87 ? ? ? ? F7 D0
                g_Offsets.m_InfiniteCombatRoll = Mem.FindSignatureStr(xorstr("89 81 ? ? ? ? 8B 87 ? ? ? ? F7 D0"));

                // CWeapon::UpdateAmmoAfterFiring: subtract shots fired from clip ammo.
                {
                    uintptr_t a0 = Mem.FindSignatureStr(xorstr("0F BF 4F 56 B8 FF 7F 00 00 2B CD 3B C8 0F 4D C8"));
                    g_Offsets.m_InfiniteAmmo0 = a0;
                }

                // CWeapon::UpdateAmmoAfterFiring: subtract shots fired from total ammo.
                {
                    uintptr_t a1 = Mem.FindSignatureStr(xorstr("0F BF 57 54 48 8B CF 2B D5 E8"));
                    g_Offsets.m_InfiniteAmmo1 = a1;
                }

                // 48 85 D2 0F 84 ? ? ? ? 48 8B C4 48 89 58 ? 48 89 68 ? 48 89 70 ? 48 89 78 ? 41 56 48 83 EC ? 49 8B D8
                g_Offsets.m_ClonePedToTarget = Mem.FindSignatureStr(xorstr("48 85 D2 0F 84 ? ? ? ? 48 8B C4 48 89 58 ? 48 89 68 ? 48 89 70 ? 48 89 78 ? 41 56 48 83 EC ? 49 8B D8"));

                // CPedFactory::CopyVariation. Resolve the callee directly instead
                // of depending on a fixed instruction offset inside ClonePedToTarget.
                // IDA b3258: 48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 81 EC ? ? ? ? 49 8B E8
                g_Offsets.m_CopyPedVariation = Mem.FindSignatureStr(
                    xorstr("48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 81 EC ? ? ? ? 49 8B E8"),
                    0, 0, true);

                // 8D 42 ? 83 F8 ? 77 ? 48 98 0F B6 94 08
                g_Offsets.m_GetWheelFromId = Mem.FindSignatureStr(xorstr("8D 42 ? 83 F8 ? 77 ? 48 98 0F B6 94 08"));

                // 48 83 EC ? 8B 05 ? ? ? ? 8B 3D ? ? ? ? 49
                g_Offsets.m_Vehicle01 = 0; // Unused legacy entry; not validated in the protected image.

                // 48 89 5C 24 ? 48 89 74 24 ? 55 57 41 56 48 8B EC 48 83 EC ? 83 61
                g_Offsets.m_RequestModel = Mem.FindSignatureStr(xorstr("48 89 5C 24 ? 48 89 7C 24 ? 55 48 8B EC 48 83 EC 50 8B 45"));

                // 48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC ? 41 8B F0 8B FA 48 8B D9 E8
                g_Offsets.m_GiveWeapon = Mem.FindSignatureStr(xorstr("48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC ? 41 8B F0 8B FA 48 8B D9 E8"));

                // 48 8B C4 48 89 58 ? 48 89 68 ? 48 89 70 ? 48 89 78 ? 41 54 41 55 41 56 48 83 EC ? 41 8A F9
                g_Offsets.m_GiveWeaponInHand = Mem.FindSignatureStr(xorstr("48 8B C4 48 89 58 ? 48 89 68 ? 48 89 70 ? 48 89 78 ? 41 54 41 55 41 56 48 83 EC ? 41 8A F9"));

                // CPhysical::SetVelocity(CPhysical*, const rage::Vector3*).
                // Non-zero velocity wakes a sleeping physics instance before applying movement.
                // b3889: CPhysical::SetVelocity — ramo 0F 84 9C 00 00 00 distingue
                // de CPhysical::SetAngVelocity (que usa 0F 84 88 00 00 00).
                g_Offsets.m_SetPhysicalVelocity = Mem.FindSignatureStr(xorstr("48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B 41 ? 0F 29 74 24 30 48 8B FA 48 8B D9 48 85 C0 0F 84 9C 00 00 00"));

                g_Offsets.m_SetEntityVelocityNative = 0; // Unused legacy entry; not validated in the protected image.

                g_Offsets.m_GetScriptGuidFromBase = Mem.FindSignatureStr(xorstr("48 83 EC 28 8B 15 ? ? ? ? 48 83 C1 10 E8 ? ? ? ? 48 85 C0 74 ? 48 8B 0D"));

                if (!g_Offsets.m_GetScriptGuidFromBase)
                {
                    // IDA: rage::fwScriptGuid::CreateGuid(fwExtensibleBase&).
                    // This is the long-lived ptr-to-handle signature used by old
                    // and new game builds. It has the same RCX input/EAX output
                    // ABI and creates the extension when one is not present.
                    g_Offsets.m_GetScriptGuidFromBase = Mem.FindSignatureStr(xorstr("48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 8B 15 ? ? ? ? 48 8B F9 48 83 C1 10 33 DB"));
                }

                g_Offsets.m_IsPedInParachuteFreeFallNative = 0; // Unused legacy entry; not validated in the protected image.

                g_Offsets.m_GetEntitySpeedNative = 0; // Unused legacy entry; not validated in the protected image.

                // TASK_LEAVE_ANY_VEHICLE native (hash 0x504D54DF3F6F2247)
                // Procura pela função que corresponde à native TASK_LEAVE_ANY_VEHICLE
                // Padrão típico de native: prólogo com sub rsp seguido de operações específicas
                g_Offsets.m_TaskLeaveAnyVehicleNative = Mem.FindSignatureStr(xorstr("48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 65 4C 8B 0C 25"));

                // Natives do rocket boost. Depois de habilitar a flag 126 e
                // m_bKERSAllowed, HornetBoost chama Fill(1.0f) e Active(true)
                // a cada tick da buzina.
                g_Offsets.m_SetRocketBoostFillNative = Mem.FindSignatureStr(xorstr("40 53 48 83 EC 40 0F 29 74 24 ? 0F 29 7C 24 ? 0F 28 F9"));
                g_Offsets.m_SetRocketBoostActiveNative = Mem.FindSignatureStr(xorstr("48 89 5C 24 ? 57 48 83 EC 30 0F 29 74 24 ? 40 8A FA E8"));

                // Procura o store do fuel dentro da função. O movss escreve
                // em [rbx+disp32] e disp32 é lido como int32.
                g_Offsets.m_RocketBoostFuelOffset = 0x320;  // fallback (Steam GTA)
                if (g_Offsets.m_SetRocketBoostFillNative)
                {
                    // Varre os primeiros 0x80 bytes da função procurando
                    // "F3 0F 11 83" (movss dword [rbx+disp32], xmm0).
                    uint8_t buf[0x80]{};
                    if (Mem.ReadBuffer(g_Offsets.m_SetRocketBoostFillNative, buf, sizeof(buf)))
                    {
                        for (size_t i = 0; i + 8 <= sizeof(buf); ++i)
                        {
                            if (buf[i] == 0xF3 && buf[i + 1] == 0x0F &&
                                buf[i + 2] == 0x11 && buf[i + 3] == 0x83)
                            {
                                uint32_t disp = *reinterpret_cast<uint32_t*>(buf + i + 4);
                                if (disp > 0 && disp < 0x2000)
                                    g_Offsets.m_RocketBoostFuelOffset = disp;
                                break;
                            }
                        }
                    }
                }
                // CVehicle::SetRocketBoosting(this /*RCX*/, bool /*DL*/)
                // Setter direto no CVehicle — sem GUID, sem fwScriptGuid,
                // sem FromGuid. Escreve bool em [this+boostFlagOffset] e
                // atualiza timestamp. Chamamos cada tick enquanto E segurado.
                g_Offsets.m_SetRocketBoosting = Mem.FindSignatureStr(xorstr("3A 91 ? ? ? ? 74 ? 84 D2 75 ? 88 91 ? ? ? ? 8B 05 ? ? ? ? 89 81 ? ? ? ? 88 91 ? ? ? ? C3"));

                // CVehicle::HasRocketBoost. No b3258:
                //   cmp [rcx+888h], bl               ; m_bKERSAllowed
                //   mov rcx, [rcx+20h]               ; CVehicleModelInfo*
                //   mov eax, [rcx+588h]
                //   shr eax, 1Eh                     ; flag 126
                g_Offsets.m_HasRocketBoost = Mem.FindSignatureStr(xorstr("40 53 48 83 EC 20 33 DB 4C 8B C1 38 99 ? ? ? ? 74 ? 48 8B 49 20"));
                g_Offsets.m_ProcessRocketBoostGate = Mem.FindSignatureStr(xorstr("48 8B CF E8 ? ? ? ? 84 C0 74 08 48 8B CF E8 ? ? ? ? 41 0F 28 CB"));

                g_Offsets.m_RocketBoostKersAllowedOffset = 0;
                g_Offsets.m_RocketBoostModelFlagOffset = 0;
                g_Offsets.m_RocketBoostModelFlagMask = 0;
                if (g_Offsets.m_HasRocketBoost)
                {
                    uint8_t buf[0x60]{};
                    if (Mem.ReadBuffer(g_Offsets.m_HasRocketBoost, buf, sizeof(buf)))
                    {
                        // cmp byte ptr [rcx+disp32], bl
                        for (size_t i = 0; i + 6 <= sizeof(buf); ++i)
                        {
                            if (buf[i] == 0x38 && buf[i + 1] == 0x99)
                            {
                                const uint32_t disp = *reinterpret_cast<uint32_t*>(buf + i + 2);
                                if (disp > 0 && disp < 0x2000)
                                    g_Offsets.m_RocketBoostKersAllowedOffset = disp;
                                break;
                            }
                        }

                        // mov eax,[rcx+disp32]; shr eax,1Eh; test al,1.
                        // O bit 30 equivale ao bit 6 do byte disp+3.
                        for (size_t i = 0; i + 11 <= sizeof(buf); ++i)
                        {
                            if (buf[i] == 0x8B && buf[i + 1] == 0x81 &&
                                buf[i + 6] == 0xC1 && buf[i + 7] == 0xE8 &&
                                buf[i + 8] == 0x1E && buf[i + 9] == 0xA8 &&
                                buf[i + 10] == 0x01)
                            {
                                const uint32_t disp = *reinterpret_cast<uint32_t*>(buf + i + 2);
                                if (disp > 0 && disp < 0x2000)
                                {
                                    g_Offsets.m_RocketBoostModelFlagOffset = disp + 3;
                                    g_Offsets.m_RocketBoostModelFlagMask = 0x40;
                                }
                                break;
                            }
                        }
                    }
                }
                // CRequestControlEvent::Trigger(netObject*) — pede ao server pra passar ownership.
                // b3889: chamada direta FF 50 68 em vez do indireto FF 90 das builds antigas.
                g_Offsets.m_RequestControlEventTrigger = Mem.FindSignatureStr(xorstr("48 89 5C 24 ? 57 48 83 EC 20 33 FF 48 8B D9 48 85 C9 74 ? 48 8B 01 FF 50 68 EB"));

                // 48 8B E8 33 C0 48 85 ED 0F 84
                g_Offsets.m_Invisible = Mem.FindSignatureStr(xorstr("48 8B E8 33 C0 48 85 ED 0F 84"));

                {
                    //48 89 5C 24 08 57 48 83 EC 30 8B DA 48 8B F9 E8 ? ? ? ?
                    g_Offsets.m_GetGameplayCamRot = Mem.FindSignatureStr(xorstr("48 89 5C 24 ? 57 48 83 EC 30 8B DA 48 8B F9 E8 ? ? ? ? 48 8D 4C 24 ? 48 8D 90"));

                    //48 89 5C 24 F8 48 8D 64 24 F8 48 83 EC 60
                    // CommandGetGameplayCamCoord: push rbx; sub rsp,20h; mov rbx,rcx;
                    // call <inner>; mov edx,[rax+220h]; mov [rbx],edx; ...
                    // (lê camera position de offset 220h dentro da inner struct;
                    // escreve em layout scrVector [rbx+0,+8,+10h]).
                    // Unico no b3258 em 0x7ff76b793a7c.
                    g_Offsets.m_GetGameplayCamCoord = Mem.FindSignatureStr(xorstr("40 53 48 83 EC 20 48 8B D9 E8 ? ? ? ? 8B 90 20 02 00 00"));

                    //48 89 5C 24 08 57 48 83 EC 30 8B DA 48 8B F9 E8 ? ? ? ? 48 8D 4C 24 20 44 8B C3 48 8B D0
                    g_Offsets.m_GetFinalRenderedCamRot = Mem.FindSignatureStr(xorstr("48 89 5C 24 08 57 48 83 EC 30 8B DA 48 8B F9 E8 ? ? ? ? 48 8D 4C 24 20 44 8B C3 48 8B D0"));

                    // 40 53 48 83 EC ? 48 8B D9 E8 ? ? ? ? 8B 10
                    // push rbx; sub rsp,?; mov rbx,rcx; call GetGameplayCamCoord;
                    // mov edx,[rax]  (lê X do result da inner call)
                    // Essa sig NAO colide com GetPedLastWeaponImpactCoord porque
                    // o prologue e "40 53" (push rbx) e nao "48 89 5C 24 ? 57".
                    g_Offsets.m_GetFinalRenderedCamCoord = Mem.FindSignatureStr(xorstr("40 53 48 83 EC ? 48 8B D9 E8 ? ? ? ? 8B 10"));

                }

                // GameplayCamHolder: Spectate camera holder (multiple patterns for different builds)
                // TEMPORARIAMENTE DESABILITADO - CAUSANDO PROBLEMAS
                /*
                // Pattern 1 (b3258): 4C 89 2D ? ? ? ? E8 ? ? ? ? E8
                g_Offsets.m_GameplayCamHolder = Mem.FindSignatureStr(xorstr("4C 89 2D ? ? ? ? E8 ? ? ? ? E8"));
                if (!g_Offsets.m_GameplayCamHolder)
                {
                    // Pattern 2 (b3889 alternative 1): 4C 89 2D ? ? ? ? E8
                    g_Offsets.m_GameplayCamHolder = Mem.FindSignatureStr(xorstr("4C 89 2D ? ? ? ? E8"));
                }
                if (!g_Offsets.m_GameplayCamHolder)
                {
                    // Pattern 3 (alternative 2): 48 89 0D ? ? ? ? E8 ? ? ? ?
                    g_Offsets.m_GameplayCamHolder = Mem.FindSignatureStr(xorstr("48 89 0D ? ? ? ? E8 ? ? ? ?"));
                }
                if (!g_Offsets.m_GameplayCamHolder)
                {
                    // Pattern 4 (generic mov to global): 4C 89 35 ? ? ? ? E8 ? ? ? ?
                    g_Offsets.m_GameplayCamHolder = Mem.FindSignatureStr(xorstr("4C 89 35 ? ? ? ? E8 ? ? ? ?"));
                }

                // GameplayCamTarget: Spectate camera target (works on both builds)
                // Pattern: 48 83 EC 38 0F 29 74 24 ? 0F 28 F0 0F 2F 35 ? ? ? ? 73
                g_Offsets.m_GameplayCamTarget = Mem.FindSignatureStr(xorstr("48 83 EC 38 0F 29 74 24 ? 0F 28 F0 0F 2F 35 ? ? ? ? 73"));
                */
                g_Offsets.m_GameplayCamHolder = 0;
                g_Offsets.m_GameplayCamTarget = 0;

                g_Offsets.m_IsPedShooting = 0; // Unused legacy entry; not validated in the protected image.
                // 40 53 48 83 EC 20 48 8B 41 10 48 8B D9 44 8B 40 10 8B 50 08 8B 08 E8 ? ? ? ? 0F B6 D0 48 8B 03 89 10 48 83 C4 20 5B C3
                g_Offsets.HasEntityDamaged = Mem.FindSignatureStr(xorstr("48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 41 8B F8 8B F2 E8 ? ? ? ? 33 DB"));

                // 48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 55 41 56 41 57 48 8B EC 48 83 EC 60 33 DB 4C 8B FA B8 FF FF 00 00 48 89 5D F8 48 8D 55 C0
                g_Offsets.GetShapeTest = Mem.FindSignatureStr(xorstr("48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 55 41 56 41 57 48 8B EC 48 83 EC 60 33 DB 4C 8B FA B8 FF FF ? ? 48 89 5D F8 48 8D 55 C0"));

                // GetPedLastImpact removido: o usuario patcha essa funcao
                // externamente, entao nao precisamos spoofar aqui.

                // CWeaponAnimationsManager::GetAnimFireRateModifier:
                // movss xmm0, [rax+CWeaponAnimations::AnimFireRateModifier]
                g_Offsets.m_RapidFire = Mem.FindSignatureStr(xorstr("F3 0F 10 80 F4 00 00 00 EB 03"));

                // CWeaponAnimationsManager::GetAnimBlindFireRateModifier:
                // caminho separado usado ao atirar de cobertura/sem mirar.
                g_Offsets.m_RapidFireBlind = Mem.FindSignatureStr(xorstr("F3 0F 10 80 F8 00 00 00 EB 03"));

                // 4C 8D 35 ? ? ? ? 3B 35 ? ? ? ? 74 ? 49 8B 3E
                g_Offsets.m_BlipList = Mem.GetRipRelativeAddress(Mem.FindSignatureStr(xorstr("4C 8D 35 ? ? ? ? 3B 35 ? ? ? ? 74 ? 49 8B 3E")));

                // CMiniMap::GetActiveWaypoint: cmp network flag / lea waypoint array.
                // IDA: 0x1402C15A6 in the reference binary; resolve at runtime for ASLR.
                g_Offsets.m_WaypointData = 0;
                g_Offsets.m_NetworkGameInProgress = 0;
                const uintptr_t WaypointSig = Mem.FindSignatureStr(xorstr("80 3D ? ? ? ? ? 4C 8D 05 ? ? ? ? 74 ? B8 03 ? ? ? EB ? 33 C0 49 8B C8"));
                int32_t NetworkDisplacement = 0;
                int32_t WaypointDisplacement = 0;
                if (WaypointSig
                    && Mem.ReadBuffer(WaypointSig + 2, &NetworkDisplacement, sizeof(NetworkDisplacement))
                    && Mem.ReadBuffer(WaypointSig + 10, &WaypointDisplacement, sizeof(WaypointDisplacement))) {
                    g_Offsets.m_NetworkGameInProgress = WaypointSig + 7 + NetworkDisplacement;
                    g_Offsets.m_WaypointData = WaypointSig + 14 + WaypointDisplacement;
                }
                //// 48 8B C4 44 89 48 ?? 44 89 40 ?? 48 89 50 ?? 48 89 48 ?? 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 ?? ?? ?? ?? 48 81 EC ?? ?? ?? ?? 83 BA
                g_Offsets.m_SetPedInVehicleFunc = Mem.FindSignatureStr(xorstr("48 8B C4 44 89 48 ? 44 89 40 ? 48 89 50 ? 48 89 48 ? 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 ? ? ? ? 48 81 EC ? ? ? ? 83 BA"));

                // 48 8B C4 48 89 58 ? 48 89 68 ? 48 89 70 ? 57 41 56 41 57 48 83 EC ? 80 3D ? ? ? ? ? 0F 29 78
                g_Offsets.m_BreakOffWheel = Mem.FindSignatureStr(xorstr("48 8B C4 48 89 58 ? 48 89 68 ? 48 89 70 ? 57 41 56 41 57 48 83 EC ? 80 3D ? ? ? ? ? 0F 29 78"));

                //f3 0f 11 47 ? f3 0f 10 44 24 ? f3 0f 11 4f ? f3 0f 10 4c 24 ? f3 0f 11 47 ? f3 0f 11 4f ? e8
                g_Offsets.m_FreecanPatch = Mem.FindSignatureStr(xorstr("F3 0F 11 47 ? F3 0F 10 44 24 ? F3 0F 11 4F ? F3 0F 10 4C 24 ? F3 0F 11 47 ? F3 0F 11 4F ? E8"));
            }

            //Offsets Update
            {
                // Helper: resolve o endereço da instrução via sig e lê o offset interno (byte N) com cheque de 0.
                auto ReadOffsetFromSig = [&](const char* name, uintptr_t sigAddr, int ripOffset, auto typeTag) -> int {
                    using T = decltype(typeTag);
                    if (!sigAddr) {
                        return 0;
                    }
                    T value{};
                    if (!Mem.ReadBuffer(sigAddr + ripOffset, &value, sizeof(value))
                        || value <= 0 || static_cast<uint64_t>(value) >= 0x10000) return 0;
                    const int val = static_cast<int>(value);
                    return val;
                    };

                // b3889: CPlayerInfo::GetRunSprintSpeedMultiplier lê [rcx+disp32].
                // O compare anterior com a global torna o padrão único.
                g_Offsets.m_Speed = ReadOffsetFromSig(xorstr("m_Speed"),
                    Mem.FindSignatureStr(xorstr("77 08 F3 0F 10 83 ? ? ? ? 48 83 C4 20 5B C3")),
                    6, int{});

                //f3 0f 11 80 ? ? ? ? 48 8b 87 ? ? ? ? f3 0f 10 05
                g_Offsets.m_VehicleEngineHealth = ReadOffsetFromSig(xorstr("m_VehicleEngineHealth"),
                    Mem.FindSignatureStr(xorstr("F3 0F 11 80 ? ? ? ? 48 8B 87 ? ? ? ? F3 0F 10 05")),
                    4, int{});

                //f3 41 0f 10 b0 ? ? ? ? 48 8b ca
                g_Offsets.m_Spread = ReadOffsetFromSig(xorstr("m_Spread"),
                    Mem.FindSignatureStr(xorstr("F3 41 0F 10 B0 ? ? ? ? 48 8B CA")),
                    5, int{});

                //f3 0f 10 b0 ? ? ? ? 49 8b 87
                g_Offsets.m_Recoil = ReadOffsetFromSig(xorstr("m_Recoil"),
                    Mem.FindSignatureStr(xorstr("F3 0F 10 B0 ? ? ? ? 49 8B 87")),
                    4, int{});

                //4C 8B 41 ? 33 D2 4D 85 C0 74 ? 49 8B 80 ? ? ? ? (CPedWeaponManager::GetCurrentWeaponInfoForHeldObject)
                uintptr_t WeaponObjSigAddr = Mem.FindSignatureStr(xorstr("4C 8B 41 ? 33 D2 4D 85 C0 74 ? 49 8B 80 ? ? ? ?"));
                g_Offsets.m_CObject = WeaponObjSigAddr ? (int)Mem.Read<BYTE>(WeaponObjSigAddr + 3) : 0;
                g_Offsets.m_CWeapon = WeaponObjSigAddr ? Mem.Read<int>(WeaponObjSigAddr + 14) : 0;
                //4c 8b 94 c7 ? ? ? ? eb ? 4d 8b d6 41 8a 82
                g_Offsets.m_VehicleDriver = ReadOffsetFromSig(xorstr("m_VehicleDriver"),
                    Mem.FindSignatureStr(xorstr("4C 8B 94 C7 ? ? ? ? EB ? 4D 8B D6 41 8A 82")),
                    4, int{});

                //8b 81 ? ? ? ? 83 e0 ? c1 e0 ? 3d ? ? ? ? 0f 8f
                g_Offsets.m_NoRagDoll = ReadOffsetFromSig(xorstr("m_NoRagDoll"),
                    Mem.FindSignatureStr(xorstr("8B 81 ? ? ? ? 83 E0 ? C1 E0 ? 3D ? ? ? ? 0F 8F")),
                    2, int{});

                //f6 81 ? ? ? ? ? 75 ? 8b 83
                g_Offsets.m_SeatBealt = ReadOffsetFromSig(xorstr("m_SeatBealt"),
                    Mem.FindSignatureStr(xorstr("F6 81 ? ? ? ? ? 75 ? 8B 83")),
                    2, int{});

                //74 ? 83 b9 ? ? ? ? ? 75 ? 45 84 f6
                g_Offsets.m_VehicleDoorsLockState = ReadOffsetFromSig(xorstr("m_VehicleDoorsLockState"),
                    Mem.FindSignatureStr(xorstr("74 ? 83 B9 ? ? ? ? ? 75 ? 45 84 F6")),
                    4, int{});

                //48 8b 83 ? ? ? ? 8b d5 48 89 44 24 ? 4c 89 74 24
                g_Offsets.m_Handling = ReadOffsetFromSig(xorstr("m_Handling"),
                    Mem.FindSignatureStr(xorstr("48 8B 83 ? ? ? ? 8B D5 48 89 44 24 ? 4C 89 74 24")),
                    3, int{});

                // CommandGetPlayerSprintStaminaRemaining — movss xmm0,[rax+Max]; subss xmm0,[rax+Cur].
                // b3889 termina em EB 03 em vez de 0F 2F 41; os disp32 continuam em +4 e +12.
                uintptr_t StaminaSigAddr = Mem.FindSignatureStr(xorstr("F3 0F 10 80 ? ? ? ? F3 0F 5C 80 ? ? ? ?"));
                g_Offsets.m_MaxStamina = StaminaSigAddr ? Mem.Read<int>(StaminaSigAddr + 4) : 0;
                g_Offsets.m_Stamina = StaminaSigAddr ? Mem.Read<int>(StaminaSigAddr + 12) : 0;
                //83 E1 1F D3 E2 48 ?? ?? 48 C1 E9 05 85 94 ?? ?? ?? ?? ?? (CommandSetPedConfigFlag)
                uintptr_t PedFlagSigAddr = Mem.FindSignatureStr(xorstr("85 94 8B ? ? ? ? 0F 95 C0"));
                if (PedFlagSigAddr)
                    g_Offsets.m_PedFlag = Mem.Read<int>(PedFlagSigAddr + 3);
                //8B 87 ?? ?? ?? ?? F3 0F 10 ?? ?? ?? ?? ?? ?? ?? 01 ?? ?? ?? C1 E8 1F (CAddCompositeSkeletonCommand — Shrink flag check)
                uintptr_t ShrinkCheckAddr = Mem.FindSignatureStr(xorstr("8B 87 ? ? ? ? F3 0F 10 ? ? ? ? ? ? ? 01 ? ? ? C1 E8 1F"));
                if (ShrinkCheckAddr) {
                    uintptr_t dataCave = Mem.CreateDataCave(16);
                    uintptr_t codeCave = dataCave ? Mem.CreateCodeCaveNearby(ShrinkCheckAddr, 0x80) : 0;
                    if (codeCave) {
                        Mem.Write<uint8_t>(dataCave, 0);
                        Mem.Write<uint64_t>(dataCave + 8, 0);

                        std::vector<uint8_t> origMov = Mem.ReadBytes(ShrinkCheckAddr, 6);
                        uintptr_t retAddr = ShrinkCheckAddr + 6;

                        auto p32 = [](std::vector<uint8_t>& v, uint32_t val) {
                            v.push_back((uint8_t)val); v.push_back((uint8_t)(val >> 8));
                            v.push_back((uint8_t)(val >> 16)); v.push_back((uint8_t)(val >> 24));
                            };
                        auto p64 = [](std::vector<uint8_t>& v, uint64_t val) {
                            for (int i = 0; i < 8; i++) v.push_back((uint8_t)(val >> (i * 8)));
                            };

                        std::vector<uint8_t> cave;
                        cave.push_back(0x51);                                           // push rcx
                        cave.push_back(0x48); cave.push_back(0xB9); p64(cave, dataCave);// mov rcx, dataCave
                        cave.insert(cave.end(), { 0x48,0x3B,0x79,0x08 });                 // cmp rdi,[rcx+8]
                        cave.insert(cave.end(), { 0x75, 24 });                            // jne .original (+24)
                        cave.insert(cave.end(), { 0x80,0x39,0x00 });                      // cmp byte [rcx],0
                        cave.insert(cave.end(), { 0x74, 11 });                            // je  .zero (+11)

                        cave.push_back(0x59);                                           // pop rcx  (shrunk)
                        cave.push_back(0xB8); p32(cave, 0x80000000);                    // mov eax,80000000h
                        cave.push_back(0xE9); p32(cave, (uint32_t)((int64_t)retAddr - (int64_t)(codeCave + cave.size() + 4))); // jmp back

                        cave.push_back(0x59);                                           // pop rcx  (zero)
                        cave.insert(cave.end(), { 0x33,0xC0 });                           // xor eax,eax
                        cave.push_back(0xE9); p32(cave, (uint32_t)((int64_t)retAddr - (int64_t)(codeCave + cave.size() + 4))); // jmp back

                        cave.push_back(0x59);                                           // pop rcx  (original)
                        cave.insert(cave.end(), origMov.begin(), origMov.end());         // mov eax,[rdi+origOff]
                        cave.push_back(0xE9); p32(cave, (uint32_t)((int64_t)retAddr - (int64_t)(codeCave + cave.size() + 4))); // jmp back

                        Mem.UnsealCave(codeCave, 0x80);
                        Mem.WriteBytes(codeCave, cave);
                        Mem.SealCave(codeCave, 0x80);

                        int32_t hookRel = (int32_t)((int64_t)codeCave - (int64_t)(ShrinkCheckAddr + 5));
                        Mem.WriteBytes(ShrinkCheckAddr, { 0xE9, (uint8_t)hookRel, (uint8_t)(hookRel >> 8), (uint8_t)(hookRel >> 16), (uint8_t)(hookRel >> 24), 0x90 });

                        g_Offsets.m_ShrinkDataCave = dataCave;
                        g_Offsets.m_ShrinkPatched = true;
                    }
                }

                // b3889: CommandGetPedType lê [rax+10B8h]; shl 0Bh; sar 19h —
                // O mesmo layout usado pelo GetPedType() do cheat.
                uintptr_t EntityTypeSigAddr = Mem.FindSignatureStr(xorstr("8B 80 ? ? ? ? C1 E0 0B C1 F8 19 48 83 C4"));
                g_Offsets.m_EntityType = EntityTypeSigAddr ? Mem.Read<int>(EntityTypeSigAddr + 2) : 0;
                // b3889: CPhysical::GetHealth — lea rbx,[rcx+280h]; o disp32 está em +3.
                uintptr_t HealthSigAddr = Mem.FindSignatureStr(xorstr("48 8D 99 ? ? ? ? 32 C9 48 8B 7B 10 EB"));
                g_Offsets.m_Health = HealthSigAddr ? Mem.Read<int>(HealthSigAddr + 3) : 0;
                // CGameWorld's safe health/max-health getters share a body.
                // Select the second distinct field only after checking their CPedFactory global.
                g_Offsets.m_MaxHealth = 0;
                uintptr_t cursor = Mem.ModBase;
                const uintptr_t end = Mem.ModBase + Mem.ModBaseSize;
                int getters = 0, other = 0;
                bool sawHealth = false, valid = true;
                while (cursor < end && getters < 3) {
                    const uintptr_t site = Mem.FindSignatureStr(
                        xorstr("48 8B 05 ? ? ? ? 48 85 C0 74 04 48 8B 40 08 48 85 C0 75 04 0F 57 C0 C3 F3 0F 10 80 ? ? ? ? C3"),
                        cursor, end - cursor, false);
                    if (!site) break;
                    ++getters;
                    int32_t field = 0;
                    if (Mem.GetRipRelativeAddress(site) != g_Offsets.m_World
                        || !Mem.ReadBuffer(site + 29, &field, sizeof(field)) || field <= 0 || field >= 0x10000)
                        valid = false;
                    else if (field == g_Offsets.m_Health) sawHealth = true;
                    else if (other && other != field) valid = false;
                    else other = field;
                    cursor = site + 1;
                }
                if (valid && getters == 2 && sawHealth && other) g_Offsets.m_MaxHealth = other;
                // b3889: CPed::SetArmour lê/escreve o pool de armadura em [ped+1550h].
                uintptr_t ArmorSigAddr = Mem.FindSignatureStr(xorstr("F3 0F 10 86 50 15 00 00"));
                g_Offsets.m_Armor = ArmorSigAddr ? Mem.Read<int>(ArmorSigAddr + 4) : 0;
                // CVehicle::SwitchEngineOn: and byte [rcx+VehicleState], ?; mov byte [rcx+?], ?; call ...
                // 80 A1 ? ? ? ? ? C6 81 ? ? ? ? ? E8 — único no b3258. Offset em sig+2 (disp32).
                uintptr_t VehicleStateSigAddr = Mem.FindSignatureStr(xorstr("80 A1 ? ? ? ? ? C6 81 ? ? ? ? ? E8"));
                if (VehicleStateSigAddr)
                    g_Offsets.m_VehicleState = Mem.Read<uint32_t>(VehicleStateSigAddr + 2);
                else
                    g_Offsets.m_VehicleState = 0;
                // CommandSetEntityInvincible: bts/btr [rdi+InvFlags], imm8; and ebx,1; shl ebx,8
                // Nova sig: 0F BA B7 ? ? ? ? ? 83 E3 01 C1 E3 08 — único; offset em sig+3.
                // Bit 8 is part of the native semantics, not a layout displacement.
                g_Offsets.m_InvincibilityFlags = 0;
                const uintptr_t InvincibleSig = Mem.FindSignatureStr(xorstr("0F BA B7 ? ? ? ? 08 83 E3 01 C1 E3 08 09 9F ? ? ? ?"));
                int32_t FlagOffset = 0;
                if (InvincibleSig
                    && Mem.ReadBuffer(InvincibleSig + 3, &FlagOffset, sizeof(FlagOffset))
                    && FlagOffset >= 0x100 && FlagOffset < 0x1000)
                    g_Offsets.m_InvincibilityFlags = static_cast<uintptr_t>(FlagOffset);

            }

        }
        else
        {
            return false;
        }

        DWORD exitCode = 0;
        if (!g_Offsets.m_World || !g_Offsets.m_ReplayInterFace || !g_Offsets.m_ViewPort
            || GetProcessId(Mem.ProcHandle) != pid
            || !GetExitCodeProcess(Mem.ProcHandle, &exitCode) || exitCode != STILL_ACTIVE)
        {
            ResetGameAttachment();
            return false;
        }
        g_LastFiveMPid.store(pid);
        return true;
    }
}

#include "Features/Exploits/Exploits.hpp"
#include "Features/Silent/MagicBullets.hpp"
#include "Features/Silent/SilentAim.hpp"
#include "Features/Aimbot/Aimbot.hpp"
#include "Features/TriggerBot/TriggerBot.hpp"
#include "Features/Esp/ESP.hpp"
#include "Core\Features\Exploits\ResourceList.hpp"
#include "Core\Features\Exploits\ResourceDumper.hpp"

#include "Threads/ScreenResolution.hpp"
#include "Threads/UpdatePointers.hpp"
#include "Threads/VehicleList.hpp"
#include "Threads/UpdateNames.hpp"
#include "Threads/EntityList.hpp"
#include "Threads/AutoDetect.hpp"

using namespace Core::Threads;

namespace Core
{
    inline void StartThreads()
    {
        if (ThreadsStarted) return;

        // Set this before launching workers so a second call cannot start a
        // duplicate set while the first one is still being created.
        ThreadsStarted = true;

        // Hosted inside Brave (Shift+F6): the worker threads read FiveM memory
        // from a process the game never expected and can fault, killing the
        // whole host. The overlay and the Insert menu do not need them.
        if (Features::RunningInsideBraveHost())
            return;

        // Info
        std::thread(&Threads::cScreenResolution::Update, Threads::g_ScreenResolution).detach();
        std::thread(&Threads::cUpdatePtrs::Update, Threads::g_UpdatePtrs).detach();
        std::thread(&Threads::cEntityList::Update, &Threads::g_EntityList).detach();
        std::thread(&Threads::cVehicleList::Update, &Threads::g_VehicleList).detach();
        std::thread(&Threads::cUpdateNames::Update, &Threads::g_UpdateNames).detach();

        // Features
        std::thread(&Features::Exploits::PersistentLoop).detach();
        std::thread(&Features::Exploits::cResourceList::List, &Features::Exploits::g_ResourceList).detach();
        //std::thread(&Features::Exploits::cResourceDumper::Poll, &Features::Exploits::g_ResourceDumper).detach();
        std::thread(&Threads::cAutoDetect::Update, &Threads::g_AutoDetect).detach();

        // Combat
        std::thread(&Core::Features::cAimbot::Start, Core::Features::g_Aimbot).detach();
        std::thread(&Core::Features::cSilentAim::HookSilent, &Core::Features::g_SilentAim).detach();
        std::thread(&Core::Features::cTriggerbot::Start, Core::Features::g_Triggerbot).detach();
    }
}
