#pragma once
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <chrono>
#include <Core/SDK/SDK.hpp>

namespace Core::Threads
{
    class cUpdateNames
    {
    private:
        uintptr_t ModuleBase = 0, ModuleBaseSize = 0, GameBase = 0;
        uintptr_t SignatureAddress = 0, PlayerListEntry = 0;
        DWORD ModulePid = 0, SnapshotPid = 0;
        uint32_t ModuleBuild = 0, SnapshotBuild = 0;
        int Failures = 0;
        std::chrono::steady_clock::time_point NextScan{};
        const std::vector<uint8_t> PlayerListEntrySig =
            {0x48, 0x8B, 0x15, 0, 0, 0, 0, 0x48, 0xC1, 0xE1};
        std::mutex m_networkMapMutex;
        std::unordered_map<int, Core::SDK::Game::NetworkInfo> NetworkMap;

        void ClearNames()
        {
            std::lock_guard<std::mutex> lock(m_networkMapMutex);
            NetworkMap.clear();
            SnapshotPid = 0;
            SnapshotBuild = 0;
        }

        void ResetCache()
        {
            ClearNames();
            ModuleBase = ModuleBaseSize = GameBase = 0;
            SignatureAddress = PlayerListEntry = 0;
            ModulePid = ModuleBuild = 0;
            Failures = 0;
            NextScan = {};
        }

        bool InModule(uintptr_t Address, size_t Size) const
        {
            if (!ModuleBase || Address < ModuleBase || Address - ModuleBase >= ModuleBaseSize)
                return false;
            return Size <= ModuleBaseSize - (Address - ModuleBase);
        }

        static bool UserPointer(uintptr_t Address)
        {
            return Address >= 0x10000 && Address < 0x0000800000000000ULL;
        }

        bool ValidateSignature()
        {
            std::array<uint8_t, 10> Code{};
            if (!InModule(SignatureAddress, Code.size())
                || !Mem.ReadBuffer(SignatureAddress, Code.data(), Code.size())) return false;
            for (size_t i = 0; i < Code.size(); ++i)
                if (PlayerListEntrySig[i] && Code[i] != PlayerListEntrySig[i]) return false;
            int32_t Displacement = 0;
            std::memcpy(&Displacement, Code.data() + 3, sizeof(Displacement));
            const uintptr_t Resolved = SignatureAddress + 7 + Displacement;
            return InModule(Resolved, 16) && Resolved == PlayerListEntry;
        }

        static bool ReadName(uintptr_t Address, std::string& Name)
        {
            struct RemoteString { char Storage[16]; uint64_t Size; uint64_t Capacity; } Value{};
            if (!Mem.ReadBuffer(Address, &Value, sizeof(Value))
                || Value.Size > 1024 || Value.Capacity < Value.Size) return false;
            Name.clear();
            if (!Value.Size) return true;
            if (Value.Capacity <= 15)
            {
                Name.assign(Value.Storage, static_cast<size_t>(Value.Size));
                return true;
            }
            uintptr_t Data = 0;
            std::memcpy(&Data, Value.Storage, sizeof(Data));
            if (!UserPointer(Data)) return false;
            Name.resize(static_cast<size_t>(Value.Size));
            return Mem.ReadBuffer(Data, Name.data(), Name.size());
        }

