#pragma once
#include <Includes/Includes.hpp>
#include <Includes/Utils.hpp>
#include <Core/Offsets.hpp>
#include <Core/Core.hpp>
#include <chrono>
#include <thread>

namespace Core
{
    namespace Threads
    {
        class cUpdatePtrs
        {
        public:
            void Update()
            {
                auto nextPointerRefresh =
                    std::chrono::steady_clock::time_point{};

                while (true)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    std::unique_lock<std::mutex> attachmentLock(
                        Core::OffsetAttachMutex, std::try_to_lock);
                    if (!attachmentLock.owns_lock())
                        continue;

                    const auto now = std::chrono::steady_clock::now();
                    if (!g_LastFiveMPid.load())
                    {
                        Core::ClearGamePointers();
                        nextPointerRefresh = std::chrono::steady_clock::time_point{};
                        continue;
                    }


                    if (now >= nextPointerRefresh)
                    {
                        auto* world = Mem.Read<CPedFactory*>(g_Offsets.m_World);
                        Core::SDK::Pointers::pWorld = world;
                        Core::SDK::Pointers::pLocalPlayer =
                            world ? world->GetLocalPlayer() : nullptr;
                        Core::SDK::Pointers::pReplayInterFace =
                            Mem.Read<CReplayInterFace*>(
                                g_Offsets.m_ReplayInterFace);
                        Core::SDK::Pointers::pViewPort =
                            Mem.Read<uintptr_t>(g_Offsets.m_ViewPort);
                        Core::SDK::Pointers::pCamGamePlayDirector =
                            Mem.Read<uintptr_t>(
                                g_Offsets.m_CamGameplayDirector);

                        nextPointerRefresh =
                            now + std::chrono::milliseconds(250);
                    }

                    // Keep weapon-defense multipliers applied while enabled, including after respawn.
                    auto* localPed = Core::SDK::Pointers::pLocalPlayer;
                    if (g_Config.Player->EnableGodMode && localPed)
                        localPed->SetGodMode(true);
                }
            }
        };

        inline cUpdatePtrs g_UpdatePtrs;
    }
}
