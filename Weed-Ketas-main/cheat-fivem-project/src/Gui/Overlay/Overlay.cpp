
#pragma once
#include "pch.h"
#include "Overlay.hpp"
#include <Includes/ImGui/Files/imgui_freetype.h>
#include <Includes/CustomWidgets/Notify.hpp>
#include <Includes/ImGui/Fonts.hpp>
#include <Includes/ImGui/Images.hpp>
#include <Core/Core.hpp>
#include <Gui/Gui.hpp>
#include <dwmapi.h>
#include <tchar.h>
#include <thread>
#include <windowsx.h>

#define FULLSCREEN_EX

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

namespace Gui
{

	typedef HWND( WINAPI * CreateWindowInBand )( _In_ DWORD dwExStyle, _In_opt_ ATOM atom, _In_opt_ LPCWSTR lpWindowName, _In_ DWORD dwStyle, _In_ int X, _In_ int Y, _In_ int nWidth, _In_ int nHeight, _In_opt_ HWND hWndParent, _In_opt_ HMENU hMenu, _In_opt_ HINSTANCE hInstance, _In_opt_ LPVOID lpParam, DWORD band );

	inline HHOOK hKeyboardHook;

	static bool IsDetachedRadarHit( POINT CursorScreen )
	{
		if ( !g_Config.ESP
			|| !g_Config.ESP->RadarEnabled
			|| !g_Config.ESP->RadarDetached )
		{
			return false;
		}

		// RadarPosition is stored in viewport coordinates. The overlay can
		// live on another monitor, so convert the global cursor position to
		// the overlay client space before testing the radar rectangle.
		POINT ClientPoint = CursorScreen;
		if ( !ScreenToClient( g_Variables.g_hCheatWindow, &ClientPoint ) )
			return false;

		const ImVec2 RadarPos = g_Config.ESP->RadarPosition;
		const ImVec2 RadarSize( 500.f, 390.f );
		return ClientPoint.x >= RadarPos.x
			&& ClientPoint.y >= RadarPos.y
			&& ClientPoint.x <= RadarPos.x + RadarSize.x
			&& ClientPoint.y <= RadarPos.y + RadarSize.y;
	}

	static void UpdateOverlayInputStyle( bool ForceInteractive = false )
	{
		if ( !IsWindow( g_Variables.g_hCheatWindow ) )
			return;

		// ImGui already handles mouse input for both the menu and the
		// detached radar. Removing WS_EX_TRANSPARENT for the whole overlay is
		// safe here: WndProc forwards unhandled mouse messages to the game
		// window so clicking outside the menu/radar still reaches FiveM.
		const bool Interactive = ForceInteractive
			|| g_MenuInfo.IsOpen
			|| Core::Features::g_Esp.IsRadarWindowHovered()
			|| IsDetachedRadarHit( []() {
				POINT Cursor{};
				GetCursorPos( &Cursor );
				return Cursor;
			}() );

		const LONG_PTR CurrentStyle = GetWindowLongPtr( g_Variables.g_hCheatWindow, GWL_EXSTYLE );
		const bool Transparent = ( CurrentStyle & WS_EX_TRANSPARENT ) != 0;

		if ( Interactive == Transparent )
		{
			LONG_PTR Style = CurrentStyle;
			if ( Interactive )
				Style &= ~static_cast<LONG_PTR>( WS_EX_TRANSPARENT );
			else
				Style |= static_cast<LONG_PTR>( WS_EX_TRANSPARENT );

			SetWindowLongPtr( g_Variables.g_hCheatWindow, GWL_EXSTYLE, Style );
			SetWindowPos( g_Variables.g_hCheatWindow, NULL, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE );
		}
	}

	LRESULT CALLBACK KeyboardCallBack(int nCode, WPARAM wParam, LPARAM lParam)
	{
		// O host injetado (RuntimeBroker dedicado) nao consegue chamar
		// SetForegroundWindow com sucesso: o Windows bloqueia ativacao de uma
		// janela de um processo que nunca recebeu input do usuario. Entao o
		// teclado continua indo para o FiveM e os campos de login do menu nunca
		// veem WM_CHAR. Encaminhamos as teclas para a janela do overlay enquanto
		// o menu estiver aberto e ela NAO estiver em primeiro plano; o
		// TranslateMessage() do loop converte WM_KEYDOWN em WM_CHAR para o ImGui.
		if (nCode == HC_ACTION && g_MenuInfo.IsOpen &&
			IsWindow(g_Variables.g_hCheatWindow) &&
			GetForegroundWindow() != g_Variables.g_hCheatWindow)
		{
			if (wParam == WM_KEYDOWN || wParam == WM_KEYUP ||
				wParam == WM_SYSKEYDOWN || wParam == WM_SYSKEYUP)
			{
				const KBDLLHOOKSTRUCT* Key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
				if (Key)
				{
					// lParam completo: repeat=1, scan code, bit de extended e
					// bit de key-up. Sem isso o TranslateMessage nao gera o
					// WM_CHAR correto (o username/password ficam vazios).
					LPARAM Translated = 1ui64 |
						( static_cast<LPARAM>( Key->scanCode ) << 16 ) |
						( ( Key->flags & LLKHF_EXTENDED ) ? ( 1ui64 << 24 ) : 0 ) |
						( ( Key->flags & LLKHF_UP ) ? ( 1ui64 << 31 ) : 0 );
					PostMessageW( g_Variables.g_hCheatWindow,
						static_cast<UINT>( wParam ),
						static_cast<WPARAM>( Key->vkCode ),
						Translated );
				}
			}
		}

		return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
	}

