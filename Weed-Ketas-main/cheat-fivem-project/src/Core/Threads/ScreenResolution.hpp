#pragma once
#include <Includes/Includes.hpp>
#include <Includes/Utils.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <windows.h>
#include <thread>
#include <vector>

#include <Includes/CustomWidgets/Notify.hpp>

namespace Core
{
    namespace Threads
    {
        class cScreenResolution
        {
        public:
            // -1 means that Settings has not selected a monitor yet. The
            // first UI render then chooses the actual primary-monitor index
            // instead of assuming EnumDisplayMonitors() returns it at 0.
            inline static int g_SelectedMonitor = -1;

            auto _stdcall MoveAndSetResolution() -> void
            {
                g_Variables.g_hGameWindow = FindWindowA(xorstr("grcWindow"), nullptr);
                if (!g_Variables.g_hGameWindow)
                    return;

                auto WindowInfo = Utils::GetWindowPosAndSize(g_Variables.g_hGameWindow);
                g_Variables.g_vGameWindowSize = WindowInfo.second;
                g_Variables.g_vGameWindowPos = WindowInfo.first;
                g_Variables.g_vGameWindowCenter = {
                    g_Variables.g_vGameWindowSize.x / 2,
                    g_Variables.g_vGameWindowSize.y / 2
                };

                // The primary monitor remains the default. The selected
                // monitor is only used after the user explicitly enables it
                // in Settings > Configuration.
                const int TargetMonitor = g_Config.General->SecondMonitor
                    ? g_SelectedMonitor
                    : GetPrimaryMonitorIndex();

                ImVec2 mappedPos;
                Threads::cScreenResolution::MapPrimaryToSelected(
                    g_Variables.g_vGameWindowPos,
                    mappedPos,
                    TargetMonitor
                );
                g_Variables.g_vGameWindowPos = mappedPos;

                MoveWindow(
                    g_Variables.g_hCheatWindow,
                    g_Variables.g_vGameWindowPos.x,
                    g_Variables.g_vGameWindowPos.y,
                    g_Variables.g_vGameWindowSize.x,
                    g_Variables.g_vGameWindowSize.y - 1,
                    true
                );
            }


            auto _stdcall Update() -> void
            {
                static int monitor_check_counter = 0;

                while (true)
                {
                    try
                    {
                        if (monitor_check_counter % 34 == 0)
                        {
                            auto monitors = GetAllMonitors();

                            if (monitors.size() <= 1 && g_Config.General->SecondMonitor)
                                g_Config.General->SecondMonitor = false;
                        }

                        monitor_check_counter++;
                        MoveAndSetResolution();
                    }
                    catch (...)
                    {
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
            }

            static std::vector<HMONITOR> GetAllMonitors()
            {
                std::vector<HMONITOR> monitors;

                EnumDisplayMonitors(
                    NULL,
                    NULL,
                    [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL
                    {
                        reinterpret_cast<std::vector<HMONITOR>*>(lParam)->push_back(hMonitor);
                        return TRUE;
                    },
                    reinterpret_cast<LPARAM>(&monitors)
                );

                return monitors;
            }

            static std::pair<POINT, POINT> GetMonitorDimensions(HMONITOR hMonitor)
            {
                MONITORINFO mi = { sizeof(MONITORINFO) };
                GetMonitorInfo(hMonitor, &mi);

                return {
                    { mi.rcMonitor.left,  mi.rcMonitor.top },
                    { mi.rcMonitor.right - mi.rcMonitor.left,
                      mi.rcMonitor.bottom - mi.rcMonitor.top }
                };
            }

            static int GetPrimaryMonitorIndex()
            {
                auto monitors = GetAllMonitors();

                for (size_t i = 0; i < monitors.size(); ++i)
                {
                    MONITORINFO mi = { sizeof(MONITORINFO) };
                    if (GetMonitorInfo(monitors[i], &mi))
                    {
                        if (mi.dwFlags & MONITORINFOF_PRIMARY)
                            return static_cast<int>(i);
                    }
                }

                return 0;
            }

            static std::vector<std::string> ListMonitors()
            {
                std::vector<std::string> list;
                auto monitors = GetAllMonitors();

                for (size_t i = 0; i < monitors.size(); ++i)
                {
                    list.push_back(
                        std::string(xorstr("Monitor ")) + std::to_string(i + 1)
                    );
                }

                return list;
            }

            static auto _stdcall MapPrimaryToSelected(
                ImVec2& inputPos,
                ImVec2& mappedPos,
                int selectedMonitorIndex
            ) -> void
            {
                auto monitors = GetAllMonitors();

                if (monitors.size() <= 1)
                {
                    mappedPos = inputPos;
                    return;
                }

                int primaryIndex = GetPrimaryMonitorIndex();

                if (selectedMonitorIndex < 0 ||
                    selectedMonitorIndex >= static_cast<int>(monitors.size()))
                {
                    selectedMonitorIndex = primaryIndex;
                }

                if (selectedMonitorIndex == primaryIndex)
                {
                    mappedPos = inputPos;
                    return;
                }

                auto [primaryPos, primarySize] =
                    GetMonitorDimensions(monitors[primaryIndex]);

                auto [targetPos, targetSize] =
                    GetMonitorDimensions(monitors[selectedMonitorIndex]);

                float xRatio = static_cast<float>(targetSize.x) / primarySize.x;
                float yRatio = static_cast<float>(targetSize.y) / primarySize.y;

                mappedPos.x = inputPos.x * xRatio + targetPos.x;
                mappedPos.y = inputPos.y * yRatio + targetPos.y;
            }
        };

        inline cScreenResolution g_ScreenResolution;
    }
}
