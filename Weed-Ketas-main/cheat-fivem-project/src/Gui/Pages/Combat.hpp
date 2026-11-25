#pragma once
#include <Includes/Includes.hpp>
#include <windows.h>
#include <iostream>
#include <thread>

using namespace std;

namespace Combat {

	// The web panel exposes Combat as three independent subtabs. Keep the
	// selected subtab local to the page so the main sidebar can animate it in
	// exactly the same way as Visuals, Local and Settings.
	static int iSubTabCount = 0;
	static float SubTabAlpha = 0.f;
	static int iSubTab = 0;

	inline void RenderAimbot( )
	{
		Custom::BeginCard( Utils::Translate( xorstr( "General" ) ), 1, 0.f, 0.f, true, ICON_FA_CROSSHAIRS );
		{
			Custom::SectionLabel( Utils::Translate( xorstr( "Settings" ) ) );
			Custom::Toggle( Utils::Translate( xorstr( "Enable" ) ), &g_Config.Aimbot->Enabled,
				xorstr( "Liga o aimbot. Ele so age com arma na mao e mirando." ) );

			static int KeyMode = 1;
			ImGui::Keybind( Utils::Translate( xorstr( "Bind" ) ), &g_Config.Aimbot->KeyBind, &KeyMode );

			Custom::CheckBox( Utils::Translate( xorstr( "Show FOV" ) ), &g_Config.Aimbot->ShowFov,
				xorstr( "Desenha o circulo do campo de mira do aimbot." ) );
			Custom::CheckBox( Utils::Translate( xorstr( "Only Visible" ) ), &g_Config.Aimbot->OnlyVisible,
				xorstr( "Ignora quem esta atras de parede." ) );
			Custom::CheckBox( Utils::Translate( xorstr( "Ignore NPCs" ) ), &g_Config.Aimbot->IgnoreNPCs,
				xorstr( "Mira apenas em jogadores reais." ) );
			Custom::CheckBox( Utils::Translate( xorstr( "Prediction" ) ), &g_Config.Aimbot->Prediction,
				xorstr( "Compensa o deslocamento do alvo." ) );
			Custom::CheckBox( Utils::Translate( xorstr( "Sticky Target" ) ), &g_Config.Aimbot->StickyTarget,
				xorstr( "Mantem o alvo enquanto a tecla permanece pressionada." ) );
			Custom::CheckBox( Utils::Translate( xorstr( "Target Vehicle Tires" ) ), &g_Config.AimbotCar->Enabled,
				xorstr( "Mira nos pneus quando nao houver jogador valido." ) );
		}
		Custom::EndCard( );

		Custom::BeginCard( Utils::Translate( xorstr( "Settings" ) ), 1, 0.f, 0.f, true, ICON_FA_SLIDERS );
		{
			Custom::SectionLabel( Utils::Translate( xorstr( "Aimbot" ) ) );
			Custom::FovSlider( Utils::Translate( xorstr( "Field of View" ) ), &g_Config.Aimbot->FOV );
			ImGui::SliderInt( Utils::Translate( xorstr( "Smooth Horizontal" ) ),
				&g_Config.Aimbot->SmoothHorizontal, 0, 100, xorstr( "%d" ), 0 );
			ImGui::SliderInt( Utils::Translate( xorstr( "Smooth Vertical" ) ),
				&g_Config.Aimbot->SmoothVertical, 0, 100, xorstr( "%d" ), 0 );
			ImGui::SliderInt( Utils::Translate( xorstr( "Max Distance" ) ),
				&g_Config.Aimbot->MaxDistance, 0, 500, xorstr( "%dm" ), 0 );

			const char* HitboxItems[] = {
				Utils::Translate( xorstr( "Head" ) ),
				Utils::Translate( xorstr( "Neck" ) ),
				Utils::Translate( xorstr( "Torso" ) )
			};
			ImGui::Combo( Utils::Translate( xorstr( "Hitbox" ) ),
				&g_Config.Aimbot->Hitbox, HitboxItems, IM_ARRAYSIZE( HitboxItems ) );
		}
		Custom::EndCard( );
	}

