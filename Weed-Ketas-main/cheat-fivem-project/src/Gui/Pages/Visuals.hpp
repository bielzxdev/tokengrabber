#pragma once
#include <Includes/includes.hpp>
#include <windows.h>
#include <iostream>
#include <thread>
#include <Includes/CustomWidgets/Preview.hpp>

#include <Core/Core.hpp>

using namespace std;

namespace Visuals {

	static int iSubTabCount = 0;
	static float SubTabAlpha = 0.f;
	static int iSubTab = 0;

	void Render( ) {

		const float Eased = Custom::PageTransition( SubTabAlpha, iSubTab, iSubTabCount );

		ImGui::PushStyleVar( ImGuiStyleVar_Alpha, Eased * ImGui::GetStyle( ).Alpha );

		const float ContentSlide = ( 1.f - Eased ) * g_MenuInfo.TransitionSlide;
		// The card body is the only scroll region. Do not wrap it in another
		// fixed-height child: that creates a second, oversized scrollbar and
		// leaves large empty areas on short pages.
		Custom::BeginCards( ContentSlide + g_MenuInfo.TabAdd );
		{
			switch ( iSubTab )
			{
			case 0: //Players
				Custom::BeginCard( Utils::Translate( xorstr( "Player Visuals" ) ) );
				{
					Custom::SectionLabel( Utils::Translate( xorstr( "Display" ) ) );
						Custom::CheckBoxCfg(Utils::Translate( xorstr("Enable Visuals") ), &g_Config.ESP->Enabled, xorstr( "Liga o ESP de jogadores." ), [&]() { static int KeyMode = 1; ImGui::Keybind(Utils::Translate( xorstr("Bind") ), &g_Config.ESP->KeyBind, &KeyMode); }, false, xorstr(""), xorstr(""));

						if ( Custom::CheckBox( Utils::Translate( xorstr( "Box" ) ), &g_Config.ESP->Box, xorstr( "Desenha uma caixa em volta do jogador." ) ) ) {
							if ( !g_Config.ESP->Box ) {
								g_Config.ESP->FilledBox = false;
							}
						}

						if ( Custom::CheckBox( Utils::Translate( xorstr( "Filled Box" ) ), &g_Config.ESP->FilledBox, xorstr( "Preenche a caixa com uma cor translúcida." ) ) ) {
							if ( !g_Config.ESP->Box ) {
								g_Config.ESP->Box = true;
							}
						}

						Custom::CheckBox( Utils::Translate( xorstr( "Player Names" ) ), &g_Config.ESP->UserNames, xorstr( "Mostra o nome do jogador acima da caixa." ) );
						Custom::CheckBox( Utils::Translate( xorstr( "Weapon Names" ) ), &g_Config.ESP->WeaponName, xorstr( "Mostra qual arma o jogador está segurando." ) );
						Custom::CheckBox( Utils::Translate( xorstr( "Distance" ) ), &g_Config.ESP->DistanceFromMe, xorstr( "Mostra a distância até o jogador, em metros." ) );

						if ( Custom::CheckBox( Utils::Translate( xorstr( "Skeleton" ) ), &g_Config.ESP->Skeleton, xorstr( "Desenha o esqueleto do jogador." ) ) )
						{
							Core::SDK::Pointers::pLocalPlayer->RemoveKinematics( );
						}

						Custom::CheckBox( Utils::Translate( xorstr( "Gay Mode" ) ), &g_Config.ESP->GayMode, xorstr( "Enquanto o aimbot está puxando em alguém, o esqueleto dele fica alternando entre branco, rosa, azul e verde." ) );
						Custom::CheckBox( Utils::Translate( xorstr( "Head Circle" ) ), &g_Config.ESP->HeadCircle, xorstr( "Desenha um círculo na cabeça do jogador." ) );
						Custom::CheckBox( Utils::Translate( xorstr( "Health" ) ), &g_Config.ESP->HealthBar, xorstr( "Mostra a vida do jogador como barra ou texto." ) );
						if ( g_Config.ESP->HealthBar )
						{
							const char * HealthStyles[] = {
								Utils::Translate( xorstr( "Bar" ) ),
								Utils::Translate( xorstr( "Text" ) )
							};
							ImGui::Combo( Utils::Translate( xorstr( "Health Display" ) ),
								&g_Config.ESP->HealthStyle, HealthStyles, IM_ARRAYSIZE( HealthStyles ) );
						}
						Custom::CheckBox( Utils::Translate( xorstr( "Armor" ) ), &g_Config.ESP->ArmorBar, xorstr( "Mostra o colete do jogador como barra ou texto." ) );
						if ( g_Config.ESP->ArmorBar )
						{
							const char * ArmorStyles[] = {
								Utils::Translate( xorstr( "Bar" ) ),
								Utils::Translate( xorstr( "Text" ) )
							};
							ImGui::Combo( Utils::Translate( xorstr( "Armor Display" ) ),
								&g_Config.ESP->ArmorStyle, ArmorStyles, IM_ARRAYSIZE( ArmorStyles ) );
						}
						Custom::CheckBox( Utils::Translate( xorstr( "SnapLines" ) ), &g_Config.ESP->SnapLines, xorstr( "Desenha uma linha da sua mira até o jogador." ) );

						Custom::CheckBoxCfg( Utils::Translate( xorstr( "Highlight Visible" ) ), &g_Config.ESP->HighlightVisible, xorstr( "Pinta o jogador de uma cor quando ele está visível e de outra quando está atrás de parede. Clique na engrenagem para escolher as cores." ), [ ]
							{
								// Mesmo padrao das outras cores do ESP na pagina
								// Settings: ColorEdit4 com AlphaBar sobre um
								// float[4]. Com NoInputs o widget saia fora de
								// proporcao dentro do popup.
								static float vis_col[ 4 ] = {
									g_Config.ESP->HighlightVisCol.Value.x, g_Config.ESP->HighlightVisCol.Value.y,
									g_Config.ESP->HighlightVisCol.Value.z, g_Config.ESP->HighlightVisCol.Value.w };

								if ( ImGui::ColorEdit4( Utils::Translate( xorstr( "Visible" ) ), vis_col, ImGuiColorEditFlags_AlphaBar ) )
								{
									g_Config.ESP->HighlightVisCol.Value.x = vis_col[ 0 ];
									g_Config.ESP->HighlightVisCol.Value.y = vis_col[ 1 ];
									g_Config.ESP->HighlightVisCol.Value.z = vis_col[ 2 ];
									g_Config.ESP->HighlightVisCol.Value.w = vis_col[ 3 ];
								}

								static float occ_col[ 4 ] = {
									g_Config.ESP->HighlightOccCol.Value.x, g_Config.ESP->HighlightOccCol.Value.y,
									g_Config.ESP->HighlightOccCol.Value.z, g_Config.ESP->HighlightOccCol.Value.w };

								if ( ImGui::ColorEdit4( Utils::Translate( xorstr( "Occluded" ) ), occ_col, ImGuiColorEditFlags_AlphaBar ) )
								{
									g_Config.ESP->HighlightOccCol.Value.x = occ_col[ 0 ];
									g_Config.ESP->HighlightOccCol.Value.y = occ_col[ 1 ];
									g_Config.ESP->HighlightOccCol.Value.z = occ_col[ 2 ];
									g_Config.ESP->HighlightOccCol.Value.w = occ_col[ 3 ];
								}
							}
						);
						Custom::CheckBoxCfg( Utils::Translate( xorstr( "Friend Marker" ) ), &g_Config.ESP->FriendsMarker, xorstr( "Marca um jogador como amigo com a tecla escolhida. Amigos são ignorados pelo aimbot." ), [ ]
							{
								static int KeyMode = 1;
								ImGui::Keybind( Utils::Translate( xorstr( "Bind" ) ), &g_Config.ESP->FriendsMarkerBind, &KeyMode );
							}
						);
						Custom::CheckBox( Utils::Translate( xorstr( "Show LocalPlayer" ) ), &g_Config.ESP->ShowLocalPlayer, xorstr( "Desenha o ESP no seu próprio personagem." ) );
						Custom::CheckBox( Utils::Translate( xorstr( "Ignore Dead" ) ), &g_Config.ESP->IgnoreDead, xorstr( "Não desenha jogadores mortos." ) );
						Custom::CheckBox( Utils::Translate( xorstr( "Ignore NPCs" ) ), &g_Config.ESP->IgnoreNPCs, xorstr( "Não desenha NPCs, só jogadores de verdade." ) );

					ImGui::SliderInt( Utils::Translate( xorstr( "Max Distance" ) ), &g_Config.ESP->MaxDistance, 0, 500, xorstr( "%dm" ), 0 );
				}
				Custom::EndCard( );

				Custom::BeginCard( Utils::Translate( xorstr( "Preview" ) ) );
				{
					Custom::g_EspPreview.DragDropHandler( );
					Custom::g_EspPreview.Draw( );
				}
				Custom::EndCard( );

				break;
			case 1: //Vehicles
				{
					// Six rows, one slider and the section label fit comfortably
					// in this compact card. Clamp for small viewports.
					const float VehicleCardHeight = ImMin( g_MenuInfo.CardHeight, 294.f );
					Custom::BeginCard( Utils::Translate( xorstr( "Vehicle Visuals" ) ),
						1, VehicleCardHeight );
					Custom::SectionLabel( Utils::Translate( xorstr( "Display" ) ) );
					Custom::Toggle( Utils::Translate( xorstr( "Enable Vehicle ESP" ) ), &g_Config.VehicleESP->Enabled, xorstr( "Liga o ESP de veículos." ) );

					Custom::CheckBox( Utils::Translate( xorstr( "Vehicle Names" ) ), &g_Config.VehicleESP->VehName, xorstr( "Mostra o nome do modelo do veículo." ) );
					Custom::CheckBox( Utils::Translate( xorstr( "Locked/Unlocked" ) ), &g_Config.VehicleESP->ShowLockUnlock, xorstr( "Mostra se o veículo está trancado ou aberto." ) );
					Custom::CheckBox( Utils::Translate( xorstr( "Vehicle Health" ) ), &g_Config.VehicleESP->VehHealth, xorstr( "Mostra a vida atual do veículo." ) );
					Custom::CheckBox( Utils::Translate( xorstr( "SnapLines" ) ), &g_Config.VehicleESP->SnapLines, xorstr( "Desenha uma linha da sua mira até o veículo." ) );
					Custom::CheckBox( Utils::Translate( xorstr( "Distance" ) ), &g_Config.VehicleESP->DistanceFromMe, xorstr( "Mostra a distância até o veículo, em metros." ) );

					ImGui::SliderInt( Utils::Translate( xorstr( "Max Distance" ) ), &g_Config.VehicleESP->MaxDistance, 0, 500, xorstr( "%dm" ), 0 );
					Custom::EndCard( );
				}

				break;
			case 2: //Radar
				Custom::BeginCard(
					Utils::Translate(xorstr("WebRadar")),
					2,
					0.f,
					0.f,
					true,
					ICON_FA_SATELLITE
				);
				{
					Custom::SectionLabel( Utils::Translate( xorstr( "Radar" ) ) );
					Custom::CheckBox(
						Utils::Translate( xorstr( "Enable Radar" ) ),
						&g_Config.ESP->RadarEnabled,
						xorstr( "Shows or hides the radar." )
					);

					bool RadarDetached = g_Config.ESP->RadarDetached;
					if ( Custom::CheckBox(
						Utils::Translate( xorstr( "Detach Radar" ) ),
						&RadarDetached,
						xorstr( "Detaches the radar from the card and leaves it free on the overlay." )
					) )
					{
						if ( RadarDetached )
							Core::Features::g_Esp.DeallocateRadar();
						else
							Core::Features::g_Esp.SetRadarDetached( false );
					}

					if ( g_Config.ESP->RadarDetached )
					{
						ImGui::TextColored(
							g_Col.SecundaryText,
							xorstr( "%s" ),
							Utils::Translate( xorstr( "Radar detached: drag the title bar to move it." ) )
						);
						ImGui::Dummy( ImVec2( 0.f, 4.f ) );
						if ( Custom::Button(
							Utils::Translate( xorstr( "Reset Radar Position" ) ),
							ImVec2( -1.f, g_MenuInfo.ButtonHeight ),
							0
						) )
						{
							Core::Features::g_Esp.ResetRadarPosition();
						}
					}
					else if ( g_Config.ESP->RadarEnabled )
					{
						ImGui::Dummy( ImVec2( 0.f, 5.f ) );
						Core::Features::g_Esp.RenderRadar();
					}
					else
					{
						ImGui::TextColored(
							g_Col.SecundaryText,
							xorstr( "%s" ),
							Utils::Translate( xorstr( "Radar disabled" ) )
						);
					}
				}
				Custom::EndCard();
				break;
			default:
				break;
			}

		}
		ImGui::PopStyleVar( );
	}
}