    public:
        void GetPlayerNamesFromMemory()
        {
            // GetOffsets owns the attachment lock while replacing the handle and offsets.
            std::unique_lock<std::mutex> AttachmentLock(Core::OffsetAttachMutex, std::try_to_lock);
            if (!AttachmentLock.owns_lock()) { ClearNames(); return; }
            const DWORD Pid = g_LastFiveMPid.load();
            DWORD ExitCode = 0;
            if (!Pid || Pid != Mem.ProcId || Pid != g_Variables.ProcIdFiveM
                || !Mem.ProcHandle || GetProcessId(Mem.ProcHandle) != Pid
                || !GetExitCodeProcess(Mem.ProcHandle, &ExitCode) || ExitCode != STILL_ACTIVE)
            { ResetCache(); return; }

            uintptr_t Size = 0;
            const uintptr_t Base = Mem.GetModuleBaseAddr(Pid, xorstr("citizen-playernames-five.dll"), &Size);
            if (!Base || !Size) { ResetCache(); return; }
            if (ModulePid != Pid || ModuleBuild != g_Offsets.CurrentBuild
                || GameBase != Mem.ModBase || ModuleBase != Base || ModuleBaseSize != Size)
            {
                ResetCache();
                ModulePid = Pid; ModuleBuild = g_Offsets.CurrentBuild;
                GameBase = Mem.ModBase; ModuleBase = Base; ModuleBaseSize = Size;
            }

            if (PlayerListEntry && !ValidateSignature())
            {
                ClearNames();
                SignatureAddress = PlayerListEntry = 0;
                Failures = 0; NextScan = {};
            }
            if (!PlayerListEntry)
            {
                const auto Now = std::chrono::steady_clock::now();
                if (Now < NextScan) return;
                SignatureAddress = Mem.FindSignatureBypass(PlayerListEntrySig, ModuleBase, ModuleBaseSize);
                PlayerListEntry = SignatureAddress ? Mem.GetRipRelativeAddress(SignatureAddress) : 0;
                if (!PlayerListEntry || !ValidateSignature())
                {
                    SignatureAddress = PlayerListEntry = 0;
                    ++Failures;
                    NextScan = Now + std::chrono::seconds(Failures >= 3 ? 60 : 4);
                    ClearNames();
                    return;
                }
                Failures = 0;
            }

            uintptr_t Head = 0;
            int Count = 0;
            if (!Mem.ReadBuffer(PlayerListEntry, &Head, sizeof(Head))
                || !Mem.ReadBuffer(PlayerListEntry + 8, &Count, sizeof(Count))
                || Count < 0 || Count > 2048) { ClearNames(); return; }
            // An empty table can be valid while connecting. Keep the checked signature,
            // clear old players, and read its current pointer again on the next update.
            if (!Count || !UserPointer(Head)) { ClearNames(); return; }
            uintptr_t Entry = 0;
            if (!Mem.ReadBuffer(Head + 8, &Entry, sizeof(Entry))) { ClearNames(); return; }
            std::unordered_map<int, Core::SDK::Game::NetworkInfo> Names;
            std::unordered_set<uintptr_t> Visited;
            Names.reserve(static_cast<size_t>(Count));
            for (int i = 0; i < Count; ++i)
            {
                if (!UserPointer(Entry) || Entry == Head || !Visited.insert(Entry).second)
                { ClearNames(); return; }
                int Id = 0;
                uintptr_t Next = 0;
                Core::SDK::Game::NetworkInfo Info;
                if (!Mem.ReadBuffer(Entry + 0x10, &Id, sizeof(Id)) || Id < 0
                    || !ReadName(Entry + 0x18, Info.UserName)
                    || !Mem.ReadBuffer(Entry + 8, &Next, sizeof(Next))) { ClearNames(); return; }
                Names[Id] = std::move(Info);
                Entry = Next;
            }
            uintptr_t ConfirmedHead = 0;
            int ConfirmedCount = 0;
            if (!Mem.ReadBuffer(PlayerListEntry, &ConfirmedHead, sizeof(ConfirmedHead))
                || !Mem.ReadBuffer(PlayerListEntry + 8, &ConfirmedCount, sizeof(ConfirmedCount))
                || Head != ConfirmedHead || Count != ConfirmedCount || Entry != Head
                || g_LastFiveMPid.load() != Pid) { ClearNames(); return; }
            std::lock_guard<std::mutex> lock(m_networkMapMutex);
            NetworkMap.swap(Names);
            SnapshotPid = Pid;
            SnapshotBuild = ModuleBuild;
        }

        void Update()
        {
            while (true)
            {
                try { GetPlayerNamesFromMemory(); }
                catch (const std::exception&)
                {
                    ResetCache();
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

        std::string GetPlayerNameSafe(int Id)
        {
            std::lock_guard<std::mutex> lock(m_networkMapMutex);
            if (!SnapshotPid || SnapshotPid != g_LastFiveMPid.load()
                || SnapshotBuild != g_Offsets.CurrentBuild) return {};
            const auto It = NetworkMap.find(Id);
            return It == NetworkMap.end() ? std::string{} : It->second.UserName;
        }
    };

    inline cUpdateNames g_UpdateNames;
}