	inline void RenderTriggerBot( )
	{
		Custom::BeginCard( Utils::Translate( xorstr( "General" ) ), 1, 0.f, 0.f, true, ICON_FA_CROSSHAIRS );
		{
			Custom::SectionLabel( Utils::Translate( xorstr( "Settings" ) ) );
			Custom::Toggle( Utils::Translate( xorstr( "Enable" ) ), &g_Config.TriggerBot->Enabled,
				xorstr( "Atira quando o alvo entra na mira." ) );

			static int KeyMode = 1;
			ImGui::Keybind( Utils::Translate( xorstr( "Bind" ) ), &g_Config.TriggerBot->KeyBind, &KeyMode );

			Custom::CheckBox( Utils::Translate( xorstr( "Show FOV" ) ), &g_Config.TriggerBot->ShowFov,
				xorstr( "Desenha o campo do triggerbot." ) );
			Custom::CheckBox( Utils::Translate( xorstr( "Only Visible" ) ), &g_Config.TriggerBot->OnlyVisible,
				xorstr( "Ignora quem esta atras de parede." ) );
			Custom::CheckBox( Utils::Translate( xorstr( "Ignore NPCs" ) ), &g_Config.TriggerBot->IgnoreNPCs,
				xorstr( "Mira apenas em jogadores reais." ) );
			Custom::CheckBox( Utils::Translate( xorstr( "Smart Trigger" ) ), &g_Config.TriggerBot->SmartTrigger,
				xorstr( "Usa o ped sob a mira para escolher o alvo." ) );
		}
		Custom::EndCard( );

		Custom::BeginCard( Utils::Translate( xorstr( "Settings" ) ), 1, 0.f, 0.f, true, ICON_FA_SLIDERS );
		{
			Custom::SectionLabel( Utils::Translate( xorstr( "TriggerBot" ) ) );
			Custom::FovSlider( Utils::Translate( xorstr( "Field of View" ) ), &g_Config.TriggerBot->FOV );
			ImGui::SliderInt( Utils::Translate( xorstr( "Max Distance" ) ),
				&g_Config.TriggerBot->MaxDistance, 0, 500, xorstr( "%dm" ), 0 );
			ImGui::SliderInt( Utils::Translate( xorstr( "Delay" ) ),
				&g_Config.TriggerBot->Delay, 0, 500, xorstr( "%dms" ), 0 );
		}
		Custom::EndCard( );
	}

	inline void RenderSilentAim( )
	{
		Custom::BeginCard( Utils::Translate( xorstr( "General" ) ), 1, 0.f, 0.f, true, ICON_FA_CROSSHAIRS );
		{
			Custom::SectionLabel( Utils::Translate( xorstr( "Settings" ) ) );
			Custom::Toggle( Utils::Translate( xorstr( "Enable" ) ), &g_Config.SilentAim->Enabled,
				xorstr( "Redireciona o tiro sem mover a camera." ) );

			static int KeyMode = 1;
			ImGui::Keybind( Utils::Translate( xorstr( "Bind" ) ), &g_Config.SilentAim->KeyBind, &KeyMode );

			Custom::CheckBox( Utils::Translate( xorstr( "Show FOV" ) ), &g_Config.SilentAim->ShowFov,
				xorstr( "Desenha o campo do silent aim." ) );
			Custom::CheckBox( Utils::Translate( xorstr( "Magic Bullets" ) ), &g_Config.SilentAim->MagicBullets,
				xorstr( "Redireciona o tiro para o alvo sem mover a camera." ) );
			Custom::CheckBox( Utils::Translate( xorstr( "Target Vehicle Tires" ) ), &g_Config.SilentAimCar->Enabled,
				xorstr( "Acerta pneus quando nao houver jogador valido." ) );
			Custom::CheckBox( Utils::Translate( xorstr( "Only Visible" ) ), &g_Config.SilentAim->OnlyVisible,
				xorstr( "Ignora jogadores atras de parede." ) );
			Custom::CheckBox( Utils::Translate( xorstr( "Ignore NPCs" ) ), &g_Config.SilentAim->IgnoreNPCs,
				xorstr( "Mira apenas em jogadores reais." ) );
		}
		Custom::EndCard( );

		Custom::BeginCard( Utils::Translate( xorstr( "Settings" ) ), 1, 0.f, 0.f, true, ICON_FA_SLIDERS );
		{
			Custom::SectionLabel( Utils::Translate( xorstr( "SilentAim" ) ) );
			Custom::FovSlider( Utils::Translate( xorstr( "Field of View" ) ), &g_Config.SilentAim->FOV );
			ImGui::SliderFloat( Utils::Translate( xorstr( "Miss Chance" ) ),
				&g_Config.SilentAim->MissChance, 0.f, 100.f, xorstr( "%.0f%%" ) );
			ImGui::SliderInt( Utils::Translate( xorstr( "Max Distance" ) ),
				&g_Config.SilentAim->MaxDistance, 0, 500, xorstr( "%dm" ), 0 );

			const char* HitboxItems[] = {
				Utils::Translate( xorstr( "Head" ) ),
				Utils::Translate( xorstr( "Neck" ) ),
				Utils::Translate( xorstr( "Torso" ) )
			};
			ImGui::Combo( Utils::Translate( xorstr( "Hitbox" ) ),
				&g_Config.SilentAim->Hitbox, HitboxItems, IM_ARRAYSIZE( HitboxItems ) );
		}
		Custom::EndCard( );
	}

	inline void Render( )
	{
		const float Eased = Custom::PageTransition( SubTabAlpha, iSubTab, iSubTabCount );

		ImGui::PushStyleVar( ImGuiStyleVar_Alpha, Eased * ImGui::GetStyle( ).Alpha );

		const float ContentSlide = ( 1.f - Eased ) * g_MenuInfo.TransitionSlide;
		Custom::BeginCards( ContentSlide + g_MenuInfo.TabAdd );
		{
			switch ( iSubTab )
			{
			case 0:
				RenderAimbot( );
				break;
			case 1:
				RenderTriggerBot( );
				break;
			case 2:
				RenderSilentAim( );
				break;
			default:
				break;
			}
		}

		ImGui::PopStyleVar( );
	}
}
