#pragma once
#include <Includes/includes.hpp>
#include <windows.h>
#include <iostream>
#include <thread>
#include <Core/Features/Esp/WeaponNames.hpp>

using namespace std;

namespace World {

	// Subtab selection is now driven by the sidebar (Custom::SubTabItem list rendered in Gui.cpp).
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
			const float InfoHeight = 165.f;
			const float ActionsHeight = g_MenuInfo.CardHeight - InfoHeight - g_MenuInfo.CardGap;

			switch ( iSubTab )
			{
			case 0: //Players
			{
				static int SeletedIndex = -1;
				static int PreviousSelectedIndex = -1;
				static bool bSpectating = false;
				static uintptr_t SpectatedPed = 0;
				
				// Se mudou a seleção e estava spectando, desativa
				if ( PreviousSelectedIndex != SeletedIndex && bSpectating )
				{
					Core::SDK::Pointers::pLocalPlayer->SpectatePed( 0, false );
					bSpectating = false;
					SpectatedPed = 0;
				}
				PreviousSelectedIndex = SeletedIndex;
				
				bool IsSelected;

				Custom::BeginCard( Utils::Translate( xorstr( "Players" ) ) );
				{
					ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 8, g_MenuInfo.RowSpacing ) );

					// Snapshot sob lock. Copiamos as strings (nao guardamos .c_str() do
					// vetor compartilhado) porque a thread de fundo troca/redimensiona
					// EntityList e liberaria a memoria enquanto o ImGui desenha.
					static std::vector<std::string> PlayerNamesOwned;
					static std::vector<int>         PlayerRealIdx;
					static ImVector<const char*>    PlayerNameList;

					PlayerNamesOwned.clear( );
					PlayerRealIdx.clear( );
					{
						std::lock_guard<std::mutex> lock( Core::Threads::g_EntityList.EntityListMutex );
						for ( int i = 0; i < ( int ) Core::SDK::Game::EntityList.size( ); i++ )
						{
							const auto & E = Core::SDK::Game::EntityList[ i ];
							if ( E.Ped == Core::SDK::Pointers::pLocalPlayer ) continue;
							if ( E.PedType != 2 ) continue;
							PlayerNamesOwned.push_back( E.NetworkInfo.UserName );
							PlayerRealIdx.push_back( i );
						}
					}
					// so agora, com o vetor estavel, tomamos os ponteiros
					PlayerNameList.clear( );
					for ( size_t i = 0; i < PlayerNamesOwned.size( ); i++ )
						PlayerNameList.push_back( PlayerNamesOwned[ i ].c_str( ) );

					Custom::PlayerSearchBar( xorstr( "##PlayerSearch" ), PlayerNameList, SeletedIndex );

					ImGui::Spacing( );

					for ( int i = 0; i < Custom::g_PlayerSearch.FilteredIndices.size( ); i++ )
					{
						int PlayerIdx = Custom::g_PlayerSearch.FilteredIndices[ i ];
						if ( PlayerIdx < 0 || PlayerIdx >= ( int ) PlayerNamesOwned.size( ) ) continue;

						const char* PlayerName = PlayerNameList[ PlayerIdx ];
						const int   RealIdx    = PlayerRealIdx[ PlayerIdx ];

						bool ItemSelected = ( RealIdx == SeletedIndex );
						if ( ImGui::ListSelectable( PlayerName, &ItemSelected ) )
						{
							SeletedIndex = RealIdx;   // indice real do EntityList, nao o da lista filtrada
							Custom::g_PlayerSearch.SelectedIndex = PlayerIdx;
						}
					}

					ImGui::PopStyleVar( );
				}
				Custom::EndCard( );