	void Overlay::Render()
	{
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, Overlay::WndProc, 0L, 0L, GetModuleHandleW(nullptr), NULL, NULL, NULL, NULL, L" ", NULL };
		ATOM RegClass = RegisterClassExW(&wc);
		if (!RegClass && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
		{
			return;
		}

#ifdef FULLSCREEN_EX
		g_Variables.g_hCheatWindow = CreateWindowExW(
			WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
			wc.lpszClassName,
			L"",
			WS_POPUP,
			static_cast<int>(g_Variables.g_vGameWindowPos.x),
			static_cast<int>(g_Variables.g_vGameWindowPos.y),
			static_cast<int>(g_Variables.g_vGameWindowSize.x),
			static_cast<int>(g_Variables.g_vGameWindowSize.y),
			NULL,
			NULL,
			wc.hInstance,
			NULL
		);
#else
		g_Variables.g_hCheatWindow = CreateWindowExW(
			WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
			wc.lpszClassName,
			L"",
			WS_POPUP,
			g_Variables.g_vGameWindowPos.x,
			g_Variables.g_vGameWindowPos.y,
			g_Variables.g_vGameWindowSize.x,
			g_Variables.g_vGameWindowSize.y,
			NULL,
			NULL,
			wc.hInstance,
			NULL
		);
#endif
		if (!g_Variables.g_hCheatWindow)
		{
			UnregisterClassW(wc.lpszClassName, wc.hInstance);
			return;
		}

		SetLayeredWindowAttributes(g_Variables.g_hCheatWindow, RGB(0, 0, 0), 255, LWA_ALPHA);
		MARGINS Margin = { g_Variables.g_vGameWindowPos.x, g_Variables.g_vGameWindowPos.y, g_Variables.g_vGameWindowSize.x, g_Variables.g_vGameWindowSize.y };
		DwmExtendFrameIntoClientArea(g_Variables.g_hCheatWindow, &Margin);

		if (!CreateContext(g_Variables.g_hCheatWindow))
		{
			DestroyContextGL();
			UnregisterClassW(wc.lpszClassName, wc.hInstance);
			return;
		}

		Utils::ApplyStreamMode( g_Variables.g_hCheatWindow, g_Config.General->StreamProof );
		UpdateOverlayInputStyle();

		ShowWindow(g_Variables.g_hCheatWindow, SW_SHOWDEFAULT);
		UpdateWindow(g_Variables.g_hCheatWindow);

		ImGui::CreateContext();

		Style();
		Fonts();

		if (!ImGui_ImplWin32_Init(g_Variables.g_hCheatWindow) ||
			!ImGui_ImplOpenGL3_Init("#version 330"))
		{
			ImGui::DestroyContext();
			DestroyContextGL();
			DestroyWindow(g_Variables.g_hCheatWindow);
			UnregisterClassW(wc.lpszClassName, wc.hInstance);
			return;
		}

#ifdef FULLSCREEN_EX
		// low-level hooks globais (dwThreadId=0) exigem um hMod valido; passar
		// NULL faz SetWindowsHookEx falhar com ERROR_HOOK_NEEDS_HMOD e o overlay
		// nunca recebe as teclas digitadas no host injetado.
		hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardCallBack,
			GetModuleHandleW(nullptr), 0);
#endif // FULLSCREEN_EX

		Core::StartThreads();
		if (!Core::ThreadsStarted)
		{
			return;
		}

		static RECT old_rc;
		ZeroMemory(&Message, sizeof(MSG));

		bool firstFrame = true;
		while (Message.message != WM_QUIT)
		{
			if (PeekMessage(&Message, g_Variables.g_hCheatWindow, 0, 0, PM_REMOVE)) {
				TranslateMessage(&Message);
				DispatchMessage(&Message);
			}

			HWND ActiveWindow = GetForegroundWindow();

			static bool MenuKeyWasDown = false;
			const bool MenuKeyDown = (GetAsyncKeyState(g_Config.General->MenuKey) & 0x8000) != 0;
			const bool MenuKeyPressed = MenuKeyDown && !MenuKeyWasDown;
			MenuKeyWasDown = MenuKeyDown;

			if (MenuKeyPressed)
			{
				if (!g_MenuInfo.IsOpen)
				{
					g_MenuInfo.IsOpen = true;
#ifdef FULLSCREEN_EX
					if (!hKeyboardHook)
						hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardCallBack,
							GetModuleHandleW(nullptr), 0);
#endif
					UpdateOverlayInputStyle( true );
					// Sem ativar a janela o teclado fica no jogo e os campos de
					// login nunca recebem WM_CHAR (nao da para digitar).
					SetForegroundWindow(g_Variables.g_hCheatWindow);
					SetFocus(g_Variables.g_hCheatWindow);
				}
				else
				{
					g_MenuInfo.IsOpen = false;
					UpdateOverlayInputStyle();
					if (g_Variables.g_hGameWindow)
						SetForegroundWindow(g_Variables.g_hGameWindow);
				}
			}

			static float BgAlpha = 0.f;
			BgAlpha = ImLerp(BgAlpha, g_MenuInfo.IsOpen ? 1.f : 0.f, ImGui::GetIO().DeltaTime * 8);

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplWin32_NewFrame();

			ImGui::NewFrame();

			// O radar pode ser arrastado sem abrir o menu: o estado do mouse
			// do ImGui ja foi atualizado por ImGui_ImplWin32_NewFrame(), entao
			// decidimos aqui se o overlay deve capturar clique ou atravessar.
			UpdateOverlayInputStyle();

