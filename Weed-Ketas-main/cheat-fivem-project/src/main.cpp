#include "pch.h"

#include <Windows.h>
#include <chrono>
#include <thread>
#include <Gui/Overlay/Overlay.hpp>
#include <Gui/gui.hpp>
#include <Includes/Includes.hpp>
#include <Includes/Utils.hpp>
#include <Core/Core.hpp>
#include <Core/SDK/Memory.hpp>
#include <Core/SDK/SDK.hpp>
#include <Security/AntiCrack.hpp>

using namespace Core;

int APIENTRY WinMain(HINSTANCE, HINSTANCE, PSTR, int)
{
	if (!Mem.GetMaxPrivileges(GetCurrentProcess()))
	{
		return 0;
	}

	while (!g_Variables.g_hGameWindow)
	{
		g_Variables.g_hGameWindow = FindWindowA(xorstr("grcWindow"), nullptr);
		if (g_Variables.g_hGameWindow)
		{
			const auto WindowInfo = Utils::GetWindowPosAndSize(g_Variables.g_hGameWindow);
			g_Variables.g_vGameWindowSize = WindowInfo.second;
			g_Variables.g_vGameWindowPos = WindowInfo.first;
			g_Variables.g_vGameWindowCenter = {
				g_Variables.g_vGameWindowSize.x / 2,
				g_Variables.g_vGameWindowSize.y / 2
			};
			break;
		}
		std::this_thread::sleep_for(std::chrono::seconds(2));
	}

	GetWindowThreadProcessId(g_Variables.g_hGameWindow, &g_Variables.ProcIdFiveM);
	GetOffsets();

	// WinMain roda na thread injetada pelo Donut (CreateThread bruto). O
	// overlay precisa de uma thread com o CRT inicializado para os estados
	// estaticos do ImGui nao ficarem com ponteiros nulos dentro do Brave.
	std::thread OverlayThread( [&]()
	{
		Gui::cOverlay.Render();
	} );
	OverlayThread.join();
	return 0;
}
