#pragma once
#include <Core/Core.hpp>
#include <chrono>
#include <thread>

namespace Core::Threads
{
    class cAutoDetect
    {
    public:
        void Update()
        {
            auto nextRetry = std::chrono::steady_clock::time_point{};
            DWORD lastCandidatePid = 0;

            while (true)
            {
                try
                {
                    const auto [pid, name] = Core::FindFiveMProcess();
                    uintptr_t moduleBase = 0;
                    uintptr_t moduleSize = 0;
                    if (pid)
                    {
                        const std::string moduleName(name.begin(), name.end());
                        moduleBase = Mem.GetModuleBaseAddr(
                            pid, moduleName.c_str(), &moduleSize);
                    }
                    DWORD exitCode = 0;
                    const bool attached = pid && g_LastFiveMPid.load() == pid
                        && g_Variables.ProcIdFiveM == pid && Mem.ProcId == pid
                        && Mem.ProcHandle && Mem.ModBase && Mem.ModBaseSize
                        && GetProcessId(Mem.ProcHandle) == pid
                        && GetExitCodeProcess(Mem.ProcHandle, &exitCode)
                        && exitCode == STILL_ACTIVE
                        && moduleBase == Mem.ModBase
                        && moduleSize == Mem.ModBaseSize
                        && g_Offsets.m_World && g_Offsets.m_ReplayInterFace
                        && g_Offsets.m_ViewPort;

                    if (pid && !attached)
                    {
                        const auto now = std::chrono::steady_clock::now();
                        if (pid != lastCandidatePid)
                        {
                            lastCandidatePid = pid;
                            nextRetry = {};
                        }

                        // A failed scan leaves g_LastFiveMPid at zero. Retry
                        // periodically without blocking the render thread.
                        if (now >= nextRetry)
                        {
                            Core::GetOffsets(pid);
                            nextRetry = now + std::chrono::seconds(2);
                        }
                    }
                    else if (!pid && (Mem.ProcHandle || g_LastFiveMPid.load()))
                    {
                        std::lock_guard<std::mutex> lock(Core::OffsetAttachMutex);
                        Core::ResetGameAttachment();
                        g_Variables.g_hGameWindow = nullptr;
                        lastCandidatePid = 0;
                        nextRetry = {};
                    }
                    else if (attached)
                    {
                        lastCandidatePid = pid;
                        nextRetry = {};
                    }
                }
                catch (const std::exception&)
                {
                    std::lock_guard<std::mutex> lock(Core::OffsetAttachMutex);
                    Core::ResetGameAttachment();
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    };

    inline cAutoDetect g_AutoDetect;
}
