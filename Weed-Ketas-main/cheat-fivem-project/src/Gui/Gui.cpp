#include "pch.h"
#include "Gui.hpp"

#include <thread>
#include <atomic>
#include <mutex>

#include <Includes/CustomWidgets/Custom.hpp>
#include <Includes/CustomWidgets/Notify.hpp>
#include <Includes/CustomWidgets/WaterMarks.hpp>

#include <Gui/Pages/Combat.hpp>
#include <Gui/Pages/Exploits.hpp>
#include <Gui/Pages/Local.hpp>
#include <Gui/Pages/Settings.hpp>
#include <Gui/Pages/Visuals.hpp>
#include <Gui/Pages/World.hpp>

#include <Core/Features/Esp/Esp.hpp>
#include <Core/Features/Exploits/Exploits.hpp>
#include <Core/Threads/WebSocket.hpp>
#include <Includes/CustomWidgets/Notify.hpp>

#include <Security/Api/api.hpp>

int PauseLoop;
inline std::mutex DrawMtx;

// Variáveis globais para autenticação
namespace AuthGlobals {
	static std::atomic<bool> authInProgress{false};
	static std::string authError;
	static std::mutex authMutex;
}

void Gui::Rendering()
{
	ImGuiIO &io = ImGui::GetIO();
	/*if (g_MenuInfo.IsLogged && !g_Api.IsAuthenticated()) {
		g_MenuInfo.IsLogged = false;
		g_Api.Logout();
	}*/

	if (!g_MenuInfo.IsLogged)
	{
		g_MenuInfo.MenuSize = {420, 280};
	}
	else
	{
		// Preserva o canvas 700x400 quando ha espaco e deriva a grade do
		// viewport em resolucoes menores. Como cards, sidebar e area de conteudo
		// usam as mesmas metricas, as colunas reduzem sem se sobrepor.
		const float AvailableW = io.DisplaySize.x > 0.f ? io.DisplaySize.x - 24.f : 700.f;
		const float AvailableH = io.DisplaySize.y > 0.f ? io.DisplaySize.y - 24.f : 400.f;
		g_MenuInfo.MenuSize.x = ImClamp( AvailableW, 360.f, 700.f );
		g_MenuInfo.MenuSize.y = ImClamp( AvailableH, 280.f, 400.f );

		const bool Compact = g_MenuInfo.MenuSize.x < 620.f;
		g_MenuInfo.SideBarWidth = Compact ? 148.f : 192.f;
		g_MenuInfo.SideBarPadding = Compact ? 8.f : 10.f;
		g_MenuInfo.TabWidth = g_MenuInfo.SideBarWidth - g_MenuInfo.SideBarPadding * 2.f;
		g_MenuInfo.CardX = g_MenuInfo.SideBarWidth + 8.f;
		g_MenuInfo.CardGap = Compact ? 6.f : 7.f;

		const float ContentW = ImMax( 190.f,
			g_MenuInfo.MenuSize.x - g_MenuInfo.CardX - 9.f );
		g_MenuInfo.CardWidth = ImMax( 92.f,
			( ContentW - g_MenuInfo.CardGap ) * 0.5f );
		g_MenuInfo.CardHeight = ImMax( 220.f,
			g_MenuInfo.MenuSize.y - g_MenuInfo.CardY - 8.f );
	}
	ImGui::SetNextWindowSize(g_MenuInfo.MenuSize);

	// Centraliza apenas na primeira vez. Com ImGuiCond_Always o ImGui
	// reposicionava a janela todo frame e o arraste era desfeito na hora.
	// io.IniFilename e nullptr, entao a posicao vale pela sessao e na
	// proxima execucao a janela volta centralizada.
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

	ImGui::Begin(xorstr(" "), nullptr, ImGuiWindowFlags);
	ImGui::PushFont(g_Variables.m_FontNormal);
	{
		Custom::DrawBackground(g_MenuInfo.IsLogged);
		if (g_MenuInfo.IsLogged)
		{
			// Sidebar navigation: tight vertical rhythm, a small breather after each expanded subtab list
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, g_MenuInfo.TabSpacing));
			ImGui::SetCursorPos(ImVec2(g_MenuInfo.SideBarPadding, g_MenuInfo.NavStartY));

			ImGui::BeginGroup();
			ImGui::PushFont(g_Variables.m_FontSecundary);
			{
				if (Custom::MainTab(ICON_FA_CROSSHAIRS, Utils::Translate(xorstr("Combat")), g_MenuInfo.Combat == g_MenuInfo.iTabCount))
				{
					g_MenuInfo.iTabCount = g_MenuInfo.Combat;
				}
				if (g_MenuInfo.iTabCount == g_MenuInfo.Combat)
				{
					if (Custom::SubTabItem(Utils::Translate(xorstr("Aimbot")), 0 == Combat::iSubTabCount))
						Combat::iSubTabCount = 0;
					if (Custom::SubTabItem(Utils::Translate(xorstr("TriggerBot")), 1 == Combat::iSubTabCount))
						Combat::iSubTabCount = 1;
					if (Custom::SubTabItem(Utils::Translate(xorstr("SilentAim")), 2 == Combat::iSubTabCount))
						Combat::iSubTabCount = 2;
					ImGui::Dummy(ImVec2(0.f, 2.f));
				}

				if (Custom::MainTab(ICON_FA_EYE, Utils::Translate(xorstr("Visuals")), g_MenuInfo.Visuals == g_MenuInfo.iTabCount))
				{
					g_MenuInfo.iTabCount = g_MenuInfo.Visuals;
				}
				if (g_MenuInfo.iTabCount == g_MenuInfo.Visuals)
				{
					if (Custom::SubTabItem(Utils::Translate(xorstr("Players")), 0 == Visuals::iSubTabCount))
					{
						Visuals::iSubTabCount = 0;
					}
					if (Custom::SubTabItem(Utils::Translate(xorstr("Vehicles")), 1 == Visuals::iSubTabCount))
					{
						Visuals::iSubTabCount = 1;
					}
					if (Custom::SubTabItem(Utils::Translate(xorstr("Radar")), 2 == Visuals::iSubTabCount))
					{
						Visuals::iSubTabCount = 2;
					}
					ImGui::Dummy(ImVec2(0.f, 2.f));
				}

				if (Custom::MainTab(ICON_FA_USER, Utils::Translate(xorstr("Local")), g_MenuInfo.Local == g_MenuInfo.iTabCount))
				{
					g_MenuInfo.iTabCount = g_MenuInfo.Local;
				}
				if (g_MenuInfo.iTabCount == g_MenuInfo.Local)
				{
					if (Custom::SubTabItem(Utils::Translate(xorstr("Main")), 0 == Local::iSubTabCount))
					{
						Local::iSubTabCount = 0;
					}
					if (Custom::SubTabItem(Utils::Translate(xorstr("Vehicles")), 1 == Local::iSubTabCount))
					{
						Local::iSubTabCount = 1;
					}
					if (Custom::SubTabItem(Utils::Translate(xorstr("Teleports")), 2 == Local::iSubTabCount))
					{
						Local::iSubTabCount = 2;
					}
					if (Custom::SubTabItem(Utils::Translate(xorstr("Weapons")), 3 == Local::iSubTabCount))
					{
						Local::iSubTabCount = 3;
					}
					ImGui::Dummy(ImVec2(0.f, 2.f));
				}

				if (Custom::MainTab(ICON_FA_GLOBE, Utils::Translate(xorstr("World")), g_MenuInfo.World == g_MenuInfo.iTabCount))
				{
					g_MenuInfo.iTabCount = g_MenuInfo.World;
				}
				if (g_MenuInfo.iTabCount == g_MenuInfo.World)
				{
					if (Custom::SubTabItem(Utils::Translate(xorstr("Players")), 0 == World::iSubTabCount))
					{
						World::iSubTabCount = 0;
					}
					if (Custom::SubTabItem(Utils::Translate(xorstr("Vehicles")), 1 == World::iSubTabCount))
					{
						World::iSubTabCount = 1;
					}
					ImGui::Dummy(ImVec2(0.f, 2.f));
				}

				if (Custom::MainTab(ICON_FA_CODE, Utils::Translate(xorstr("Scripts")), g_MenuInfo.Exploits == g_MenuInfo.iTabCount))
				{
					g_MenuInfo.iTabCount = g_MenuInfo.Exploits;
				}
				if (g_MenuInfo.iTabCount == g_MenuInfo.Exploits)
				{
					if (Custom::SubTabItem(Utils::Translate(xorstr("Weapon Spawn")), 0 == ExploitsGui::iSubTabCount))
						ExploitsGui::iSubTabCount = 0;
					if (Custom::SubTabItem(Utils::Translate(xorstr("Resources")), 1 == ExploitsGui::iSubTabCount))
						ExploitsGui::iSubTabCount = 1;
					ImGui::Dummy(ImVec2(0.f, 2.f));
				}

				if (Custom::MainTab(ICON_FA_GEAR, Utils::Translate(xorstr("Settings")), g_MenuInfo.Settings == g_MenuInfo.iTabCount))
				{
					g_MenuInfo.iTabCount = g_MenuInfo.Settings;
				}
				if (g_MenuInfo.iTabCount == g_MenuInfo.Settings)
				{
					if (Custom::SubTabItem(Utils::Translate(xorstr("Main")), 0 == Settings::iSubTabCount))
					{
						Settings::iSubTabCount = 0;
					}
					if (Custom::SubTabItem(Utils::Translate(xorstr("Configuration")), 1 == Settings::iSubTabCount))
					{
						Settings::iSubTabCount = 1;
					}
					if (Custom::SubTabItem(Utils::Translate(xorstr("Colors")), 2 == Settings::iSubTabCount))
					{
						Settings::iSubTabCount = 2;
					}
					ImGui::Dummy(ImVec2(0.f, 2.f));
				}
			}
			ImGui::PopFont();
			ImGui::EndGroup();

			ImGui::PopStyleVar();
		}

		// Eased page transition: fade out, swap, fade + slide back in
		const float PageEased =
			Custom::PageTransition(g_MenuInfo.TabAlpha, g_MenuInfo.iCurrentPage, g_MenuInfo.iTabCount);

		ImGuiStyle *style = &ImGui::GetStyle();
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, PageEased * style->Alpha);

		if (g_MenuInfo.IsLogged)
		{
			// Page fade slide is forwarded to the cards, which position themselves absolutely
			g_MenuInfo.TabAdd = (1.f - PageEased) * g_MenuInfo.TransitionSlide;

			ImGui::SetCursorPos(ImVec2(g_MenuInfo.CardX, g_MenuInfo.CardY + g_MenuInfo.TabAdd));
			ImGui::BeginGroup();

			switch (g_MenuInfo.iCurrentPage)
			{

			case g_MenuInfo.Combat:
				Combat::Render();
				break;
			case g_MenuInfo.Visuals:
				Visuals::Render();
				break;
			case g_MenuInfo.Local:
				Local::Render();
				break;
			case g_MenuInfo.World:
				World::Render();
				break;
			case g_MenuInfo.Exploits:
				ExploitsGui::Render();
				break;
			case g_MenuInfo.Settings:
				Settings::Render();
				break;
			}

			ImGui::EndGroup();

			HWND ActiveWindow = GetForegroundWindow();

			{
				std::lock_guard<std::mutex> Lock(DrawMtx);

				NotifyManager::Render();

				if (ActiveWindow == g_Variables.g_hGameWindow)
				{
					if (GetAsyncKeyState(g_Config.Player->GodModeKey) & 1)
					{
						g_Config.Player->EnableGodMode = !g_Config.Player->EnableGodMode;

						if (Core::SDK::Pointers::pLocalPlayer)
							Core::SDK::Pointers::pLocalPlayer->SetGodMode(g_Config.Player->EnableGodMode);

						std::thread(
							[&]()
							{
								NotifyManager::Send(xorstr("GodMode has been ") +
														(std::string)(g_Config.Player->EnableGodMode
																		  ? xorstr("enabled!")
																		  : xorstr("disabled!")),
													2000);
							})
							.detach();
					}

					if (GetAsyncKeyState(g_Config.Player->BlockAimingKey) & 1)
					{
						g_Config.Player->BlockAiming = !g_Config.Player->BlockAiming;

						if (Core::SDK::Pointers::pLocalPlayer)
							Core::SDK::Pointers::pLocalPlayer->SetBlockAimming(g_Config.Player->BlockAiming);

						std::thread(
							[&]()
							{
								NotifyManager::Send(xorstr("BlockAim has been ") +
														(std::string)(g_Config.Player->BlockAiming
																		  ? xorstr("enabled!")
																		  : xorstr("disabled!")),
													2000);
							})
							.detach();
					}

					if (GetAsyncKeyState(g_Config.Player->KeyBindFix) & 1)
					{

						if (Core::SDK::Pointers::pLocalPlayer)
						{
							auto CurrentVehicle = Core::SDK::Pointers::pLocalPlayer->GetLastVehicle();
							if (CurrentVehicle)
								CurrentVehicle->Fix(Core::SDK::Pointers::pLocalPlayer);
						}

						std::thread([&]() { NotifyManager::Send(xorstr("Vehicle has been repaired!"), 2000); })
							.detach();
					}

					if (GetAsyncKeyState(g_Config.Player->KeyBindUnlock) & 1)
					{

						auto CurrentVehicle = Core::SDK::Game::GetClosestVehicle(50);
						if (CurrentVehicle)
							CurrentVehicle->DoorState(eVehicleLockState::VEHICLELOCK_UNLOCKED);

						std::thread([&]() { NotifyManager::Send(xorstr("Vehicle has been unlocked!"), 2000); })
							.detach();
					}

					if (GetAsyncKeyState(g_Config.Player->NoClipKey) & 1)
					{
						g_Config.Player->NoClipEnabled = !g_Config.Player->NoClipEnabled;

						if (Core::SDK::Pointers::pLocalPlayer)
							Core::SDK::Pointers::pLocalPlayer->NoRagDoll(g_Config.Player->NoClipEnabled);

						std::thread(
							[&]()
							{
								NotifyManager::Send(xorstr("NoClip has been ") +
														(std::string)(g_Config.Player->NoClipEnabled
																		  ? xorstr("enabled!")
																		  : xorstr("disabled!")),
													2000);
							})
							.detach();
					}

					// Bind do ESP (wallhack). Sem este bloco a tecla escolhida
					// no "Enable Visuals" nao fazia nada, porque o KeyBind so
					// era exibido na GUI e nunca lido no loop.
					if (g_Config.ESP->KeyBind && (GetAsyncKeyState(g_Config.ESP->KeyBind) & 1))
					{
						g_Config.ESP->Enabled = !g_Config.ESP->Enabled;

						std::thread(
							[&]()
							{
								NotifyManager::Send(xorstr("Wallhack has been ") +
														(std::string)(g_Config.ESP->Enabled
																		  ? xorstr("enabled!")
																		  : xorstr("disabled!")),
													2000);
							})
							.detach();
					}

				}

				if (ActiveWindow == g_Variables.g_hGameWindow || ActiveWindow == g_Variables.g_hCheatWindow)
				{
					struct FovFuncs_t
					{
						bool *Enabled;
						float *FovSize;
						ImVec4 FovColor;
					};

					std::vector<FovFuncs_t> FovDrawList = {
						FovFuncs_t(&g_Config.Aimbot->ShowFov, &g_Config.Aimbot->FOV, g_Config.Aimbot->FovColor),
						FovFuncs_t(&g_Config.SilentAim->ShowFov, &g_Config.SilentAim->FOV,
								   g_Config.SilentAim->FovColor),
						FovFuncs_t(&g_Config.TriggerBot->ShowFov, &g_Config.TriggerBot->FOV,
								   g_Config.TriggerBot->FovColor),
					};

					static std::vector<float> Alphas(FovDrawList.size(), 0.0f);
					static std::vector<float> Sizes(FovDrawList.size(), 0.0f);

					for (int i = 0; i < FovDrawList.size(); ++i)
					{
						auto &Fov = FovDrawList[i];

						Alphas[i] = ImClamp(
							ImLerp(Alphas[i], *Fov.Enabled ? 1.f : 0.f, ImGui::GetIO().DeltaTime * 10.f), 0.f, 1.f);
						Sizes[i] = ImLerp(Sizes[i], FovRadius(*Fov.FovSize), ImGui::GetIO().DeltaTime * 12.f);

						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, Alphas[i]);

						ImGui::GetBackgroundDrawList()->AddCircle(
							ImVec2(g_Variables.g_vGameWindowCenter.x, g_Variables.g_vGameWindowCenter.y), Sizes[i],
							ImGui::GetColorU32(Fov.FovColor), 999);

						ImGui::PopStyleVar();
					}

					ImGui::PushFont(g_Variables.m_DrawFont);

					Features::g_Esp.Draw();
					Features::g_Esp.DrawVehicle();

					ImGui::PopFont();
				}
				else
				{

					if (ImGui::GetStyle().Alpha >= 0.9f)
					{
						g_MenuInfo.IsOpen = false;
						SetWindowLong(g_Variables.g_hCheatWindow, GWL_EXSTYLE,
									  WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT);
					}
				}
			}
		}
		else
		{
					static char username[64] = { 0 };
					static char password[128] = { 0 };
			static float fade = 0.f;
			static float typingTimer = 0.f;
			static bool authAttempted = false;
			
			if (AuthGlobals::authError.empty() && !AuthGlobals::authInProgress.load()) {
				AuthGlobals::authError = g_Api.AuthError();
				if (!AuthGlobals::authError.empty()) authAttempted = true;
			}

			if (g_MenuInfo.IsOpen)
			{
				fade = ImClamp(fade + ImGui::GetIO().DeltaTime * 3.f, 0.f, 1.f);
				typingTimer += ImGui::GetIO().DeltaTime * 25.f;

				if (!g_MenuInfo.IsLogged)
				{
					std::string welcomeText = Utils::Translate(xorstr("Account login"));
					std::string infoText;
					
					if (AuthGlobals::authInProgress.load()) {
						infoText = Utils::Translate(xorstr("Validating... Please wait"));
					} else if (authAttempted && !AuthGlobals::authError.empty()) {
						infoText = AuthGlobals::authError;
					} else {
						infoText = Utils::Translate(xorstr("Enter your account username and password"));
					}

					int visibleChars1 = (int)typingTimer;
					int visibleChars2 = (int)(typingTimer - (float)welcomeText.length());

					std::string animatedWelcome = welcomeText.substr(0, min((int)welcomeText.length(), visibleChars1));
					std::string animatedInfo =
						visibleChars2 > 0 ? infoText.substr(0, min((int)infoText.length(), visibleChars2)) : std::string();

					float inputWidth = 200.0f;
					float buttonWidth = 200.0f;
					float totalHeight = 176.f;
					float startY = (g_MenuInfo.MenuSize.y - totalHeight) * 0.5f;

					// Compact login chrome: the W sits in the top-right and the
					// X is a real hit target for closing the application.
					ImGui::SetCursorPos( ImVec2( g_MenuInfo.MenuSize.x - 29.f, 5.f ) );
					ImGui::InvisibleButton( xorstr( "##login_close" ), ImVec2( 20.f, 20.f ) );
					const bool CloseHovered = ImGui::IsItemHovered( );
					const bool ClosePressed = ImGui::IsItemClicked( ImGuiMouseButton_Left );
					const ImVec2 CloseMin = ImGui::GetItemRectMin( );
					const ImVec2 CloseMax = ImGui::GetItemRectMax( );
					const ImVec2 CloseCenter = ImVec2(
						( CloseMin.x + CloseMax.x ) * 0.5f,
						( CloseMin.y + CloseMax.y ) * 0.5f );
					const float CloseArm = 4.f;
					const ImU32 CloseColor = ImGui::GetColorU32(
						CloseHovered ? g_Col.BaseLight : g_Col.SecundaryText );
					ImDrawList * LoginDraw = ImGui::GetWindowDrawList( );
					LoginDraw->AddLine(
						ImVec2( CloseCenter.x - CloseArm, CloseCenter.y - CloseArm ),
						ImVec2( CloseCenter.x + CloseArm, CloseCenter.y + CloseArm ),
						CloseColor, 1.5f );
					LoginDraw->AddLine(
						ImVec2( CloseCenter.x + CloseArm, CloseCenter.y - CloseArm ),
						ImVec2( CloseCenter.x - CloseArm, CloseCenter.y + CloseArm ),
						CloseColor, 1.5f );
					if ( CloseHovered )
						ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
					ImGui::SetCursorPos( ImVec2( 0.f, 0.f ) );

					if ( ClosePressed )
					{
						Core::Threads::ShutdownWebSocket( );
						g_Api.Logout( );
						ExitProcess( 0 );
					}

					ImGui::SetCursorPosY(startY);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fade);

					ImGui::PushFont(g_Variables.m_FontSecundary);
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, fade));
					Custom::TextCentered(animatedWelcome.c_str(), 1);
					ImGui::PopStyleColor();
					ImGui::PopFont();

					ImGui::Spacing();

					ImGui::PushFont(g_Variables.m_FontNormal);
					bool isError = (authAttempted && !AuthGlobals::authError.empty() && !AuthGlobals::authInProgress.load());
					bool isValidating = AuthGlobals::authInProgress.load();
					ImVec4 infoColor = isError ? ImVec4(1.0f, 0.3f, 0.3f, fade) : 
									   isValidating ? ImVec4(g_Col.BaseLight.x, g_Col.BaseLight.y, g_Col.BaseLight.z, fade) :
									   ImVec4(0.75f, 0.75f, 0.75f, fade);
					ImGui::PushStyleColor(ImGuiCol_Text, infoColor);
					
					// Mensagens de erro longas usam uma area rolavel.
					if (isError && AuthGlobals::authError.find('\n') != std::string::npos) {
						ImGui::SetCursorPosX(30.0f);
						ImGui::BeginChild(xorstr("##error_log"), ImVec2(g_MenuInfo.MenuSize.x - 60.0f, 100.0f), true);
						ImGui::TextWrapped(xorstr("%s"), animatedInfo.c_str());
						ImGui::EndChild();
					} else {
						Custom::TextCentered(animatedInfo.c_str(), 1);
					}
					
					ImGui::PopStyleColor();
					ImGui::PopFont();

					ImGui::Spacing();

					ImGui::SetCursorPosX((g_MenuInfo.MenuSize.x - inputWidth) * 0.5f);
					ImGui::PushItemWidth(inputWidth);
					ImGui::InputTextWithHint(xorstr("##username"), Utils::Translate(xorstr("Username")), username,
											 sizeof(username));
					ImGui::PopItemWidth();

					ImGui::Spacing();

					ImGui::SetCursorPosX((g_MenuInfo.MenuSize.x - inputWidth) * 0.5f);
					ImGui::PushItemWidth(inputWidth);
					ImGui::InputTextWithHint(xorstr("##password"), Utils::Translate(xorstr("Password")), password,
											 sizeof(password), ImGuiInputTextFlags_Password);
					ImGui::PopItemWidth();

					ImGui::Spacing();

					ImGui::SetCursorPosX((g_MenuInfo.MenuSize.x - buttonWidth) * 0.5f);
					
					// Desabilitar botão enquanto autentica
					bool isAuthInProgress = AuthGlobals::authInProgress.load();
					if (isAuthInProgress) {
						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
						ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					}
					
					if (Custom::Button(Utils::Translate(xorstr("Login")), {buttonWidth, g_MenuInfo.ButtonHeight}, 0))
					{
						if (!isAuthInProgress) {
							authAttempted = true;
							AuthGlobals::authError.clear();
							AuthGlobals::authInProgress.store(true);

							// Cópia das credenciais para a thread de autenticação
							std::string user(username);
							std::string pass(password);

							// Iniciar autenticação em thread separada
							std::thread([user, pass]() {
								std::string threadUser = user;
								std::string threadPass = pass;
								bool success = g_Api.CombinedLogin(threadUser, threadPass);

								SecureZeroMemory(&threadUser[0], threadUser.size());
								SecureZeroMemory(&threadPass[0], threadPass.size());

								std::lock_guard<std::mutex> lock(AuthGlobals::authMutex);
								if (success)
								{
									g_MenuInfo.IsLogged = true;
									g_MenuInfo.MenuSize = {700, 400};

									// WebSocket do painel: ws://...:1534 (sem certificado).
									Core::Threads::InitializeWebSocket( );
								}
								else
								{
									AuthGlobals::authError = g_Api.AuthError();
									if (AuthGlobals::authError.empty())
										AuthGlobals::authError = Utils::Translate(xorstr("Authentication failed"));
								}
								AuthGlobals::authInProgress.store(false);
							}).detach();

							// Limpar o campo de senha
							SecureZeroMemory(password, sizeof(password));
						}
					}
					
					if (isAuthInProgress) {
						ImGui::PopItemFlag();
						ImGui::PopStyleVar();
					}

					ImGui::PopStyleVar();
				}
			}
			else
			{
				fade = 0.f;
				typingTimer = 0.f;
			}
		}
		ImGui::PopStyleVar();
		ImGui::PopFont();
	}
	ImGui::End();
}