				Custom::BeginCard( Utils::Translate( xorstr( "Info" ) ), 1, InfoHeight, 0.f, false );
				{
					{
						if ( SeletedIndex == -1 ) {
							ImGui::TextColored( ImColor( g_Col.SecundaryText ), Utils::Translate( xorstr( "Select Player" ) ) );
						}
						else
						{
							Core::SDK::Game::EntityStruct SelectedPed;
							{
								std::lock_guard<std::mutex> lock( Core::Threads::g_EntityList.EntityListMutex );
								if ( SeletedIndex < 0 || SeletedIndex >= ( int ) Core::SDK::Game::EntityList.size( ) )
									{ SeletedIndex = -1; }
								else
									{ SelectedPed = Core::SDK::Game::EntityList[ SeletedIndex ]; }
							}
							if ( SeletedIndex == -1 ) { ImGui::TextColored( ImColor( g_Col.SecundaryText ), Utils::Translate( xorstr( "Select Player" ) ) ); }
							else
							{
							ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 6, 2 ) );

							ImGui::TextColored( ImColor( g_Col.FeaturesText ), Utils::Translate( xorstr( "Name:" ) ) ); ImGui::SameLine( );
							ImGui::TextColored( ImColor( g_Col.SecundaryText ), SelectedPed.NetworkInfo.UserName.c_str());
							ImGui::TextColored( ImColor( g_Col.FeaturesText ), Utils::Translate( xorstr( "Id:" ) ) ); ImGui::SameLine( );
							ImGui::TextColored( ImColor( g_Col.SecundaryText ), std::to_string( SelectedPed.Id ).c_str( ) );
							ImGui::TextColored( ImColor( g_Col.FeaturesText ), Utils::Translate( xorstr( "Distance:" ) ) ); ImGui::SameLine( );
							ImGui::TextColored( ImColor( g_Col.SecundaryText ), std::to_string( SelectedPed.Distance ).c_str( ) );
							ImGui::TextColored( ImColor( g_Col.FeaturesText ), Utils::Translate( xorstr( "Weapon:" ) ) ); ImGui::SameLine( );
							{
								// Mesma resolucao do ESP: tabela do wiki pelo hash, e a
								// string do jogo so como reserva.
								std::string W = Core::Features::WeaponNames::FromHash( SelectedPed.WeaponHash );
								if ( W.empty( ) ) W = SelectedPed.WeaponName;
								if ( W.empty( ) ) W = xorstr( "UNARMED" );
								ImGui::TextColored( ImColor( g_Col.SecundaryText ), W.c_str( ) );
							}
							ImGui::TextColored( ImColor( g_Col.FeaturesText ), Utils::Translate( xorstr( "Friend:" ) ) ); ImGui::SameLine( );
							ImGui::TextColored( ImColor( g_Col.SecundaryText ), SelectedPed.IsFriend ? Utils::Translate( xorstr( "True" ) ) : Utils::Translate( xorstr( "False" ) ) );

							ImGui::PopStyleVar( );
							}
						}
					}
				}
				Custom::EndCard( );

				Custom::BeginCard( Utils::Translate( xorstr( "Actions" ) ), 1, ActionsHeight, InfoHeight + g_MenuInfo.CardGap );
				{
					{
						if ( SeletedIndex == -1 ) {
							ImGui::TextColored( ImColor( g_Col.SecundaryText ), Utils::Translate( xorstr( "Select Player" ) ) );
						}
						else
						{
							Core::SDK::Game::EntityStruct SelectedPed;
							bool PedOk = false;
							{
								std::lock_guard<std::mutex> lock( Core::Threads::g_EntityList.EntityListMutex );
								if ( SeletedIndex >= 0 && SeletedIndex < ( int ) Core::SDK::Game::EntityList.size( ) )
								{ SelectedPed = Core::SDK::Game::EntityList[ SeletedIndex ]; PedOk = true; }
							}
							if ( !PedOk ) { ImGui::TextColored( ImColor( g_Col.SecundaryText ), Utils::Translate( xorstr( "Select Player" ) ) ); }
							else {

							ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 8, g_MenuInfo.RowSpacing ) );
							bool Friend = SelectedPed.IsFriend;
							if ( Custom::CheckBox( Utils::Translate( xorstr( "Friend" ) ), &Friend, xorstr( "Marca esse jogador como amigo. Amigos são ignorados pelo aimbot e pelo ESP de inimigos." ) ) )
							{
								Core::SDK::Game::FriendMap[ SelectedPed.Id ] = Friend;
							}

							ImGui::Spacing( );

							if ( Custom::Button( Utils::Translate( xorstr( "Teleport" ) ), ImVec2( -1, g_MenuInfo.ButtonHeight ), 0 ) )
							{
								Core::SDK::Pointers::pLocalPlayer->SetPos( SelectedPed.Pos );
							}
							
							if ( Custom::Button( Utils::Translate( bSpectating ? xorstr( "Stop Spectate" ) : xorstr( "Spectate" ) ), ImVec2( -1, g_MenuInfo.ButtonHeight ), 0 ) )
							{
								bSpectating = !bSpectating;
								if ( bSpectating )
								{
									SpectatedPed = (uintptr_t)SelectedPed.Ped;
									Core::SDK::Pointers::pLocalPlayer->SpectatePed( SpectatedPed, true );
								}
								else
								{
									Core::SDK::Pointers::pLocalPlayer->SpectatePed( 0, false );
									SpectatedPed = 0;
								}
							}

							if (Custom::Button(Utils::Translate( xorstr("Copy Outfit") ) , ImVec2(-1, 30), 0))
							{
							uintptr_t selectedPedPtr = reinterpret_cast<uintptr_t>(SelectedPed.Ped);
                            uintptr_t localPedPtr = reinterpret_cast<uintptr_t>(Core::SDK::Pointers::pLocalPlayer);
                            uintptr_t factoryPtr = reinterpret_cast<uintptr_t>(Core::SDK::Pointers::pWorld);
                            uintptr_t copyVarFunc = g_Offsets.m_CopyPedVariation;

                            if (selectedPedPtr && localPedPtr && factoryPtr &&
                                selectedPedPtr != localPedPtr && copyVarFunc)
                            {
                                std::thread([=]() {
                                        auto push8 = [](std::vector<uint8_t>& v, uint8_t b) { v.push_back(b); };
										auto push32 = [](std::vector<uint8_t>& v, uint32_t val) {
											for (int i = 0; i < 4; i++) v.push_back((val >> (i * 8)) & 0xFF);
										};
										auto push64 = [](std::vector<uint8_t>& v, uint64_t val) {
											for (int i = 0; i < 8; i++) v.push_back((val >> (i * 8)) & 0xFF);
										};
										auto pushJunk = [&](std::vector<uint8_t>& v) {
											int n = 1 + (std::rand() % 3);
											for (int j = 0; j < n; j++) {
												switch (std::rand() % 8) {
												case 0: push8(v, 0x90); break;
												case 1: push8(v, 0x48); push8(v, 0x87); push8(v, 0xC0); break;
												case 2: push8(v, 0x66); push8(v, 0x90); break;
												case 3: push8(v, 0x0F); push8(v, 0x1F); push8(v, 0x00); break;
												case 4: push8(v, 0x48); push8(v, 0x87); push8(v, 0xDB); break;
												case 5: push8(v, 0x0F); push8(v, 0x1F); push8(v, 0x40); push8(v, 0x00); break;
												case 6: push8(v, 0x48); push8(v, 0x8D); push8(v, 0x36); break;
												default: push8(v, 0x90); break;
												}
											}
										};

										std::vector<uint8_t> sc;
										sc.reserve(0x100);
										pushJunk(sc);
										// sub rsp, 0x28
										push8(sc, 0x48); push8(sc, 0x83); push8(sc, 0xEC); push8(sc, 0x28);
										pushJunk(sc);
										// mov rcx, factoryPtr (this = CPedFactory)
										push8(sc, 0x48); push8(sc, 0xB9); push64(sc, factoryPtr);
										pushJunk(sc);
										// mov rdx, selectedPedPtr (source)
										push8(sc, 0x48); push8(sc, 0xBA); push64(sc, selectedPedPtr);
										pushJunk(sc);
										// mov r8, localPedPtr (dest)
										push8(sc, 0x49); push8(sc, 0xB8); push64(sc, localPedPtr);
										pushJunk(sc);
                                        // mov r9d, 0 (stallForAssets = false), as used by
                                        // CPedFactory::ClonePedToTarget in the game binary.
                                        push8(sc, 0x45); push8(sc, 0x31); push8(sc, 0xC9);
										pushJunk(sc);
										// mov rax, copyVarFunc
										push8(sc, 0x48); push8(sc, 0xB8); push64(sc, copyVarFunc);
										// call rax
										push8(sc, 0xFF); push8(sc, 0xD0);
										// add rsp, 0x28
										push8(sc, 0x48); push8(sc, 0x83); push8(sc, 0xC4); push8(sc, 0x28);
										// ret
										push8(sc, 0xC3);

										uintptr_t cave = Core::Mem.CreateDataCave(0x200);
										if (cave) {
											Mem.execShellcode(cave, sc.data(), sc.size());
											Core::Mem.FreeCave(cave, sc.size());
											NotifyManager::Send(xorstr("Outfit copied!"), 3000);
										}
									}).detach();
								}
							}

							ImGui::PopStyleVar( );
							}
						}
					}
				}
				Custom::EndCard( );
			}
			break;
			case 1: //Vehicles
			{
				static int SeletedIndex = -1;

				Custom::BeginCard( Utils::Translate( xorstr( "Vehicles" ) ) );
				{
					ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 8, g_MenuInfo.RowSpacing ) );

					// Mesmo problema dos players: o .c_str() apontava para strings de um
					// vetor que a thread de fundo substitui (std::move) -> ponteiro pendurado.
					static std::vector<std::string> VehicleNamesOwned;
					static ImVector<const char*>    VehicleNameList;

					VehicleNamesOwned.clear( );
					{
						std::lock_guard<std::mutex> lock( Core::SDK::Game::VehicleListMutex );
						for ( int i = 0; i < ( int ) Core::SDK::Game::VehicleList.size( ); i++ )
							VehicleNamesOwned.push_back( Core::SDK::Game::VehicleList[ i ].Name );
					}
					VehicleNameList.clear( );
					for ( size_t i = 0; i < VehicleNamesOwned.size( ); i++ )
						VehicleNameList.push_back( VehicleNamesOwned[ i ].c_str( ) );

							Custom::PlayerSearchBar(xorstr( "##VehicleSearch" ), VehicleNameList, SeletedIndex );

					ImGui::Spacing( );

					for ( int i = 0; i < Custom::g_PlayerSearch.FilteredIndices.size( ); i++ )
					{
						int VehicleIdx = Custom::g_PlayerSearch.FilteredIndices[ i ];
						if ( VehicleIdx < 0 || VehicleIdx >= ( int ) VehicleNamesOwned.size( ) ) continue;
						const char* VehicleName = VehicleNameList[ VehicleIdx ];

						bool ItemSelected = ( VehicleIdx == SeletedIndex );
						if ( ImGui::ListSelectable( VehicleName, &ItemSelected ) )
						{
							SeletedIndex = VehicleIdx;
							Custom::g_PlayerSearch.SelectedIndex = VehicleIdx;
						}
					}

					ImGui::PopStyleVar( );
				}
				Custom::EndCard( );

				Custom::BeginCard( Utils::Translate( xorstr( "Info" ) ), 1, InfoHeight, 0.f, false );
				{
					{
						if ( SeletedIndex == -1 ) {
							ImGui::TextColored( ImColor( g_Col.SecundaryText ), Utils::Translate( xorstr( "Select Vehicle" ) ) );
						}
						else
						{
							Core::SDK::Game::VehicleStructure V;
							bool Ok = false;
							{
								std::lock_guard<std::mutex> lock( Core::SDK::Game::VehicleListMutex );
								if ( SeletedIndex >= 0 && SeletedIndex < ( int ) Core::SDK::Game::VehicleList.size( ) )
								{ V = Core::SDK::Game::VehicleList[ SeletedIndex ]; Ok = true; }
								else { SeletedIndex = -1; }
							}
							if ( !Ok ) { ImGui::TextColored( ImColor( g_Col.SecundaryText ), Utils::Translate( xorstr( "Select Vehicle" ) ) ); }
							else
							{
								ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 6, 2 ) );

								ImGui::TextColored( ImColor( g_Col.FeaturesText ), Utils::Translate( xorstr( "Name:" ) ) ); ImGui::SameLine( );
								ImGui::TextColored( ImColor( g_Col.SecundaryText ), V.Name.c_str( ) );
								ImGui::TextColored( ImColor( g_Col.FeaturesText ), Utils::Translate( xorstr( "Distance:" ) ) ); ImGui::SameLine( );
								ImGui::TextColored( ImColor( g_Col.SecundaryText ), ( std::to_string( ( int ) V.Dist ) + xorstr( "m" ) ).c_str( ) );
								ImGui::TextColored( ImColor( g_Col.FeaturesText ), Utils::Translate( xorstr( "Driver:" ) ) ); ImGui::SameLine( );
								ImGui::TextColored( ImColor( g_Col.SecundaryText ), V.Driver == 0 ? Utils::Translate( xorstr( "Unknown" ) ) : V.Driver->GetPedType( ) != 2 ? Utils::Translate( xorstr( "NPC" ) ) : Core::SDK::Game::GetPedName( V.Driver ).c_str( ) );
								ImGui::TextColored( ImColor( g_Col.FeaturesText ), Utils::Translate( xorstr( "Doors Locked:" ) ) ); ImGui::SameLine( );
								ImGui::TextColored( ImColor( g_Col.SecundaryText ), V.IsLocked ? Utils::Translate( xorstr( "Yes" ) ) : Utils::Translate( xorstr( "No" ) ) );

								ImGui::PopStyleVar( );
							}
						}
					}
				}
				Custom::EndCard( );

				Custom::BeginCard( Utils::Translate( xorstr( "Actions" ) ), 1, ActionsHeight, InfoHeight + g_MenuInfo.CardGap );
				{
					{
						if ( SeletedIndex == -1 ) {
							ImGui::TextColored( ImColor( g_Col.SecundaryText ), Utils::Translate( xorstr( "Select Vehicle" ) ) );
						}
						else
						{
							CVehicle* Veh = nullptr;
							bool VehLocked = false;
							{
								std::lock_guard<std::mutex> lock( Core::SDK::Game::VehicleListMutex );
								if ( SeletedIndex >= 0 && SeletedIndex < ( int ) Core::SDK::Game::VehicleList.size( ) )
								{ Veh = Core::SDK::Game::VehicleList[ SeletedIndex ].Pointer; VehLocked = Core::SDK::Game::VehicleList[ SeletedIndex ].IsLocked; }
							}
							if ( !Veh ) { ImGui::TextColored( ImColor( g_Col.SecundaryText ), Utils::Translate( xorstr( "Select Vehicle" ) ) ); }
							else {
							ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 8, g_MenuInfo.RowSpacing ) );

							if ( VehLocked )
							{
								if ( Custom::Button( Utils::Translate( xorstr( "Unlock" ) ), ImVec2( -1, g_MenuInfo.ButtonHeight ), 0 ) ) {
									Veh->DoorState( eVehicleLockState::VEHICLELOCK_UNLOCKED );
								}
							}
							else {
								if ( Custom::Button( Utils::Translate( xorstr( "Lock" ) ), ImVec2( -1, g_MenuInfo.ButtonHeight ), 0 ) ) {
									Veh->DoorState( eVehicleLockState::VEHICLELOCK_LOCKED );
								}
							}

							if ( Custom::Button( Utils::Translate( xorstr( "Teleport" ) ), ImVec2( -1, g_MenuInfo.ButtonHeight ), 0 ) ) {
								Core::SDK::Pointers::pLocalPlayer->SetPos( Veh->GetPos( ) );
							}

							if (Custom::Button(Utils::Translate(xorstr("Steal Seat") ), ImVec2(-1, 30), 0)) {
								Veh->StealDriverSeat(Core::SDK::Pointers::pLocalPlayer);
							}

							if (Custom::Button(Utils::Translate(xorstr("Freeze Vehicle")), ImVec2(-1, 30), 0)) {
								Veh->Freeze(true);
							}

							if (Custom::Button(Utils::Translate(xorstr("Unfreeze Vehicle")), ImVec2(-1, 30), 0)) {
								Veh->Freeze(false);
							}

							if (Custom::Button(Utils::Translate(xorstr("Teleport to Ocean")), ImVec2(-1, g_MenuInfo.ButtonHeight), 0)) {
								D3DXVECTOR3 Pos = D3DXVECTOR3(-1000.0f, -1000.0f, -100.0f);
								Veh->SetPos(Pos);
							}

							if (Custom::Button(Utils::Translate(xorstr("Break Wheels")), ImVec2(-1, 30), 0)) {
								Veh->BreakWheels(Core::SDK::Pointers::pLocalPlayer);
							}

							ImGui::PopStyleVar( );
							}
						}
					}
				}
				Custom::EndCard( );
			}
			break;
			default:
				break;
			}

		}
		ImGui::PopStyleVar( );

	}
}