			{
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, BgAlpha);
				ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), ImVec2(g_Variables.g_vGameWindowSize.x, g_Variables.g_vGameWindowSize.y), ImColor(0.f, 0.f, 0.f, BgAlpha >= 0.4f ? 0.4f : BgAlpha));
				Gui::Rendering();
				ImGui::PopStyleVar();

				// O radar desacoplado fica fora da alpha da interface. Assim
				// ele continua visivel quando o menu principal esta fechado,
				// mas permanece no mesmo overlay fullscreen e pode ser movido
				// pela barra de titulo quando a interface estiver aberta.
				if ( g_MenuInfo.IsLogged )
					Core::Features::g_Esp.RenderDetachedRadar();
			}
			ImGui::EndFrame();

			if (!g_GLSurface.Context)
				break;

			glViewport( 0, 0,
				static_cast<GLsizei>( g_Variables.g_vGameWindowSize.x ),
				static_cast<GLsizei>( g_Variables.g_vGameWindowSize.y ) );
			glClearColor( 0.f, 0.f, 0.f, 0.f );
			glClear( GL_COLOR_BUFFER_BIT );

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );

			GLX::SetVSync( g_Config.General->VSync );
			g_GLSurface.Present();
		}

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		DestroyContextGL();
		DestroyWindow(g_Variables.g_hCheatWindow);
		UnregisterClassW(wc.lpszClassName, wc.hInstance);

#ifdef FULLSCREEN_EX
		UnhookWindowsHookEx(hKeyboardHook);
