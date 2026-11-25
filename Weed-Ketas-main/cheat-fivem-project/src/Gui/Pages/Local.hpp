#pragma once
#include <Includes/includes.hpp>
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <thread>

#include <Core/Core.hpp>
#include <Core/Features/Exploits/Exploits.hpp>
#include <Core/Features/Exploits/HandlingEditor.hpp>

using namespace std;

namespace Local {

	static int iSubTabCount = 0;
	static float SubTabAlpha = 0.f;
	static int iSubTab = 0;

	void Render( )
	{
		const float Eased = Custom::PageTransition( SubTabAlpha, iSubTab, iSubTabCount );

		ImGui::PushStyleVar( ImGuiStyleVar_Alpha, Eased * ImGui::GetStyle( ).Alpha );

		float ContentSlide = ( 1.f - Eased ) * g_MenuInfo.TransitionSlide;
		Custom::BeginCards( ContentSlide + g_MenuInfo.TabAdd );
		{
			switch ( iSubTab )
			{
			case 0: //Main
				Custom::BeginCard( Utils::Translate( xorstr( "Player Features" ) ) );
				{
					{

						if ( Custom::CheckBoxCfg( Utils::Translate( xorstr( "GodMode" ) ), &g_Config.Player->EnableGodMode, xorstr( "Você para de receber dano." ), [ & ] ( ) { static int KeyMode = 1; ImGui::Keybind( Utils::Translate( xorstr( "Bind" ) ), &g_Config.Player->GodModeKey, &KeyMode ); }, true, Utils::Translate( xorstr( "That function can be detected by server AntiCheat" ) ), ICON_FA_TRIANGLE_EXCLAMATION ) )
						{
							Core::SDK::Pointers::pLocalPlayer->SetGodMode( g_Config.Player->EnableGodMode );
						}

						Custom::CheckBoxCfg(Utils::Translate( xorstr( "Car Throw" ) ), &g_Config.Player->CarThrowEnabled, xorstr( "Pressione a tecla perto de um veículo para levantar, pressione de novo para arremessar." ), [&]() {
							static int KeyMode = 1;
							ImGui::Keybind(Utils::Translate( xorstr( "Bind" ) ), &g_Config.Player->CarThrowKey, &KeyMode);
							ImGui::SliderFloat(Utils::Translate( xorstr( "Force" ) ), &g_Config.Player->CarThrowForce, 20.f, 200.f, xorstr( "%.0f" ));
						}, false);

						Custom::CheckBoxCfg(Utils::Translate( xorstr( "Magnetto" ) ), &g_Config.Player->MagnettoEnabled, xorstr( "Puxa todos os veículos próximos para onde você está mirando. Segure a tecla." ), [&]() {
							static int KeyMode = 1;
							ImGui::Keybind(Utils::Translate( xorstr( "Bind" ) ), &g_Config.Player->MagnettoKey, &KeyMode);
							ImGui::SliderFloat(Utils::Translate( xorstr( "Force" ) ), &g_Config.Player->MagnettoForce, 50.f, 500.f, xorstr( "%.0f" ));
							ImGui::SliderFloat(Utils::Translate( xorstr( "Range" ) ), &g_Config.Player->MagnettoRange, 10.f, 200.f, xorstr( "%.0fm" ));
						}, false);

						Custom::CheckBoxCfg(Utils::Translate( xorstr( "Freecam" ) ), &g_Config.Player->FreecamEnabled, xorstr( "Câmera livre independente do jogador. Use WASD/Space/Ctrl para mover." ), [&]() {
							static int KeyMode = 1;
							ImGui::Keybind(Utils::Translate( xorstr( "Bind" ) ), &g_Config.Player->FreecamKey, &KeyMode);
							ImGui::SliderFloat(Utils::Translate( xorstr( "Speed" ) ), &g_Config.Player->FreecamSpeed, 1.f, 50.f, xorstr( "%.0f" ));
						}, false);

						if (g_Config.Player->BlockAiming && Core::SDK::Pointers::pLocalPlayer) {
							Core::SDK::Pointers::pLocalPlayer->SetBlockAimming(true);
						}

						if (Custom::CheckBoxCfg(Utils::Translate( xorstr( "Block Aiming" ) ), &g_Config.Player->BlockAiming, xorstr( "Impede que os outros jogadores travem a mira em você." ), [&]() { static int KeyMode = 1; ImGui::Keybind(Utils::Translate( xorstr( "Bind" ) ), &g_Config.Player->BlockAimingKey, &KeyMode); }, false))
						{
							if (!g_Config.Player->BlockAiming && Core::SDK::Pointers::pLocalPlayer)
								Core::SDK::Pointers::pLocalPlayer->SetBlockAimming(false);
						}

						if ( Custom::CheckBoxCfg( Utils::Translate( xorstr( "NoClip" ) ), &g_Config.Player->NoClipEnabled, xorstr( "Voa e atravessa paredes. Use a tecla escolhida para ligar e desligar." ), [ & ] ( ) { static int KeyMode = 1; ImGui::Keybind( Utils::Translate( xorstr( "Bind" ) ), &g_Config.Player->NoClipKey, &KeyMode ); ImGui::SliderFloat( Utils::Translate( xorstr( "Speed" ) ), &g_Config.Player->NoClipSpeed, 0.1f, 60.f, xorstr( "%1.2fm/s" ) ); }, true, Utils::Translate( xorstr( "That function can be detected by server AntiCheat" ) ), ICON_FA_TRIANGLE_EXCLAMATION ) )
						{
							Core::SDK::Pointers::pLocalPlayer->NoRagDoll( g_Config.Player->NoClipEnabled );
						}

						if ( Custom::CheckBoxCfg( Utils::Translate( xorstr( "Fast Run" ) ), &g_Config.Player->FastRun, xorstr( "Aumenta a sua velocidade de corrida." ), [ & ] ( ) {
							if ( ImGui::SliderFloat( Utils::Translate( xorstr( "Speed" ) ), &g_Config.Player->RunSpeed, 1.f, 10.f, xorstr( "%1.2fm/s" ) ) )
							{
								if ( g_Config.Player->FastRun )
									Core::SDK::Pointers::pLocalPlayer->SetSpeed( g_Config.Player->RunSpeed );
							}
							} ) )
						{
							if ( !g_Config.Player->FastRun ) {
								Core::SDK::Pointers::pLocalPlayer->SetSpeed( 1.f );
							}
						}

						Custom::CheckBox( Utils::Translate( xorstr( "Anti Headshot" ) ),
							&g_Config.Player->AntiHSEnabled,
							xorstr( "Tiro na cabeça deixa de ser fatal." ) );

						Custom::CheckBox( Utils::Translate( xorstr( "Shrink" ) ),
							&g_Config.Player->ShrinkEnabled,
							xorstr( "Encolhe o seu personagem, ficando mais difícil de acertar." ) );

						if (g_Config.Player->NoRagDollEnabled && Core::SDK::Pointers::pLocalPlayer) {
							Core::SDK::Pointers::pLocalPlayer->NoRagDoll(true);
						}

						if ( Custom::CheckBox( Utils::Translate( xorstr( "No RagDoll" ) ), &g_Config.Player->NoRagDollEnabled, xorstr( "Seu personagem não cai mais no chão ao levar impacto." ) ) )
						{
							if (!g_Config.Player->NoRagDollEnabled && Core::SDK::Pointers::pLocalPlayer)
								Core::SDK::Pointers::pLocalPlayer->NoRagDoll(false);
						}

						Custom::CheckBox( Utils::Translate( xorstr( "Steal Car" ) ), &g_Config.Player->StealCarEnabled, xorstr( "Puxa o motorista para fora ao entrar no veículo." ) );

						if ( Custom::CheckBox( Utils::Translate( xorstr( "Infinite CombatRoll" ) ), &g_Config.Player->InfiniteCombatRoll, xorstr( "Rola sem limite, sem esperar entre um rolamento e outro." ) ) )
						{
							std::thread InfiniteCombatRoll( [ ] ( ) { Core::SDK::Pointers::pLocalPlayer->SetInfCombatRoll( g_Config.Player->InfiniteCombatRoll ); } );
							InfiniteCombatRoll.detach( );
						}

						Custom::CheckBox( Utils::Translate( xorstr( "Infinite Stamina" ) ), &g_Config.Player->InfiniteStamina, xorstr( "Corre sem cansar." ) );

						if (Custom::CheckBox(Utils::Translate( xorstr( "Force Tab" ) ), &g_Config.Player->ForceWeaponWheel, xorstr( "Mantém a roda de armas aberta." ) ))
						{
							std::thread ForceWeaponWheel([]() { Core::SDK::Pointers::pLocalPlayer->ForceWeaponWheel(g_Config.Player->ForceWeaponWheel); });
							ForceWeaponWheel.detach();
						}

						Custom::CheckBox(Utils::Translate( xorstr( "Invisible" ) ), &g_Config.Player->Invisible, xorstr( "Deixa o seu personagem invisível para os outros." ) );
					}
				}
				Custom::EndCard( );

				Custom::BeginCard( Utils::Translate( xorstr( "Health and Armor Actions" ) ) );
				{
					{
						// Os dois valores eram sobrescritos do jogo a CADA frame e
						// aplicados no ato em que o slider mexia. Com botao isso nao
						// funciona: o valor escolhido seria apagado no frame seguinte,
						// antes de dar tempo de clicar.
						//
						// Agora o slider tem estado proprio, semeado uma vez com a
						// vida/colete atuais, e so o botao escreve no jogo.
						const float MaxHealth = Core::SDK::Pointers::pLocalPlayer->GetMaxHealth( );

						static bool  Seeded = false;
						static float HealthValue = 0.f;
						static float ArmorValue = 0.f;

						if ( !Seeded )
						{
							HealthValue = Core::SDK::Pointers::pLocalPlayer->GetHealth( ) - 100.f;
							ArmorValue = Core::SDK::Pointers::pLocalPlayer->GetArmor( );
							Seeded = true;
						}

						ImGui::SliderFloat( Utils::Translate( xorstr( "Health" ) ), &HealthValue, 0.f, MaxHealth, xorstr( "%1.f" ) );

						if ( Custom::Button( Utils::Translate( xorstr( "Set Health" ) ), ImVec2( -1, g_MenuInfo.ButtonHeight ), 0, true,
							Utils::Translate( xorstr( "That function can be detected by server AntiCheat" ) ), ICON_FA_TRIANGLE_EXCLAMATION ) )
						{
							// O jogo guarda a vida com 100 de offset.
							Core::SDK::Pointers::pLocalPlayer->SetHealth( HealthValue + 100.f );
						}

						ImGui::Dummy( ImVec2( 0.f, 4.f ) );

						ImGui::SliderFloat( Utils::Translate( xorstr( "Armor" ) ), &ArmorValue, 0.f, 100.f, xorstr( "%1.f" ) );

						if ( Custom::Button( Utils::Translate( xorstr( "Set Armor" ) ), ImVec2( -1, g_MenuInfo.ButtonHeight ), 0, true,
							Utils::Translate( xorstr( "That function can be detected by server AntiCheat" ) ), ICON_FA_TRIANGLE_EXCLAMATION ) )
						{
							Core::SDK::Pointers::pLocalPlayer->SetArmor( ArmorValue );
						}
					}
				}
				Custom::EndCard( );
				break;
			case 1: //Vehicles
			{
				bool InVehicle = Core::SDK::Pointers::pLocalPlayer->InVehicle( );

				auto CurrentVehicle = Core::SDK::Pointers::pLocalPlayer->GetLastVehicle( );

				Custom::BeginCard( Utils::Translate( xorstr( "Vehicle Features" ) ) );
				{
					{
						if (Custom::CheckBox(Utils::Translate( xorstr( "GodMode" ) ), &g_Config.Player->VehicleGodMode, xorstr( "O veículo para de receber dano." ) ))
						{
							if (!InVehicle)
							{
								g_Config.Player->VehicleGodMode = false;
								std::thread( NotifyManager::Send, Utils::Translate( xorstr( "You must be in a car to use this function" ) ), 4000 ).detach( );
							}
							else {
								CurrentVehicle->SetGodMode(g_Config.Player->VehicleGodMode);
							}
						}

						// So le o estado do veiculo enquanto o usuario nao mexeu no
						// checkbox, senao o valor dele seria sobrescrito no frame
						// seguinte ao clique. O ponteiro pode ser nulo fora do carro.
						static bool Locked = false;
						static bool LockedTouched = false;

						if ( !LockedTouched && CurrentVehicle )
							Locked = CurrentVehicle->IsLocked( );
						if ( Custom::CheckBox( Utils::Translate( xorstr( "Doors Locked" ) ), &Locked, xorstr( "Tranca ou destranca as portas do veículo." ) ) )
						{
							LockedTouched = true;

							if ( CurrentVehicle )
								CurrentVehicle->DoorState( Locked ? eVehicleLockState::VEHICLELOCK_LOCKED : eVehicleLockState::VEHICLELOCK_UNLOCKED );
						}

						if (Custom::CheckBox(Utils::Translate( xorstr( "Don't Fall" ) ), &g_Config.Player->SeatBelt, xorstr( "Você não é arremessado para fora do veículo na batida." ) ))
						{
							std::thread SeatBelt([]() { Core::SDK::Pointers::pLocalPlayer->SeatBealt(g_Config.Player->SeatBelt); });
							SeatBelt.detach();
						}

						if ( Custom::CheckBoxPage( Utils::Translate( xorstr( "Handling Editor" ) ), &g_Config.Player->HandlingEditor, xorstr( "Abre a página de edição do handling do veículo." ), [ & ] ( ) {
							if ( !InVehicle ) {
								std::thread( NotifyManager::Send, Utils::Translate( xorstr( "You must be in a car to use this function" ) ), 4000 ).detach( );
							}
							else {
								iSubTabCount = 4;
							}
							} ) )
						{
							if ( !InVehicle )
							{
								g_Config.Player->HandlingEditor = false;
								std::thread( NotifyManager::Send, Utils::Translate( xorstr( "You must be in a car to use this function" ) ), 4000 ).detach( );
							}

							if ( g_Config.Player->HandlingEditor )
							{
								Core::Features::Exploits::g_HandlingEditor.SaveHandlingValues( );
							}
							else
							{
								Core::Features::Exploits::g_HandlingEditor.RestoreHandlingValues( );
							}

						}

					}
				}
				Custom::EndCard( );

				Custom::BeginCard( Utils::Translate( xorstr( "Vehicle Actions" ) ) );
				{
					Custom::SectionLabel( Utils::Translate( xorstr( "Actions" ) ) );

					if ( Custom::Button( Utils::Translate( xorstr( "Repair Vehicle" ) ), ImVec2( -1, g_MenuInfo.ButtonHeight ), 0 ) )
					{
						CurrentVehicle->Fix( Core::SDK::Pointers::pLocalPlayer );
					}

					if ( Custom::Button( Utils::Translate( xorstr( "Perfect Tune" ) ), ImVec2( -1, g_MenuInfo.ButtonHeight ), 0 ) )
					{
						if ( !InVehicle || !CurrentVehicle )
						{
							NotifyManager::Send(
								std::string( Utils::Translate( xorstr( "You must be in a car to use this function" ) ) ),
								4000 );
						}
						else if ( !Core::Features::Exploits::g_HandlingEditor.ApplyPerfectTune( ) )
						{
							NotifyManager::Send(
								std::string( Utils::Translate( xorstr( "Failed to apply tune" ) ) ),
								4000 );
						}
					}

					if ( Core::Features::Exploits::g_HandlingEditor.HasPerfectTuneApplied( ) )
					{
						if ( Custom::Button( Utils::Translate( xorstr( "Restore Original Tune" ) ), ImVec2( -1, g_MenuInfo.ButtonHeight ), 0 ) )
						{
							Core::Features::Exploits::g_HandlingEditor.RestorePerfectTune( );
						}
					}

					if ( Custom::Button( Utils::Translate( xorstr( "Armored Vehicle" ) ), ImVec2( -1, g_MenuInfo.ButtonHeight ), 0 ) )
					{
						if ( !InVehicle || !CurrentVehicle )
						{
							NotifyManager::Send(
								std::string( Utils::Translate( xorstr( "You must be in a car to use this function" ) ) ),
								4000 );
						}
						else if ( !Core::Features::Exploits::g_HandlingEditor.ApplyArmor( ) )
						{
							NotifyManager::Send(
								std::string( Utils::Translate( xorstr( "Failed to apply armor" ) ) ),
								4000 );
						}
					}

					if ( Core::Features::Exploits::g_HandlingEditor.HasArmorApplied( ) )
					{
						if ( Custom::Button( Utils::Translate( xorstr( "Restore Original Armor" ) ), ImVec2( -1, g_MenuInfo.ButtonHeight ), 0 ) )
						{
							Core::Features::Exploits::g_HandlingEditor.RestoreArmor( );
						}
					}

					if ( Custom::Button( Utils::Translate( xorstr( "Install Armor Mod (Max)" ) ), ImVec2( -1, g_MenuInfo.ButtonHeight ), 0 ) )
					{
						if ( !InVehicle || !CurrentVehicle )
						{
							NotifyManager::Send(
								std::string( Utils::Translate( xorstr( "You must be in a car to use this function" ) ) ),
								4000 );
						}
						else
						{
							const uintptr_t VehicleAddress = reinterpret_cast< uintptr_t >( CurrentVehicle );
							std::thread( [ VehicleAddress ] ( )
							{
								try
								{
									auto* Vehicle = reinterpret_cast< CVehicle* >( VehicleAddress );
									const bool Ok = Vehicle->InstallMaxArmorMod( );
									NotifyManager::Send(
										std::string( Utils::Translate( Ok
											? xorstr( "Armor mod installed" )
											: xorstr( "Failed to install armor mod" ) ) ),
										3500 );
								}
								catch ( const std::exception& )
								{
								}
							} ).detach( );
						}
					}

					if ( Custom::CheckBox( Utils::Translate( xorstr( "Refill Rocket Boost" ) ), &g_Config.Player->RocketBoost, xorstr( "Recarrega o boost do veículo usando a native do jogo." ) ) )
					{
						if ( g_Config.Player->RocketBoost && ( !InVehicle || !CurrentVehicle ) )
						{
							g_Config.Player->RocketBoost = false;
							NotifyManager::Send(
								std::string(Utils::Translate(xorstr("You must be in a car to use this function"))),
								4000);
						}
						else if ( g_Config.Player->RocketBoost )
						{
							const uintptr_t VehicleAddress = reinterpret_cast< uintptr_t >( CurrentVehicle );
							g_Config.Player->RocketBoost = false;
							std::thread( [ VehicleAddress ] ( )
							{
								try
								{
									auto Vehicle = reinterpret_cast< CVehicle * >( VehicleAddress );
									Vehicle->RefillVehicleRocketBoost( );
								}
								catch ( const std::exception & )
								{
								}
							} ).detach( );
						}
					}

					if ( Custom::CheckBox( Utils::Translate( xorstr( "Hornet Boost" ) ), &g_Config.Player->HornetBoost, xorstr( "Enquanto a buzina (E) está pressionada, aplica rocket boost sem delay." ) ) )
					{
						if ( g_Config.Player->HornetBoost && !InVehicle )
						{
							g_Config.Player->HornetBoost = false;
							NotifyManager::Send(
								std::string(Utils::Translate(xorstr("You must be in a car to use this function"))),
								4000);
						}
					}

					if ( Custom::CheckBox( Utils::Translate( xorstr( "Always Engine On" ) ), &g_Config.Player->AlwaysEngineOn, xorstr( "Mantém o motor do veículo sempre ligado, independente da situação." ) ) )
					{
						if ( g_Config.Player->AlwaysEngineOn && !InVehicle )
						{
							g_Config.Player->AlwaysEngineOn = false;
							NotifyManager::Send(
								std::string(Utils::Translate(xorstr("You must be in a car to use this function"))),
								4000);
						}
					}

					{
						// O texto exibido vem da native do jogo, que decodifica o buffer
						// interno da placa antes de devolve-lo.
						static char PlateBuffer[9] = {};
						static CVehicle* PlateVehicle = nullptr;

						if ( InVehicle && CurrentVehicle && CurrentVehicle != PlateVehicle )
						{
							const std::string Current = CurrentVehicle->GetPlateText();
							std::memset( PlateBuffer, 0, sizeof( PlateBuffer ) );
							if ( !Current.empty() )
								std::memcpy( PlateBuffer, Current.c_str(),
									std::min<size_t>( Current.size(), sizeof( PlateBuffer ) - 1 ) );
							PlateVehicle = CurrentVehicle;
						}
						else if ( !InVehicle || !CurrentVehicle )
						{
							// Sem veiculo: limpa o estado pra proxima entrada
							// comecar do zero.
							PlateVehicle = nullptr;
							std::memset( PlateBuffer, 0, sizeof( PlateBuffer ) );
						}

						const float PlatePaddingY = std::max(
							0.f, (g_MenuInfo.ButtonHeight - ImGui::GetFontSize()) * 0.5f );
						ImGui::PushID( xorstr( "VehiclePlate" ) );
						ImGui::PushStyleVar( ImGuiStyleVar_FramePadding,
							ImVec2( ImGui::GetStyle().FramePadding.x, PlatePaddingY ) );
						ImGui::SetNextItemWidth( -1.f );
						if ( ImGui::InputTextWithHint(
							xorstr( "##Plate" ), Utils::Translate( xorstr( "Plate" ) ),
							PlateBuffer, sizeof( PlateBuffer ),
							ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank ) )
						{
							if ( InVehicle && CurrentVehicle )
								CurrentVehicle->SetPlateText( PlateBuffer );
						}
						ImGui::PopStyleVar( );
						ImGui::PopID( );
					}

					static int KeyMode = 1;
					ImGui::Keybind(Utils::Translate( xorstr( "Bind Fix" ) ), &g_Config.Player->KeyBindFix, &KeyMode);

					static int KeyMode2 = 1;
					ImGui::Keybind(Utils::Translate( xorstr( "Bind Unlock" ) ), &g_Config.Player->KeyBindUnlock, &KeyMode2);


				}
				Custom::EndCard( );
			}
			break;
			case 2: //Teleports
			{
				struct Locations_t
				{
					std::string Name;
					D3DXVECTOR3 Coords;
					bool IsCustom;
				};

				static std::vector<Locations_t> DefaultLocations =
				{
					{ Utils::Translate(xorstr("Waypoint")), D3DXVECTOR3(0, 0, 0), false },
					{ Utils::Translate(xorstr("Square")), D3DXVECTOR3(156.184f, -1043.17f, 29.3236f), false },
					{ Utils::Translate(xorstr("Pier")), D3DXVECTOR3(-1847.72f, -1223.36f, 13.8745f), false },
					{ Utils::Translate(xorstr("Paleto Bay")), D3DXVECTOR3(-397.605f, 6047.57f, 32.1797f), false },
					{ Utils::Translate(xorstr("Central Bank")), D3DXVECTOR3(221.781f, 217.278f, 106.705f), false },
					{ Utils::Translate(xorstr("Cassino")), D3DXVECTOR3(885.322f, 16.8489f, 80.65f), false },
					{ Utils::Translate(xorstr("Los Santos Airport")), D3DXVECTOR3(-975.532f, -2880.89f, 16.2665f), false },
				};

				static std::vector<Locations_t> Locations = DefaultLocations;
				static int SelectedIndex = 0;
				static char CustomName[64] = "";

				Custom::BeginCard(Utils::Translate(xorstr("Locations")), 1, 0.f, 0.f, true, ICON_FA_MAP_PIN);
				{
					Custom::SectionLabel(Utils::Translate(xorstr("Select a location to teleport.")));

					ImGui::Dummy(ImVec2(0.f, 3.f));

					{
						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, g_MenuInfo.RowSpacing));

						for (size_t i = 0; i < Locations.size(); i++)
						{
							bool Selected = (SelectedIndex == static_cast<int>(i));

							if (ImGui::ListSelectable(Locations[i].Name.c_str(), &Selected))
								SelectedIndex = static_cast<int>(i);
						}

						ImGui::PopStyleVar();
					}
				}
				Custom::EndCard();

				Custom::BeginCard(Utils::Translate(xorstr("Actions")), 1, 0.f, 0.f, true, ICON_FA_WRENCH);
				{
					if (SelectedIndex >= static_cast<int>(Locations.size()))
						SelectedIndex = 0;

					auto& Location = Locations[SelectedIndex];

					Custom::SectionLabel(Utils::Translate(xorstr("Teleport")));

					if (Custom::Button(
						(Utils::Translate(xorstr("Teleport to ")) + Location.Name).c_str(),
						ImVec2(-1, g_MenuInfo.ButtonHeight),
						0))
					{
						if (SelectedIndex == 0)
							Core::Features::Exploits::TpToWaypoint();
						else
							Core::SDK::Pointers::pLocalPlayer->SetPos(Location.Coords);
					}

					ImGui::Dummy(ImVec2(0.f, 6.f));

					Custom::SectionLabel(Utils::Translate(xorstr("Custom Location")));

					const float LocationPaddingY = std::max(
						0.f, (g_MenuInfo.ButtonHeight - ImGui::GetFontSize()) * 0.5f );
					ImGui::PushStyleVar( ImGuiStyleVar_FramePadding,
						ImVec2( ImGui::GetStyle().FramePadding.x, LocationPaddingY ) );
					ImGui::SetNextItemWidth( -1.f );
					ImGui::InputTextWithHint(
						xorstr("##CustomLocationName"),
						Utils::Translate(xorstr("Custom Location")),
						CustomName,
						sizeof(CustomName),
						ImGuiInputTextFlags_None
					);
					ImGui::PopStyleVar( );

					if (Custom::Button(
						Utils::Translate(xorstr("Save Current Position")),
						ImVec2(-1, g_MenuInfo.ButtonHeight),
						0))
					{
						if (strlen(CustomName) > 0)
						{
							D3DXVECTOR3 Pos = Core::SDK::Pointers::pLocalPlayer->GetPos();
							Locations.push_back({ CustomName, Pos, true });
							memset(CustomName, 0, sizeof(CustomName));
						}
					}

					if (Location.IsCustom)
					{
						if (Custom::Button(
							Utils::Translate(xorstr("Delete Selected Location")),
							ImVec2(-1, g_MenuInfo.ButtonHeight),
							0))
						{
							Locations.erase(Locations.begin() + SelectedIndex);
							SelectedIndex = 0;
						}
					}
				}
				Custom::EndCard();
			}
			break;
			case 3: //Weapons
				Custom::BeginCard( Utils::Translate( xorstr( "Weapon Modifiers" ) ) );
				{
					{
						g_Config.Player->RecoilValue = Core::SDK::Pointers::pLocalPlayer->GetWeaponManager()->GetRecoil();
						static const float RecoilBackup = g_Config.Player->RecoilValue;

						g_Config.Player->SpreadValue = Core::SDK::Pointers::pLocalPlayer->GetWeaponManager()->GetSpread();
						static const float SpreadBackup = g_Config.Player->RecoilValue;

						g_Config.Player->RangeValue = Core::SDK::Pointers::pLocalPlayer->GetWeaponManager()->GetRange();
						static const float RangeBackup = g_Config.Player->RangeValue;

						g_Config.Player->DamageValue = Core::SDK::Pointers::pLocalPlayer->GetWeaponManager()->GetDamage();
						static const float DamageBackup = g_Config.Player->DamageValue;

						auto sizes = Core::SDK::Pointers::pLocalPlayer->GetWeaponManager()->GetSizes();
						g_Config.Player->SizeValue = sizes.first;
						static float SizeValueBackup = g_Config.Player->SizeValue;

						if (Custom::CheckBoxCfg(Utils::Translate( xorstr( "Recoil Control" ) ), &g_Config.Player->NoRecoilEnabled, xorstr( "Remove ou reduz o coice da arma." ), [&]() {

							if (ImGui::SliderFloat(Utils::Translate( xorstr( "Value" ) ), &g_Config.Player->RecoilValue, 0.0f, 10.f, xorstr("%1.2fx"))) {
								if (g_Config.Player->NoRecoilEnabled) {
									Core::SDK::Pointers::pLocalPlayer->GetWeaponManager()->SetRecoil(g_Config.Player->RecoilValue);
								}
							}
							}))
						{
							if (!g_Config.Player->NoRecoilEnabled) {
								Core::SDK::Pointers::pLocalPlayer->GetWeaponManager()->SetRecoil(RecoilBackup);
							}
						}

						if (Custom::CheckBoxCfg(Utils::Translate( xorstr( "Spread Control" ) ), &g_Config.Player->NoSpreadEnabled, xorstr( "Remove ou reduz a dispersão do tiro." ), [&]() {
							if (ImGui::SliderFloat(Utils::Translate( xorstr( "Value" ) ), &g_Config.Player->SpreadValue, 0.0f, 10.f, xorstr("%1.2fx"))) {
								if (g_Config.Player->NoSpreadEnabled) {
									Core::SDK::Pointers::pLocalPlayer->GetWeaponManager()->SetSpread(g_Config.Player->SpreadValue);
								}
							}
							}))
						{
							if (!g_Config.Player->NoSpreadEnabled) {
								Core::SDK::Pointers::pLocalPlayer->GetWeaponManager()->SetSpread(SpreadBackup);
							}
						}

						if (Custom::CheckBoxCfg(Utils::Translate( xorstr( "Range Control" ) ), &g_Config.Player->Range, xorstr( "Altera o alcance da arma." ), [&]() {
							if (ImGui::SliderFloat(Utils::Translate( xorstr( "Value" ) ), &g_Config.Player->RangeValue, 0, 800.0f, xorstr("%1.2fx"))) {
								if (g_Config.Player->Range) {
									Core::SDK::Pointers::pLocalPlayer->GetWeaponManager()->SetRange(g_Config.Player->RangeValue);
								}
							}
							}))
						{
							if (!g_Config.Player->Range) {
								Core::SDK::Pointers::pLocalPlayer->GetWeaponManager()->SetRange(RangeBackup);
							}
						}

						if (Custom::CheckBoxCfg(Utils::Translate( xorstr( "Damage Control" ) ), &g_Config.Player->Damage, xorstr( "Altera o dano da arma." ), [&]() {
							if (ImGui::SliderFloat(Utils::Translate( xorstr( "Value" ) ), &g_Config.Player->DamageValue, 0, 800, xorstr("%1.2fx"))) {
								if (g_Config.Player->Damage) {
									Core::SDK::Pointers::pLocalPlayer->GetWeaponManager()->SetDamage(g_Config.Player->DamageValue);
								}
							}
							}))
						{
							if (!g_Config.Player->Damage) {
								Core::SDK::Pointers::pLocalPlayer->GetWeaponManager()->SetDamage(DamageBackup);
							}
						}

						if (Custom::CheckBoxCfg(Utils::Translate( xorstr( "Size Control" ) ), &g_Config.Player->Size, xorstr( "Altera o tamanho do projétil." ), [&]() {
							if (ImGui::SliderFloat(Utils::Translate( xorstr( "Value" ) ), &g_Config.Player->SizeValue, 0.5f, 80, xorstr("%1.2fx"))) {
								if (g_Config.Player->Size) {
									Core::SDK::Pointers::pLocalPlayer->GetWeaponManager()->SetSize(g_Config.Player->SizeValue);
								}
							}
							}))
						{
							if (!g_Config.Player->Size) {
								Core::SDK::Pointers::pLocalPlayer->GetWeaponManager()->SetSize(SizeValueBackup);
							}
						}

						// Rapid Fire usa os modificadores de animacao confirmados no
						// IDA. O loop global mantem o patch independente desta aba.
						if (Custom::CheckBoxCfg(Utils::Translate( xorstr( "Rapid Fire" ) ), &g_Config.Player->RapidFireEnabled, xorstr( "Aumenta a cadencia de tiro. 1.0x = normal, 20.0x = full-auto absurdo." ), [&]() {
							ImGui::SliderFloat(Utils::Translate( xorstr( "Speed" ) ), &g_Config.Player->RapidFireValue, 1.0f, 20.f, xorstr("%1.1fx"));
							}))
						{
							// Desligar restaura imediatamente as instrucoes originais.
							if (!g_Config.Player->RapidFireEnabled)
								Core::Features::Exploits::DisableRapidFire();
						}

						Custom::CheckBox(Utils::Translate( xorstr( "Infinite Ammo" ) ), &g_Config.Player->InfiniteAmmoEnabled, xorstr( "Munição infinita, sem precisar recarregar." ) );
					}
				}
				Custom::EndCard( );

				Custom::BeginCard( Utils::Translate( xorstr( "Ammunition Actions" ) ) );
				{
					{

						static int Ammo = 0;
						ImGui::SliderInt( Utils::Translate( xorstr( "Ammo Value" ) ), &Ammo, 0, 250 );

						if ( Custom::Button( Utils::Translate( xorstr( "Add Ammo" ) ), ImVec2( -1, g_MenuInfo.ButtonHeight ), 0, true, Utils::Translate( xorstr( "That function can be detected by server AntiCheat" ) ), ICON_FA_TRIANGLE_EXCLAMATION ) )
						{
							CWeaponInfo * pCWeaponInfo = Core::SDK::Pointers::pLocalPlayer->GetWeaponManager( )->GetWeaponInfo( );
							if ( pCWeaponInfo )
							{
								//48 8b 43 ? 48 83 c4 ? 5b c3 cc 40 53
								uintptr_t Lixo = Mem.FindSignature(
									{ 0x48, 0x8b, 0x43, 0x00, 0x48, 0x83, 0xc4, 0x00, 0x5b, 0xc3, 0xcc, 0x40, 0x53 }
								);

								uintptr_t AmmoInfo = Mem.Read<uintptr_t>( ( uintptr_t ) pCWeaponInfo + Mem.Read<BYTE>( Lixo + 3 ) );

								if ( AmmoInfo )
								{
									uintptr_t pAmmoCount = Mem.Read<uintptr_t>( ( uintptr_t ) AmmoInfo + 0x8 );
									if ( pAmmoCount )
									{

										uintptr_t CurrentAmmoAddr = Mem.Read<uintptr_t>( pAmmoCount + 0x0 ) + 0x18;
										int CurrentAmmo = Mem.Read<int>( CurrentAmmoAddr );
										
										Mem.Write( CurrentAmmoAddr, CurrentAmmo + Ammo );
									}
								}
								else
								{
									std::thread AmmoAlert( [ ] ( ) {  NotifyManager::Send( xorstr( "Get a weapon first!" ), 4000 );  } );
									AmmoAlert.detach( );
								}
							}
						}

					}
				}
				Custom::EndCard( );
				break;
			case 4: //HandlingEditor
				{
					bool InVehicle = Core::SDK::Pointers::pLocalPlayer->InVehicle( );

					if ( !InVehicle ) 
						iSubTabCount = 1;

					Custom::BeginCard( Utils::Translate( xorstr( "Handling Editor" ) ), 2 );
					{
						ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 8, 3 ) );
						{
							Custom::SectionLabel( Utils::Translate( xorstr( "Voltar" ) ) );
							ImGui::PushFont( g_Variables.FontAwesomeSolidSmall );
							{
								ImGui::SameLine( );
								ImGui::TextColored( g_Col.Base, ICON_FA_REPLY );
								if ( ImGui::IsItemClicked( ) )
								{
									iSubTabCount = 1;
								}
							}
							ImGui::PopFont( );
						}
						ImGui::PopStyleVar( );

						ImGui::Dummy( ImVec2( 0.f, 3.f ) );

						if ( ImGui::SliderFloat( Utils::Translate( xorstr( "Aceleração" ) ), &Core::Features::Exploits::g_HandlingEditor.fAcceleration, 0.0f, 400.f, xorstr( "%1.1f" ) ) )
						{
							Core::Features::Exploits::g_HandlingEditor.ApplyHandlingValues( );
						}

						if (ImGui::SliderFloat(Utils::Translate( xorstr( "Força de freio" ) ), &Core::Features::Exploits::g_HandlingEditor.fBreakForce, 0.0f, 100.f, xorstr("%1.1f")))
						{
							Core::Features::Exploits::g_HandlingEditor.ApplyHandlingValues();
						}

						if ( ImGui::SliderFloat( Utils::Translate( xorstr( "Aderência mínima" ) ), &Core::Features::Exploits::g_HandlingEditor.fTractionCurveMin, 0.0f, 100.f, xorstr( "%1.1f" ) ) )
						{
							Core::Features::Exploits::g_HandlingEditor.ApplyHandlingValues( );
						}

					}
					Custom::EndCard( );
				}
				break;
			case 5: //WeaponSpawn
				Custom::BeginCard( Utils::Translate( xorstr( "Weapon Spawn" ) ), 2 );
				{
					ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 8, 3 ) );
					{
						Custom::SectionLabel( Utils::Translate( xorstr( "Here you can spawn weapon for you" ) ) );
						ImGui::PushFont( g_Variables.FontAwesomeSolidSmall );
						{
							ImGui::SameLine( );
							ImGui::TextColored( g_Col.Base, ICON_FA_REPLY );
							if ( ImGui::IsItemClicked( ) ) {
								iSubTabCount = 3;
							}
						}
						ImGui::PopFont( );
					}
					ImGui::PopStyleVar( );

					ImGui::Dummy( ImVec2( 0.f, 3.f ) );

					{
						struct WeaponSpawn_t {
							std::string Name;
							unsigned int WeaponHash;
							GLTexture Picture;
						};

						std::vector<WeaponSpawn_t> WeaponList = {
							// Melee Weapons
							WeaponSpawn_t( xorstr( "Dagger" ), 0x92A27487, g_Variables.FiveM_WeaponsPic.weapon_dagger ),
							WeaponSpawn_t( xorstr( "Bat" ), 0x958A4A8F, g_Variables.FiveM_WeaponsPic.weapon_bat ),
							WeaponSpawn_t( xorstr( "Bottle" ), 0xF9E6AA4B, g_Variables.FiveM_WeaponsPic.weapon_bottle ),
							WeaponSpawn_t( xorstr( "Crowbar" ), 0x84BD7BFD, g_Variables.FiveM_WeaponsPic.weapon_crowbar ),
							//WeaponSpawn_t( xorstr( "Unarmed" ), 0xA2719263, g_Variables.FiveM_WeaponsPic.weapon_unarmed ),
							WeaponSpawn_t( xorstr( "Flashlight" ), 0x8BB05FD7, g_Variables.FiveM_WeaponsPic.weapon_flashlight ),
							WeaponSpawn_t( xorstr( "GolfClub" ), 0x440E4788, g_Variables.FiveM_WeaponsPic.weapon_golfclub ),
							WeaponSpawn_t( xorstr( "Hammer" ), 0x4E875F73, g_Variables.FiveM_WeaponsPic.weapon_hammer ),
							WeaponSpawn_t( xorstr( "Hatchet" ), 0xF9DCBF2D, g_Variables.FiveM_WeaponsPic.weapon_hatchet ),
							WeaponSpawn_t( xorstr( "Knuckle" ), 0xD8DF3C3C, g_Variables.FiveM_WeaponsPic.weapon_knuckle ),
							WeaponSpawn_t( xorstr( "Knife" ), 0x99B507EA, g_Variables.FiveM_WeaponsPic.weapon_knife ),
							WeaponSpawn_t( xorstr( "Machete" ), 0xDD5DF8D9, g_Variables.FiveM_WeaponsPic.weapon_machete ),
							WeaponSpawn_t( xorstr( "Switchblade" ), 0xDFE37640, g_Variables.FiveM_WeaponsPic.weapon_switchblade ),
							WeaponSpawn_t( xorstr( "Nightstick" ), 0x678B81B1, g_Variables.FiveM_WeaponsPic.weapon_nightstick ),
							WeaponSpawn_t( xorstr( "Wrench" ), 0x19044EE0, g_Variables.FiveM_WeaponsPic.weapon_wrench ),
							WeaponSpawn_t( xorstr( "BattleAxe" ), 0xCD274149, g_Variables.FiveM_WeaponsPic.weapon_battleaxe ),
							WeaponSpawn_t( xorstr( "PoolCue" ), 0x94117305, g_Variables.FiveM_WeaponsPic.weapon_poolcue ),
							WeaponSpawn_t( xorstr( "StoneHatchet" ), 0x3813FC08, g_Variables.FiveM_WeaponsPic.weapon_stone_hatchet ),

							// Handguns
							WeaponSpawn_t( xorstr( "Pistol" ), 0x1B06D571, g_Variables.FiveM_WeaponsPic.weapon_pistol ),
							WeaponSpawn_t( xorstr( "PistolMk2" ), 0xBFE256D4, g_Variables.FiveM_WeaponsPic.weapon_pistol_mk2 ),
							WeaponSpawn_t( xorstr( "CombatPistol" ), 0x5EF9FEC4, g_Variables.FiveM_WeaponsPic.weapon_combatpistol ),
							WeaponSpawn_t( xorstr( "APPistol" ), 0x22D8FE39, g_Variables.FiveM_WeaponsPic.weapon_appistol ),
							WeaponSpawn_t( xorstr( "StunGun" ), 0x3656C8C1, g_Variables.FiveM_WeaponsPic.weapon_stungun ),
							WeaponSpawn_t( xorstr( "Pistol50" ), 0x99AEEB3B, g_Variables.FiveM_WeaponsPic.weapon_pistol50 ),
							WeaponSpawn_t( xorstr( "SNSPistol" ), 0xBFD21232, g_Variables.FiveM_WeaponsPic.weapon_snspistol ),
							WeaponSpawn_t( xorstr( "SNSPistolMk2" ), 0x88374054, g_Variables.FiveM_WeaponsPic.weapon_snspistol_mk2 ),
							WeaponSpawn_t( xorstr( "HeavyPistol" ), 0xD205520E, g_Variables.FiveM_WeaponsPic.weapon_heavypistol ),
							WeaponSpawn_t( xorstr( "VintagePistol" ), 0x83839C4, g_Variables.FiveM_WeaponsPic.weapon_vintagepistol ),
							WeaponSpawn_t( xorstr( "FlareGun" ), 0x47757124, g_Variables.FiveM_WeaponsPic.weapon_flaregun ),
							WeaponSpawn_t( xorstr( "MarksmanPistol" ), 0xDC4DB296, g_Variables.FiveM_WeaponsPic.weapon_marksmanpistol ),
							WeaponSpawn_t( xorstr( "Revolver" ), 0xC1B3C3D1, g_Variables.FiveM_WeaponsPic.weapon_revolver ),
							WeaponSpawn_t( xorstr( "RevolverMk2" ), 0xCB96392F, g_Variables.FiveM_WeaponsPic.weapon_revolver_mk2 ),
							WeaponSpawn_t( xorstr( "DoubleAction" ), 0x97EA20B8, g_Variables.FiveM_WeaponsPic.weapon_doubleaction ),
							WeaponSpawn_t( xorstr( "RayPistol" ), 0xAF3696A1, g_Variables.FiveM_WeaponsPic.weapon_raypistol ),
							WeaponSpawn_t( xorstr( "CeramicPistol" ), 0x2B5EF5EC, g_Variables.FiveM_WeaponsPic.weapon_ceramicpistol ),
							WeaponSpawn_t( xorstr( "NavyRevolver" ), 0x917F6C8C, g_Variables.FiveM_WeaponsPic.weapon_navyrevolver ),

							// SMGs
							WeaponSpawn_t( xorstr( "MicroSMG" ), 0x13532244, g_Variables.FiveM_WeaponsPic.weapon_microsmg ),
							WeaponSpawn_t( xorstr( "SMG" ), 0x2BE6766B, g_Variables.FiveM_WeaponsPic.weapon_smg ),
							WeaponSpawn_t( xorstr( "SMGMk2" ), 0x78A97CD0, g_Variables.FiveM_WeaponsPic.weapon_smg_mk2 ),
							WeaponSpawn_t( xorstr( "AssaultSMG" ), 0xEFE7E2DF, g_Variables.FiveM_WeaponsPic.weapon_assaultsmg ),
							WeaponSpawn_t( xorstr( "CombatPDW" ), 0xA3D4D34, g_Variables.FiveM_WeaponsPic.weapon_combatpdw ),
							WeaponSpawn_t( xorstr( "MachinePistol" ), 0xDB1AA450, g_Variables.FiveM_WeaponsPic.weapon_machinepistol ),
							WeaponSpawn_t( xorstr( "MiniSMG" ), 0xBD248B55, g_Variables.FiveM_WeaponsPic.weapon_minismg ),
							WeaponSpawn_t( xorstr( "RayCarbine" ), 0x476BF155, g_Variables.FiveM_WeaponsPic.weapon_raycarbine ),

							// Shotguns
							WeaponSpawn_t( xorstr( "PumpShotgun" ), 0x1D073A89, g_Variables.FiveM_WeaponsPic.weapon_pumpshotgun ),
							WeaponSpawn_t( xorstr( "PumpShotgunMk2" ), 0x555AF99A, g_Variables.FiveM_WeaponsPic.weapon_pumpshotgun_mk2 ),
							WeaponSpawn_t( xorstr( "SawnoffShotgun" ), 0x7846A318, g_Variables.FiveM_WeaponsPic.weapon_sawnoffshotgun ),
							WeaponSpawn_t( xorstr( "AssaultShotgun" ), 0xE284C527, g_Variables.FiveM_WeaponsPic.weapon_assaultshotgun ),
							WeaponSpawn_t( xorstr( "BullpupShotgun" ), 0x9D61E50F, g_Variables.FiveM_WeaponsPic.weapon_bullpupshotgun ),
							WeaponSpawn_t( xorstr( "Musket" ), 0xA89CB99E, g_Variables.FiveM_WeaponsPic.weapon_musket ),
							WeaponSpawn_t( xorstr( "HeavyShotgun" ), 0x3AABBBAA, g_Variables.FiveM_WeaponsPic.weapon_heavyshotgun ),
							WeaponSpawn_t( xorstr( "DBShotgun" ), 0xEF951FBB, g_Variables.FiveM_WeaponsPic.weapon_dbshotgun ),
							WeaponSpawn_t( xorstr( "AutoShotgun" ), 0x12E82D3D, g_Variables.FiveM_WeaponsPic.weapon_autoshotgun ),

							// Assault Rifles
							WeaponSpawn_t( xorstr( "AssaultRifle" ), 0xBFEFFF6D, g_Variables.FiveM_WeaponsPic.weapon_assaultrifle ),
							WeaponSpawn_t( xorstr( "AssaultRifleMk2" ), 0x394F415C, g_Variables.FiveM_WeaponsPic.weapon_assaultrifle_mk2 ),
							WeaponSpawn_t( xorstr( "CarbineRifle" ), 0x83BF0278, g_Variables.FiveM_WeaponsPic.weapon_carbinerifle ),
							WeaponSpawn_t( xorstr( "CarbineRifleMk2" ), 0xFAD1F1C9, g_Variables.FiveM_WeaponsPic.weapon_carbinerifle_mk2 ),
							WeaponSpawn_t( xorstr( "AdvancedRifle" ), 0xAF113F99, g_Variables.FiveM_WeaponsPic.weapon_advancedrifle ),
							WeaponSpawn_t( xorstr( "SpecialCarbine" ), 0xC0A3098D, g_Variables.FiveM_WeaponsPic.weapon_specialcarbine ),
							WeaponSpawn_t( xorstr( "SpecialCarbineMk2" ), 0x969C3D67, g_Variables.FiveM_WeaponsPic.weapon_specialcarbine_mk2 ),
							WeaponSpawn_t( xorstr( "BullpupRifle" ), 0x7F229F94, g_Variables.FiveM_WeaponsPic.weapon_bullpuprifle ),
							WeaponSpawn_t( xorstr( "BullpupRifleMk2" ), 0x84D6FAFD, g_Variables.FiveM_WeaponsPic.weapon_bullpuprifle_mk2 ),
							WeaponSpawn_t( xorstr( "CompactRifle" ), 0x624FE830, g_Variables.FiveM_WeaponsPic.weapon_compactrifle ),

							// Machine Guns
							WeaponSpawn_t( xorstr( "MG" ), 0x9D07F764, g_Variables.FiveM_WeaponsPic.weapon_mg ),
							WeaponSpawn_t( xorstr( "CombatMG" ), 0x7FD62962, g_Variables.FiveM_WeaponsPic.weapon_combatmg ),
							WeaponSpawn_t( xorstr( "CombatMGMk2" ), 0xDBBD7280, g_Variables.FiveM_WeaponsPic.weapon_combatmg_mk2 ),
							WeaponSpawn_t( xorstr( "Gusenberg" ), 0x61012683, g_Variables.FiveM_WeaponsPic.weapon_gusenberg ),

							// Sniper Rifles
							WeaponSpawn_t( xorstr( "SniperRifle" ), 0x5FC3C11, g_Variables.FiveM_WeaponsPic.weapon_sniperrifle ),
							WeaponSpawn_t( xorstr( "HeavySniper" ), 0xC472FE2, g_Variables.FiveM_WeaponsPic.weapon_heavysniper ),
							WeaponSpawn_t( xorstr( "HeavySniperMk2" ), 0xA914799, g_Variables.FiveM_WeaponsPic.weapon_heavysniper_mk2 ),
							WeaponSpawn_t( xorstr( "MarksmanRifle" ), 0xC734385A, g_Variables.FiveM_WeaponsPic.weapon_marksmanrifle ),
							WeaponSpawn_t( xorstr( "MarksmanRifleMk2" ), 0x6A6C02E0, g_Variables.FiveM_WeaponsPic.weapon_marksmanrifle_mk2 ),

							// Heavy Weapons
							WeaponSpawn_t( xorstr( "RPG" ), 0xB1CA77B1, g_Variables.FiveM_WeaponsPic.weapon_rpg ),
							WeaponSpawn_t( xorstr( "GrenadeLauncher" ), 0xA284510B, g_Variables.FiveM_WeaponsPic.weapon_grenadelauncher ),
							WeaponSpawn_t( xorstr( "GrenadeLauncherSmoke" ), 0x4DD2DC56, g_Variables.FiveM_WeaponsPic.weapon_grenadelauncher_smoke ),
							WeaponSpawn_t( xorstr( "Minigun" ), 0x42BF8A85, g_Variables.FiveM_WeaponsPic.weapon_minigun ),
							WeaponSpawn_t( xorstr( "Firework" ), 0x7F7497E5, g_Variables.FiveM_WeaponsPic.weapon_firework ),
							WeaponSpawn_t( xorstr( "Railgun" ), 0x6D544C99, g_Variables.FiveM_WeaponsPic.weapon_railgun ),
							WeaponSpawn_t( xorstr( "HomingLauncher" ), 0x63AB0442, g_Variables.FiveM_WeaponsPic.weapon_hominglauncher ),
							WeaponSpawn_t( xorstr( "CompactLauncher" ), 0x781FE4A, g_Variables.FiveM_WeaponsPic.weapon_compactlauncher ),
							WeaponSpawn_t( xorstr( "RayMinigun" ), 0xB62D1F67, g_Variables.FiveM_WeaponsPic.weapon_rayminigun ),

							// Throwables
							WeaponSpawn_t( xorstr( "Grenade" ), 0x93E220BD, g_Variables.FiveM_WeaponsPic.weapon_grenade ),
							WeaponSpawn_t( xorstr( "BZGas" ), 0xA0973D5E, g_Variables.FiveM_WeaponsPic.weapon_bzgas ),
							WeaponSpawn_t( xorstr( "SmokeGrenade" ), 0xFDBC8A50, g_Variables.FiveM_WeaponsPic.weapon_smokegrenade ),
							WeaponSpawn_t( xorstr( "Flare" ), 0x497FACC3, g_Variables.FiveM_WeaponsPic.weapon_flare ),
							WeaponSpawn_t( xorstr( "Molotov" ), 0x24B17070, g_Variables.FiveM_WeaponsPic.weapon_molotov ),
							WeaponSpawn_t( xorstr( "StickyBomb" ), 0x2C3731D9, g_Variables.FiveM_WeaponsPic.weapon_stickybomb ),
							WeaponSpawn_t( xorstr( "ProximityMine" ), 0xAB564B93, g_Variables.FiveM_WeaponsPic.weapon_proxmine ),
							WeaponSpawn_t( xorstr( "Snowball" ), 0x787F0BB, g_Variables.FiveM_WeaponsPic.weapon_snowball ),
							WeaponSpawn_t( xorstr( "PipeBomb" ), 0xBA45E8B8, g_Variables.FiveM_WeaponsPic.weapon_pipebomb ),
							WeaponSpawn_t( xorstr( "Ball" ), 0x23C9F95C, g_Variables.FiveM_WeaponsPic.weapon_ball ),

							// Miscellaneous
							WeaponSpawn_t( xorstr( "PetrolCan" ), 0x34A67B97, g_Variables.FiveM_WeaponsPic.weapon_petrolcan ),
							WeaponSpawn_t( xorstr( "FireExtinguisher" ), 0x60EC506, g_Variables.FiveM_WeaponsPic.weapon_fireextinguisher ),
							WeaponSpawn_t( xorstr( "Parachute" ), 0xFBAB5776, g_Variables.FiveM_WeaponsPic.gadget_parachute ),
							WeaponSpawn_t( xorstr( "HazardCan" ), 0xBA536372, g_Variables.FiveM_WeaponsPic.weapon_hazardcan )
						};

						int count = 0;
						//ImGui::SetCursorPosX( 6 );
						for ( auto Weapon : WeaponList )
						{

							if ( Weapon.Name.empty( ) || Weapon.Picture == nullptr ) {
								continue;
							}

							if ( Custom::WeaponButtonHeld( Weapon.Picture, Weapon.Name.c_str( ), 0 ) ) {
								std::string Msg = Weapon.Name + Utils::Translate( xorstr( " has been spawned" ) );
								std::thread NotifyThread( NotifyManager::Send, Msg, 4000 );
								NotifyThread.detach( );
							}

							count++;

							if ( count % 5 != 0 ) {
								ImGui::SameLine( );
							}
							else {
								count = 0;
								//ImGui::SetCursorPosX( 6 );
							}
						}
					}
				}
				Custom::EndCard( );
				break;
			default:
				break;
			}

		}
		ImGui::PopStyleVar( );

	}
}