#endif
		return;
	}
	void Overlay::Fonts( )
	{
		ImGuiIO & io = ImGui::GetIO( ); ( void ) io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
		io.IniFilename = nullptr;

		ImFontConfig Cfg;
		Cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint | ImGuiFreeTypeBuilderFlags_LightHinting | ImGuiFreeTypeBuilderFlags_LoadColor;
		Cfg.FontDataOwnedByAtlas = false;

		ImFontConfig DrawCfg;
		DrawCfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint | ImGuiFreeTypeBuilderFlags_Bitmap;
		DrawCfg.FontDataOwnedByAtlas = false;

		// Explicit UTF-8 text coverage. Portuguese accents live in Latin-1,
		// while Latin Extended keeps future translations from falling back to '?'.
		// Cyrillic is retained for compatibility with existing saved settings.
		static const ImWchar TextGlyphRanges[ ] = {
			0x0020, 0x00FF,
			0x0100, 0x017F,
			0x0400, 0x052F,
			0
		};

		// Typography scaled to the 620x350 canvas: readable body text with a clear hierarchy
		g_Variables.m_FontBig = io.Fonts->AddFontFromMemoryTTF( InterSemiBold, sizeof( InterSemiBold ), 19.f, &Cfg, TextGlyphRanges );
		g_Variables.m_FontBigSmall = io.Fonts->AddFontFromMemoryTTF( InterSemiBold, sizeof( InterSemiBold ), 16.f, &Cfg, TextGlyphRanges );
		g_Variables.m_FontNormal = io.Fonts->AddFontFromMemoryTTF( InterSemiBold, sizeof( InterSemiBold ), 16.f, &Cfg, TextGlyphRanges );
		g_Variables.m_FontSecundary = io.Fonts->AddFontFromMemoryTTF( InterSemiBold, sizeof( InterSemiBold ), 15.f, &Cfg, TextGlyphRanges );
		g_Variables.m_FontSmaller = io.Fonts->AddFontFromMemoryTTF( InterSemiBold, sizeof( InterSemiBold ), 13.f, &Cfg, TextGlyphRanges );

		g_Variables.m_DrawFont = io.Fonts->AddFontFromMemoryTTF( InterSemiBold, sizeof( InterSemiBold ), 12.f, &DrawCfg, TextGlyphRanges );

		// ---- Fontes do ESP ----
		// Chalet London e a fonte do HUD do GTA, mas isso aqui e overlay ImGui
		// em DirectX: desenha por fora do jogo e nao alcanca as fontes dele.
		// Entao ficam as do sistema + a bitmap padrao do ImGui, e o usuario
		// escolhe no menu.
		//
		// Flag Bitmap = sem antialiasing. E o que da o aspecto crispo/
		// serrilhado tipico de ESP; com AA o texto sai macio e nao bate.
		{
			ImFontConfig EspCfg;
			EspCfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint
				| ImGuiFreeTypeBuilderFlags_Bitmap;

			char WinDir[ MAX_PATH ]{};
			GetWindowsDirectoryA( WinDir, MAX_PATH );
			const std::string Fonts = std::string( WinDir ) + xorstr( "\\Fonts\\" );

			// Assados em 16px; o ESP desenha passando o tamanho, entao o
			// slider do menu so muda o tamanho de render.
			const char * Files[ ] = {
				xorstr("tahoma.ttf"), xorstr("tahomabd.ttf"),
				xorstr("verdana.ttf"), xorstr("verdanab.ttf"),
				xorstr("arial.ttf"),   xorstr("arialbd.ttf"),
				xorstr("segoeuib.ttf"),
			};

			// slot 0 = Inter; 1..7 seguem a ordem de Files; 6 = arialbd.ttf
			constexpr int kEspFontArial = 6;
			g_Variables.m_EspFonts[ 0 ] = g_Variables.m_FontSmaller; // Inter (atual)

			for ( int i = 0; i < 7; i++ )
			{
				g_Variables.m_EspFonts[ i + 1 ] = io.Fonts->AddFontFromFileTTF(
					( Fonts + Files[ i ] ).c_str( ), 16.f, &EspCfg,
					TextGlyphRanges );
			}

			// ProggyClean, a bitmap embutida do ImGui
			g_Variables.m_EspFonts[ 8 ] = io.Fonts->AddFontDefault( );

			// Qualquer uma que nao tenha carregado cai na Inter, para o ESP
			// nunca ficar com ponteiro nulo.
			for ( int i = 0; i < g_Variables.kEspFontCount; i++ )
				if ( !g_Variables.m_EspFonts[ i ] )
					g_Variables.m_EspFonts[ i ] = g_Variables.m_FontSmaller;

			// Arial Bold fixa. O seletor de fonte saiu do menu, entao o indice
			// salvo em config antigo e ignorado de proposito -- se fosse lido,
			// quem tivesse escolhido outra fonte antes continuaria com ela.
			g_Variables.m_FontEsp = g_Variables.m_EspFonts[ kEspFontArial ];
		}

		static const ImWchar FontAwesomeRanges[ ] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
		static const ImWchar FontAwesomeRangesBrands[ ] = { ICON_MIN_FAB, ICON_MAX_FAB, 0 };

		ImFontConfig FontAwesomeConfig;
		FontAwesomeConfig.PixelSnapH = true;
		FontAwesomeConfig.GlyphMinAdvanceX = 18;

		g_Variables.FontAwesomeSolid = io.Fonts->AddFontFromMemoryCompressedTTF( FontAwesome6Solid_compressed_data, FontAwesome6Solid_compressed_size, 18, &FontAwesomeConfig, FontAwesomeRanges );

		FontAwesomeConfig.GlyphMinAdvanceX = 17;
		g_Variables.FontAwesomeSolidSmall = io.Fonts->AddFontFromMemoryCompressedTTF( FontAwesome6Solid_compressed_data, FontAwesome6Solid_compressed_size, 17, &FontAwesomeConfig, FontAwesomeRanges );

		FontAwesomeConfig.GlyphMinAdvanceX = 18;
		g_Variables.FontAwesomeRegular = io.Fonts->AddFontFromMemoryCompressedTTF( FontAwesome6Regular_compressed_data, FontAwesome6Regular_compressed_size, 18, &FontAwesomeConfig, FontAwesomeRanges );

		g_Variables.m_Expand = io.Fonts->AddFontFromMemoryTTF( expand_binary, sizeof( expand_binary ), 11.f, &Cfg, TextGlyphRanges );
	}

	void Overlay::Style( )
	{
		ImGui::StyleColorsDark( );

		ImGuiStyle * style = &ImGui::GetStyle( );
		{
			style->WindowPadding = ImVec2( 0, 0 );
			style->WindowBorderSize = 0.f;
			style->ItemSpacing = ImVec2( 8, g_MenuInfo.RowSpacing );
			style->FramePadding = ImVec2( 6, 3 );
			style->ScrollbarSize = 3.f;
			style->ScrollbarRounding = 2.f;
			style->ChildRounding = g_MenuInfo.CardRounding;

			// Thin green scrollbar that lives inside the card instead of covering its border.
			style->Colors[ ImGuiCol_ScrollbarBg ] = ImVec4( 0.f, 0.f, 0.f, 0.f );
			style->Colors[ ImGuiCol_ScrollbarGrab ] = g_Col.BaseDark;
			style->Colors[ ImGuiCol_ScrollbarGrabHovered ] = g_Col.BaseFill;
			style->Colors[ ImGuiCol_ScrollbarGrabActive ] = g_Col.Base;

			// Native ImGui controls use the same dark/green surfaces as the
			// custom controls. In particular, do not leave the default light
			// separator/border visible between list rows.
			style->Colors[ ImGuiCol_WindowBg ] = g_Col.BackgroundCol;
			style->Colors[ ImGuiCol_ChildBg ] = g_Col.ChildCol;
			style->Colors[ ImGuiCol_FrameBg ] = g_Col.InputBackground;
			style->Colors[ ImGuiCol_FrameBgHovered ] = g_Col.HoverCol;
			style->Colors[ ImGuiCol_FrameBgActive ] = g_Col.ActiveItemCol;
			style->Colors[ ImGuiCol_Border ] = ImVec4( g_Col.ChildBorderCol.x, g_Col.ChildBorderCol.y, g_Col.ChildBorderCol.z, 0.35f );
			style->Colors[ ImGuiCol_Separator ] = ImVec4( 0.f, 0.f, 0.f, 0.f );
			style->Colors[ ImGuiCol_SeparatorHovered ] = ImVec4( g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, 0.18f );
			style->Colors[ ImGuiCol_SeparatorActive ] = ImVec4( g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, 0.28f );
			style->Colors[ ImGuiCol_Button ] = g_Col.InputBackground;
			style->Colors[ ImGuiCol_ButtonHovered ] = g_Col.HoverCol;
			style->Colors[ ImGuiCol_ButtonActive ] = g_Col.ActiveItemCol;
			style->Colors[ ImGuiCol_CheckMark ] = g_Col.Base;
			style->Colors[ ImGuiCol_SliderGrab ] = g_Col.BaseFill;
			style->Colors[ ImGuiCol_SliderGrabActive ] = g_Col.Base;
		}

		// The menu mark is drawn procedurally by Custom::DrawBackground.
		// Do not upload the old raster logo texture.
	}

	bool Overlay::CreateContext( HWND hWnd )
	{
		return g_GLSurface.Create( hWnd );
	}

	void Overlay::DestroyContextGL( )
	{
		g_GLSurface.Destroy( );
	}

	LRESULT CALLBACK Overlay::WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
		if ( ImGui_ImplWin32_WndProcHandler( hWnd, msg, wParam, lParam ) )
			return true;

		if (g_MenuInfo.IsOpen)
		{
			switch (msg)
			{
			case WM_KEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_CHAR:
			case WM_SYSCHAR:
			case WM_LBUTTONDOWN:
			case WM_LBUTTONUP:
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
			case WM_MOUSEWHEEL:
			case WM_MOUSEHWHEEL:
				return 0;
			}
		}

		switch ( msg ) {
		case WM_SIZE:
			// O viewport e recalculado a cada frame a partir do tamanho da janela.
			return 0;
		case WM_SYSCOMMAND:
			if ( ( wParam & 0xfff0 ) == SC_KEYMENU ) // Disable ALT application menu
				return 0;
			break;
		case WM_DESTROY:
			::PostQuitMessage( 0 );
			return 0;
			break;
		}

		return DefWindowProc( hWnd, msg, wParam, lParam );
	}

	DWORD Overlay::GetWinLogonToken( DWORD dwSessionId, DWORD dwDesiredAccess, PHANDLE phToken )
	{
		DWORD dwErr;
		PRIVILEGE_SET ps;

		ps.PrivilegeCount = 1;
		ps.Control = PRIVILEGE_SET_ALL_NECESSARY;

		if ( LookupPrivilegeValue( NULL, SE_TCB_NAME, &ps.Privilege[ 0 ].Luid ) ) {
			HANDLE hSnapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
			if ( INVALID_HANDLE_VALUE != hSnapshot ) {
				BOOL bCont, bFound = FALSE;
				PROCESSENTRY32 pe;

				pe.dwSize = sizeof( pe );
				dwErr = ERROR_NOT_FOUND;

				for ( bCont = Process32First( hSnapshot, &pe ); bCont; bCont = Process32Next( hSnapshot, &pe ) ) {
					HANDLE hProcess;

					if ( 0 != _tcsicmp( pe.szExeFile, xorstr( "winlogon.exe" ) ) ) {
						continue;
					}

					hProcess = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID );
					if ( hProcess ) {
						HANDLE hToken;
						DWORD dwRetLen, sid;

						if ( OpenProcessToken( hProcess, TOKEN_QUERY | TOKEN_DUPLICATE, &hToken ) ) {
							BOOL fTcb;

							if ( PrivilegeCheck( hToken, &ps, &fTcb ) && fTcb ) {
								if ( GetTokenInformation( hToken, TokenSessionId, &sid, sizeof( sid ), &dwRetLen ) && sid == dwSessionId ) {
									bFound = TRUE;
									if ( DuplicateTokenEx( hToken, dwDesiredAccess, NULL, SecurityImpersonation, TokenImpersonation, phToken ) ) {
										dwErr = ERROR_SUCCESS;
									}
									else {
										dwErr = GetLastError( );
									}
								}
							}
							CloseHandle( hToken );
						}
						CloseHandle( hProcess );
					}

					if ( bFound ) break;
				}

				CloseHandle( hSnapshot );
			}
			else {
				dwErr = GetLastError( );
			}
		}
		else {
			dwErr = GetLastError( );
		}


		return dwErr;
	}

	DWORD Overlay::CreateUIAccessToken( PHANDLE phToken )
	{
		DWORD dwErr;
		HANDLE hTokenSelf;

		if ( OpenProcessToken( GetCurrentProcess( ), TOKEN_QUERY | TOKEN_DUPLICATE, &hTokenSelf ) ) {
			DWORD dwSessionId, dwRetLen;

			if ( GetTokenInformation( hTokenSelf, TokenSessionId, &dwSessionId, sizeof( dwSessionId ), &dwRetLen ) ) {
				HANDLE hTokenSystem;

				dwErr = GetWinLogonToken( dwSessionId, TOKEN_IMPERSONATE, &hTokenSystem );
				if ( ERROR_SUCCESS == dwErr ) {
					if ( SetThreadToken( NULL, hTokenSystem ) ) {
						if ( DuplicateTokenEx( hTokenSelf, TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_DEFAULT, NULL, SecurityAnonymous, TokenPrimary, phToken ) ) {
							BOOL bUIAccess = TRUE;

							if ( !SetTokenInformation( *phToken, TokenUIAccess, &bUIAccess, sizeof( bUIAccess ) ) ) {
								dwErr = GetLastError( );
								CloseHandle( *phToken );
							}
						}
						else {
							dwErr = GetLastError( );
						}
						RevertToSelf( );
					}
					else {
						dwErr = GetLastError( );
					}
					CloseHandle( hTokenSystem );
				}
			}
			else {
				dwErr = GetLastError( );
			}

			CloseHandle( hTokenSelf );
		}
		else {
			dwErr = GetLastError( );
		}

		return dwErr;
	}

	BOOL Overlay::CheckForUIAccess( DWORD * pdwErr, DWORD * pfUIAccess )
	{
		BOOL result = FALSE;
		HANDLE hToken;

		if ( OpenProcessToken( GetCurrentProcess( ), TOKEN_QUERY, &hToken ) ) {
			DWORD dwRetLen;

			if ( GetTokenInformation( hToken, TokenUIAccess, pfUIAccess, sizeof( *pfUIAccess ), &dwRetLen ) ) {
				result = TRUE;
			}
			else {
				*pdwErr = GetLastError( );
			}
			CloseHandle( hToken );
		}
		else {
			*pdwErr = GetLastError( );
		}

		return result;
	}

	DWORD Overlay::PrepareForUIAccess( )
	{
		DWORD dwErr;
		HANDLE hTokenUIAccess;
		DWORD fUIAccess;

		if ( CheckForUIAccess( &dwErr, &fUIAccess ) ) {
			if ( fUIAccess ) {
				dwErr = ERROR_SUCCESS;
			}
			else {
				dwErr = CreateUIAccessToken( &hTokenUIAccess );
				if ( ERROR_SUCCESS == dwErr ) {
					STARTUPINFO si;
					PROCESS_INFORMATION pi;

					GetStartupInfo( &si );
					if ( CreateProcessAsUser( hTokenUIAccess, NULL, GetCommandLine( ), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ) ) {
						CloseHandle( pi.hProcess ), CloseHandle( pi.hThread );
						ExitProcess( 0 );
					}
					else {
						dwErr = GetLastError( );
					}

					CloseHandle( hTokenUIAccess );
				}
			}
		}

		return dwErr;
	}

	void Overlay::LoadWeaponIcons( ) {
		{
			GLX::CreateTextureFromPNG( FiveM_Weapons::gadget_parachute, sizeof( FiveM_Weapons::gadget_parachute ), g_Variables.FiveM_WeaponsPic.gadget_parachute );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_advancedrifle, sizeof( FiveM_Weapons::weapon_advancedrifle ), g_Variables.FiveM_WeaponsPic.weapon_advancedrifle );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_appistol, sizeof( FiveM_Weapons::weapon_appistol ), g_Variables.FiveM_WeaponsPic.weapon_appistol );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_assaultrifle, sizeof( FiveM_Weapons::weapon_assaultrifle ), g_Variables.FiveM_WeaponsPic.weapon_assaultrifle );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_assaultrifle_mk2, sizeof( FiveM_Weapons::weapon_assaultrifle_mk2 ), g_Variables.FiveM_WeaponsPic.weapon_assaultrifle_mk2 );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_assaultshotgun, sizeof( FiveM_Weapons::weapon_assaultshotgun ), g_Variables.FiveM_WeaponsPic.weapon_assaultshotgun );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_assaultsmg, sizeof( FiveM_Weapons::weapon_assaultsmg ), g_Variables.FiveM_WeaponsPic.weapon_assaultsmg );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_autoshotgun, sizeof( FiveM_Weapons::weapon_autoshotgun ), g_Variables.FiveM_WeaponsPic.weapon_autoshotgun );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_ball, sizeof( FiveM_Weapons::weapon_ball ), g_Variables.FiveM_WeaponsPic.weapon_ball );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_bat, sizeof( FiveM_Weapons::weapon_bat ), g_Variables.FiveM_WeaponsPic.weapon_bat );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_battleaxe, sizeof( FiveM_Weapons::weapon_battleaxe ), g_Variables.FiveM_WeaponsPic.weapon_battleaxe );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_bottle, sizeof( FiveM_Weapons::weapon_bottle ), g_Variables.FiveM_WeaponsPic.weapon_bottle );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_bullpuprifle, sizeof( FiveM_Weapons::weapon_bullpuprifle ), g_Variables.FiveM_WeaponsPic.weapon_bullpuprifle );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_bullpuprifle_mk2, sizeof( FiveM_Weapons::weapon_bullpuprifle_mk2 ), g_Variables.FiveM_WeaponsPic.weapon_bullpuprifle_mk2 );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_bullpupshotgun, sizeof( FiveM_Weapons::weapon_bullpupshotgun ), g_Variables.FiveM_WeaponsPic.weapon_bullpupshotgun );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_bzgas, sizeof( FiveM_Weapons::weapon_bzgas ), g_Variables.FiveM_WeaponsPic.weapon_bzgas );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_carbinerifle, sizeof( FiveM_Weapons::weapon_carbinerifle ), g_Variables.FiveM_WeaponsPic.weapon_carbinerifle );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_carbinerifle_mk2, sizeof( FiveM_Weapons::weapon_carbinerifle_mk2 ), g_Variables.FiveM_WeaponsPic.weapon_carbinerifle_mk2 );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_ceramicpistol, sizeof( FiveM_Weapons::weapon_ceramicpistol ), g_Variables.FiveM_WeaponsPic.weapon_ceramicpistol );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_combatmg, sizeof( FiveM_Weapons::weapon_combatmg ), g_Variables.FiveM_WeaponsPic.weapon_combatmg );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_combatmg_mk2, sizeof( FiveM_Weapons::weapon_combatmg_mk2 ), g_Variables.FiveM_WeaponsPic.weapon_combatmg_mk2 );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_combatpdw, sizeof( FiveM_Weapons::weapon_combatpdw ), g_Variables.FiveM_WeaponsPic.weapon_combatpdw );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_combatpistol, sizeof( FiveM_Weapons::weapon_combatpistol ), g_Variables.FiveM_WeaponsPic.weapon_combatpistol );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_combatshotgun, sizeof( FiveM_Weapons::weapon_combatshotgun ), g_Variables.FiveM_WeaponsPic.weapon_combatshotgun );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_compactlauncher, sizeof( FiveM_Weapons::weapon_compactlauncher ), g_Variables.FiveM_WeaponsPic.weapon_compactlauncher );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_compactrifle, sizeof( FiveM_Weapons::weapon_compactrifle ), g_Variables.FiveM_WeaponsPic.weapon_compactrifle );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_crowbar, sizeof( FiveM_Weapons::weapon_crowbar ), g_Variables.FiveM_WeaponsPic.weapon_crowbar );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_dagger, sizeof( FiveM_Weapons::weapon_dagger ), g_Variables.FiveM_WeaponsPic.weapon_dagger );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_dbshotgun, sizeof( FiveM_Weapons::weapon_dbshotgun ), g_Variables.FiveM_WeaponsPic.weapon_dbshotgun );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_doubleaction, sizeof( FiveM_Weapons::weapon_doubleaction ), g_Variables.FiveM_WeaponsPic.weapon_doubleaction );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_fireextinguisher, sizeof( FiveM_Weapons::weapon_fireextinguisher ), g_Variables.FiveM_WeaponsPic.weapon_fireextinguisher );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_firework, sizeof( FiveM_Weapons::weapon_firework ), g_Variables.FiveM_WeaponsPic.weapon_firework );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_flare, sizeof( FiveM_Weapons::weapon_flare ), g_Variables.FiveM_WeaponsPic.weapon_flare );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_flaregun, sizeof( FiveM_Weapons::weapon_flaregun ), g_Variables.FiveM_WeaponsPic.weapon_flaregun );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_flashlight, sizeof( FiveM_Weapons::weapon_flashlight ), g_Variables.FiveM_WeaponsPic.weapon_flashlight );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_gadgetpistol, sizeof( FiveM_Weapons::weapon_gadgetpistol ), g_Variables.FiveM_WeaponsPic.weapon_gadgetpistol );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_golfclub, sizeof( FiveM_Weapons::weapon_golfclub ), g_Variables.FiveM_WeaponsPic.weapon_golfclub );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_grenade, sizeof( FiveM_Weapons::weapon_grenade ), g_Variables.FiveM_WeaponsPic.weapon_grenade );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_grenadelauncher, sizeof( FiveM_Weapons::weapon_grenadelauncher ), g_Variables.FiveM_WeaponsPic.weapon_grenadelauncher );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_grenadelauncher_smoke, sizeof( FiveM_Weapons::weapon_grenadelauncher_smoke ), g_Variables.FiveM_WeaponsPic.weapon_grenadelauncher_smoke );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_gusenberg, sizeof( FiveM_Weapons::weapon_gusenberg ), g_Variables.FiveM_WeaponsPic.weapon_gusenberg );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_hammer, sizeof( FiveM_Weapons::weapon_hammer ), g_Variables.FiveM_WeaponsPic.weapon_hammer );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_hatchet, sizeof( FiveM_Weapons::weapon_hatchet ), g_Variables.FiveM_WeaponsPic.weapon_hatchet );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_hazardcan, sizeof( FiveM_Weapons::weapon_hazardcan ), g_Variables.FiveM_WeaponsPic.weapon_hazardcan );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_heavypistol, sizeof( FiveM_Weapons::weapon_heavypistol ), g_Variables.FiveM_WeaponsPic.weapon_heavypistol );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_heavyshotgun, sizeof( FiveM_Weapons::weapon_heavyshotgun ), g_Variables.FiveM_WeaponsPic.weapon_heavyshotgun );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_heavysniper, sizeof( FiveM_Weapons::weapon_heavysniper ), g_Variables.FiveM_WeaponsPic.weapon_heavysniper );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_hominglauncher, sizeof( FiveM_Weapons::weapon_hominglauncher ), g_Variables.FiveM_WeaponsPic.weapon_hominglauncher );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_knife, sizeof( FiveM_Weapons::weapon_knife ), g_Variables.FiveM_WeaponsPic.weapon_knife );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_knuckle, sizeof( FiveM_Weapons::weapon_knuckle ), g_Variables.FiveM_WeaponsPic.weapon_knuckle );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_machete, sizeof( FiveM_Weapons::weapon_machete ), g_Variables.FiveM_WeaponsPic.weapon_machete );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_machinepistol, sizeof( FiveM_Weapons::weapon_machinepistol ), g_Variables.FiveM_WeaponsPic.weapon_machinepistol );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_marksmanpistol, sizeof( FiveM_Weapons::weapon_marksmanpistol ), g_Variables.FiveM_WeaponsPic.weapon_marksmanpistol );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_marksmanrifle, sizeof( FiveM_Weapons::weapon_marksmanrifle ), g_Variables.FiveM_WeaponsPic.weapon_marksmanrifle );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_marksmanrifle_mk2, sizeof( FiveM_Weapons::weapon_marksmanrifle_mk2 ), g_Variables.FiveM_WeaponsPic.weapon_marksmanrifle_mk2 );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_mg, sizeof( FiveM_Weapons::weapon_mg ), g_Variables.FiveM_WeaponsPic.weapon_mg );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_microsmg, sizeof( FiveM_Weapons::weapon_microsmg ), g_Variables.FiveM_WeaponsPic.weapon_microsmg );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_militaryrifle, sizeof( FiveM_Weapons::weapon_militaryrifle ), g_Variables.FiveM_WeaponsPic.weapon_militaryrifle );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_minigun, sizeof( FiveM_Weapons::weapon_minigun ), g_Variables.FiveM_WeaponsPic.weapon_minigun );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_minismg, sizeof( FiveM_Weapons::weapon_minismg ), g_Variables.FiveM_WeaponsPic.weapon_minismg );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_molotov, sizeof( FiveM_Weapons::weapon_molotov ), g_Variables.FiveM_WeaponsPic.weapon_molotov );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_musket, sizeof( FiveM_Weapons::weapon_musket ), g_Variables.FiveM_WeaponsPic.weapon_musket );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_navyrevolver, sizeof( FiveM_Weapons::weapon_navyrevolver ), g_Variables.FiveM_WeaponsPic.weapon_navyrevolver );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_nightstick, sizeof( FiveM_Weapons::weapon_nightstick ), g_Variables.FiveM_WeaponsPic.weapon_nightstick );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_petrolcan, sizeof( FiveM_Weapons::weapon_petrolcan ), g_Variables.FiveM_WeaponsPic.weapon_petrolcan );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_pipebomb, sizeof( FiveM_Weapons::weapon_pipebomb ), g_Variables.FiveM_WeaponsPic.weapon_pipebomb );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_pistol, sizeof( FiveM_Weapons::weapon_pistol ), g_Variables.FiveM_WeaponsPic.weapon_pistol );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_pistol50, sizeof( FiveM_Weapons::weapon_pistol50 ), g_Variables.FiveM_WeaponsPic.weapon_pistol50 );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_pistol_mk2, sizeof( FiveM_Weapons::weapon_pistol_mk2 ), g_Variables.FiveM_WeaponsPic.weapon_pistol_mk2 );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_poolcue, sizeof( FiveM_Weapons::weapon_poolcue ), g_Variables.FiveM_WeaponsPic.weapon_poolcue );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_proxmine, sizeof( FiveM_Weapons::weapon_proxmine ), g_Variables.FiveM_WeaponsPic.weapon_proxmine );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_pumpshotgun, sizeof( FiveM_Weapons::weapon_pumpshotgun ), g_Variables.FiveM_WeaponsPic.weapon_pumpshotgun );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_pumpshotgun_mk2, sizeof( FiveM_Weapons::weapon_pumpshotgun_mk2 ), g_Variables.FiveM_WeaponsPic.weapon_pumpshotgun_mk2 );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_railgun, sizeof( FiveM_Weapons::weapon_railgun ), g_Variables.FiveM_WeaponsPic.weapon_railgun );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_raycarbine, sizeof( FiveM_Weapons::weapon_raycarbine ), g_Variables.FiveM_WeaponsPic.weapon_raycarbine );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_rayminigun, sizeof( FiveM_Weapons::weapon_rayminigun ), g_Variables.FiveM_WeaponsPic.weapon_rayminigun );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_raypistol, sizeof( FiveM_Weapons::weapon_raypistol ), g_Variables.FiveM_WeaponsPic.weapon_raypistol );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_revolver, sizeof( FiveM_Weapons::weapon_revolver ), g_Variables.FiveM_WeaponsPic.weapon_revolver );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_revolver_mk2, sizeof( FiveM_Weapons::weapon_revolver_mk2 ), g_Variables.FiveM_WeaponsPic.weapon_revolver_mk2 );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_rpg, sizeof( FiveM_Weapons::weapon_rpg ), g_Variables.FiveM_WeaponsPic.weapon_rpg );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_sawnoffshotgun, sizeof( FiveM_Weapons::weapon_sawnoffshotgun ), g_Variables.FiveM_WeaponsPic.weapon_sawnoffshotgun );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_smg, sizeof( FiveM_Weapons::weapon_smg ), g_Variables.FiveM_WeaponsPic.weapon_smg );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_smg_mk2, sizeof( FiveM_Weapons::weapon_smg_mk2 ), g_Variables.FiveM_WeaponsPic.weapon_smg_mk2 );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_smokegrenade, sizeof( FiveM_Weapons::weapon_smokegrenade ), g_Variables.FiveM_WeaponsPic.weapon_smokegrenade );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_sniperrifle, sizeof( FiveM_Weapons::weapon_sniperrifle ), g_Variables.FiveM_WeaponsPic.weapon_sniperrifle );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_snowball, sizeof( FiveM_Weapons::weapon_snowball ), g_Variables.FiveM_WeaponsPic.weapon_snowball );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_snspistol, sizeof( FiveM_Weapons::weapon_snspistol ), g_Variables.FiveM_WeaponsPic.weapon_snspistol );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_snspistol_mk2, sizeof( FiveM_Weapons::weapon_snspistol_mk2 ), g_Variables.FiveM_WeaponsPic.weapon_snspistol_mk2 );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_specialcarbine, sizeof( FiveM_Weapons::weapon_specialcarbine ), g_Variables.FiveM_WeaponsPic.weapon_specialcarbine );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_specialcarbine_mk2, sizeof( FiveM_Weapons::weapon_specialcarbine_mk2 ), g_Variables.FiveM_WeaponsPic.weapon_specialcarbine_mk2 );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_stickybomb, sizeof( FiveM_Weapons::weapon_stickybomb ), g_Variables.FiveM_WeaponsPic.weapon_stickybomb );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_stone_hatchet, sizeof( FiveM_Weapons::weapon_stone_hatchet ), g_Variables.FiveM_WeaponsPic.weapon_stone_hatchet );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_stungun, sizeof( FiveM_Weapons::weapon_stungun ), g_Variables.FiveM_WeaponsPic.weapon_stungun );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_switchblade, sizeof( FiveM_Weapons::weapon_switchblade ), g_Variables.FiveM_WeaponsPic.weapon_switchblade );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_unarmed, sizeof( FiveM_Weapons::weapon_unarmed ), g_Variables.FiveM_WeaponsPic.weapon_unarmed );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_vintagepistol, sizeof( FiveM_Weapons::weapon_vintagepistol ), g_Variables.FiveM_WeaponsPic.weapon_vintagepistol );
			GLX::CreateTextureFromPNG( FiveM_Weapons::weapon_wrench, sizeof( FiveM_Weapons::weapon_wrench ), g_Variables.FiveM_WeaponsPic.weapon_wrench );
		}
	}
}
