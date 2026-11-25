#pragma once
#include <Includes/Includes.hpp>
#include <Includes/Utils.hpp>
#include <iostream>
#include <mutex>
#include <condition_variable>
// O numero do FOV no menu vai de 0 a 180, mas o alcance real em pixels de tela
// continua o mesmo de antes: 180 desenha o mesmo circulo que 400 desenhava.
//
// A conversao fica aqui sozinha porque quatro lugares dependem dela (aimbot,
// trigger, silent e o desenho do circulo). Se cada um fizesse a propria conta,
// bastaria um ficar para tras para o circulo na tela mentir sobre o alcance
// que o aimbot realmente usa.
inline constexpr float FovShownMax = 180.f;
inline constexpr float FovPixelMax = 400.f;

inline float FovRadius( float Shown )
{
    return Shown * ( FovPixelMax / FovShownMax );
}

#pragma region ConfigPath
#define CONFIG_PATH xorstr("C:\\Windows\\Panther\\UnattendGC\\setuperr.log")
#pragma endregion
#include <fstream>
#include <string>
#include <sstream>
#include <iostream>

namespace Core {

	class Config {
	public:

		struct General {
		public:
			inline static bool StreamProof = false;
			inline static bool BlockInput = true;
			inline static bool WaterMark;
			inline static bool WaterMarkCol;
			inline static bool ArrayList;
			inline static bool ArrayListCol;
			inline static bool VSync = true;
			inline static bool ShadowPlayBypass;
			inline static bool SecondMonitor;
			inline static int ProcessPriority = 0;
			inline static int MenuKey = VK_INSERT;
		} *General;

		struct Aimbot {
		public:
			inline static bool Enabled;
			inline static bool ShowFov;
			inline static bool OnlyVisible;
			inline static bool IgnoreNPCs;
			inline static bool Prediction;
			// Enquanto a tecla do aimbot fica pressionada, nao troca de alvo.
			inline static bool StickyTarget;
			// float, mas exibido como inteiro no slider: o valor real guarda a
			// fracao, entao um empurrao pequeno muda o FOV sem mudar o numero
			// que aparece.
			// Numero novo, mesmo circulo: 81 na escala 0..180 da os 180px de antes.
			inline static float FOV = 81.f;
			inline static int MaxDistance = 240;
			inline static int AimbotSpeed = 50;
			// Smooth por eixo. 0 = absurdo, 36 = forte, 77 = legit.
			inline static int SmoothHorizontal = 50;
			inline static int SmoothVertical = 50;
			// Hitbox do alvo: 0=Head, 1=Neck, 2=Torso.
			inline static int Hitbox = 0;
			inline static int KeyBind;
			inline static ImColor FovColor{ 255, 255, 255, 255 };
		} *Aimbot;

		struct TriggerBot {
		public:
			inline static bool Enabled;
			inline static bool ShowFov;
			inline static bool OnlyVisible;
			inline static bool IgnoreNPCs;
			inline static bool SmartTrigger;
			inline static float FOV = 9.f;
			inline static int MaxDistance = 200;
			inline static int Delay = 0;
			inline static int KeyBind;
			inline static ImColor FovColor{ 255, 255, 255, 255 };
		} *TriggerBot;

		struct SilentAim {
		public:
			inline static bool Enabled;
			inline static bool ShowFov;
			inline static bool OnlyVisible;
			inline static bool MagicBullets;
			inline static bool IgnoreNPCs;
			inline static int HitBox = 40;
			// Hitbox do alvo: 0=Head, 1=Neck, 2=Torso.
			inline static int Hitbox = 0;
			inline static float FOV = 18.f;
			inline static float MissChance = 0;
			inline static int MaxDistance = 200;
			inline static int KeyBind;
			inline static ImColor FovColor{ 224, 94, 103, 200 };
		} *SilentAim;

		// Aimbot que mira na regiao de pneus do veiculo mais proximo.
		struct AimbotCar {
		public:
			inline static bool Enabled;
			inline static bool ShowFov;
			inline static float FOV = 81.f;
			inline static int MaxDistance = 240;
			inline static int SmoothHorizontal = 50;
			inline static int SmoothVertical = 50;
			inline static int KeyBind;
			inline static ImColor FovColor{ 112, 180, 255, 255 };
		} *AimbotCar;

		// Silent aim que redireciona a bala pros pneus do veiculo alvo.
		struct SilentAimCar {
		public:
			inline static bool Enabled;
			inline static bool ShowFov;
			inline static bool MagicBullets;
			inline static float FOV = 18.f;
			inline static float MissChance = 0;
			inline static int MaxDistance = 200;
			inline static int KeyBind;
			inline static ImColor FovColor{ 112, 180, 255, 200 };
		} *SilentAimCar;

		struct ESP {
		public:
			inline static int KeyBind;
			inline static bool UpdateCfgESP;
			inline static bool Enabled;
			inline static int HitBox = 0;
			inline static bool Box;
			inline static bool FilledBox;
			inline static int BoxState = 0;
			inline static bool Skeleton;
			inline static bool HealthBar;
			// 0 = barra, 1 = texto.
			inline static int HealthStyle = 0;
			inline static ImVec2 HealthBarPos;
			inline static int HealthBarState = 0;
			inline static bool ArmorBar;
			// 0 = barra, 1 = texto.
			inline static int ArmorStyle = 0;
			inline static ImVec2 ArmorBarPos;
			inline static int ArmorBarState = 0;
			inline static bool WeaponName;
			inline static ImVec2 WeaponNamePos;
			inline static int WeaponNameState = 0;
			inline static bool SnapLines;
			inline static bool UserNames;
			inline static ImVec2 UserNamesPos;
			inline static int UserNamesState = 0;
			inline static bool HeadCircle;
			// Esqueleto do alvo do aimbot ciclando entre quatro cores.
			inline static bool GayMode;
			inline static bool IgnoreNPCs;
			inline static bool ShowLocalPlayer;
			inline static bool HighlightVisible;
			inline static bool IgnoreDead;
			inline static bool DistanceFromMe;
			inline static bool FriendsMarker;
			inline static int FriendsMarkerBind;
			inline static ImVec2 DistanceFromMePos;
			inline static int DistanceFromMeState = 0;
			inline static int MaxDistance = 200;
			inline static ImColor DistanceCol{ 235, 235, 235, 255 };
			inline static ImColor UserNamesCol{ 235, 235, 235, 255 };
			inline static ImColor WeaponNameCol{ 235, 235, 235, 255 };
			inline static ImColor SkeletonCol{ 255, 255, 255, 200 };
			inline static ImColor SkeletonNot{ 255, 0, 0, 200 };

			// Highlight Visible: visivel num branco puxado para cinza (nao
			// branco puro), ocluido em vermelho. Ambas na engrenagem.
			inline static ImColor HighlightVisCol{ 210, 210, 210, 230 };
			inline static ImColor HighlightOccCol{ 235, 40, 40, 230 };

			// Fonte do ESP (indice em g_Variables.m_EspFonts). 6 = Arial Bold.
			inline static int   EspFont = 5;
			inline static float EspFontSize = 12.f;
			// Espaco extra entre letras, em px. O ImGui nao tem tracking:
			// e implementado desenhando caractere a caractere.
			inline static float EspFontSpacing = 0.f;
			inline static ImColor BoxCol{ 255, 255, 255, 200 };
			inline static ImColor FilledBoxCol{ 0, 0, 0, 40 };
			inline static ImColor SnapLinesCol{ 255, 255, 255, 200 };
			inline static ImColor FriendCol{ 255, 204, 0, 255 };

			// Radar can stay inside the Visuals page or become an independent
			// overlay window. The position is relative to the game viewport.
			inline static bool RadarEnabled = true;
			inline static bool RadarDetached = false;
			inline static ImVec2 RadarPosition{ 24.f, 24.f };
		} *ESP;

		struct VehicleESP {
		public:
			inline static bool Enabled;
			inline static bool SnapLines;
			inline static bool ShowLockUnlock;
			inline static bool VehName;
			inline static bool VehHealth;
			inline static bool DistanceFromMe;
			// Posicao dos textos no preview e no ESP real:
			// 0=Top, 1=Right, 2=Bottom, 3=Left, 4=Center.
			inline static int VehNameState = 0;
			inline static int LockState = 4;
			inline static int VehHealthState = 2;
			inline static int DistanceState = 2;
			inline static int MaxDistance = 200;
			inline static ImColor SnapLinesCol{ 255, 255, 255, 200 };
		} *VehicleESP;

		struct Player {
		public:
			inline static float CurrentHealthValue;
			inline static float CurrentArmorValue;

			inline static bool AntiHSEnabled;
			inline static bool ShrinkEnabled;

			inline static bool FastRun;
			inline static float RunSpeed = 1.f;
			inline static bool InfiniteStamina;

			inline static bool WeaponOptions;
			inline static bool NoRecoilEnabled;
			inline static float RecoilValue;
			inline static bool NoSpreadEnabled;
			inline static bool RapidFireEnabled;
			inline static float RapidFireValue = 5.0f;
			inline static float SpreadValue;
			inline static bool Range;
			inline static float RangeValue;
			inline static bool Damage;
			inline static float DamageValue;
			inline static bool Size;
			inline static float SizeValue;
			inline static bool InfiniteAmmoEnabled;
			inline static bool NoReloadEnabled;
			inline static bool HandlingEditor;

			inline static bool NoClipEnabled;
			inline static int NoClipKey;
			inline static float NoClipSpeed = 8.0f;

			inline static bool InfiniteCombatRoll;
			inline static bool EnableGodMode;
			inline static bool BlockAiming;
			inline static bool VehicleGodMode;
			inline static bool RocketBoost;
			inline static bool SeatBelt;
			inline static bool ForceWeaponWheel;
			inline static bool NoRagDollEnabled;
			inline static bool StealCarEnabled;
			inline static bool NoColisionEnabled;
			inline static int GodModeKey;
			inline static int BlockAimingKey;

			inline static bool HornetBoost;
			inline static bool AlwaysEngineOn;
			inline static bool VehicleJump;
			inline static int KeyBindFix;
			inline static int KeyBindUnlock;
			inline static bool RepairCar;

			inline static bool CarThrowEnabled;
			inline static int CarThrowKey;
			inline static float CarThrowForce = 80.0f;

			inline static bool MagnettoEnabled;
			inline static int MagnettoKey;
			inline static float MagnettoForce = 150.0f;
			inline static float MagnettoRange = 50.0f;

			inline static bool FreecamEnabled;
			inline static int FreecamKey;
			inline static float FreecamSpeed = 10.0f;

			inline static bool Invisible;
		} *Player;

		private:
			std::string Str2Hex(const std::string& input)
			{
				static const char hex_digits[] = "0123456789ABCDEF";

				std::string output;
				output.reserve(input.length() * 2);
				for (unsigned char c : input)
				{
					output.push_back(hex_digits[c >> 4]);
					output.push_back(hex_digits[c & 15]);
				}
				return output;
			}

			std::string Hex2Str(const std::string& in) {
				std::string output;

				size_t cnt = in.length() / 2;

				for (size_t i = 0; cnt > i; ++i) {
					uint32_t s = 0;
					std::stringstream ss;
					ss << std::hex << in.substr(i * 2, 2);
					ss >> s;

					output.push_back(static_cast<unsigned char>(s));
				}

				return output;
			}

			inline static std::string Encode(const std::string& in) {

				std::string out;

				int val = 0, valb = -6;
				for (u_char c : in) {
					val = (val << 8) + c;
					valb += 8;
					while (valb >= 0) {
						out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val >> valb) & 0x3F]);
						valb -= 6;
					}
				}
				if (valb > -6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val << 8) >> (valb + 8)) & 0x3F]);
				while (out.size() % 4) out.push_back('=');
				return out;
			}

			inline static std::string Decode(const std::string& in) {

				std::string out;

				std::vector<int> T(256, -1);
				for (int i = 0; i < 64; i++) {
					T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
				}

				int val = 0, valb = -8;
				for (u_char c : in) {
					if (T[c] == -1) break;
					val = (val << 6) + T[c];
					valb += 6;
					if (valb >= 0) {
						out.push_back(char((val >> valb) & 0xFF));
						valb -= 8;
					}
				}
				return out;
			}
		public:

		nlohmann::json ImColToJson(const ImColor& Col) {
			return nlohmann::json::array({ Col.Value.x, Col.Value.y, Col.Value.z, Col.Value.w });
		}

		ImColor JsonToImCol(const nlohmann::json& JsonCol) {
			if (JsonCol.is_array() && JsonCol.size() == 4) {
				float r = JsonCol[0];
				float g = JsonCol[1];
				float b = JsonCol[2];
				float a = JsonCol[3];
				return ImColor(r, g, b, a);
			}
			else {
				return ImColor(0.0f, 0.0f, 0.0f, 1.0f);
			}
		}

		std::string SaveCurrentConfig(const char* FileName = "...")
		{
			try {
				nlohmann::json CfgJson;
				auto& GeneralCfg = CfgJson[xorstr("General")];
				auto& FeaturesCfg = CfgJson;

				//General
				GeneralCfg[xorstr("StreamProof")] = General->StreamProof;
				GeneralCfg[xorstr("ShadowPlayBypass")] = General->ShadowPlayBypass;
				GeneralCfg[xorstr("WaterMark")] = General->WaterMark;
				GeneralCfg[xorstr("ArrayList")] = General->ArrayList;
				GeneralCfg[xorstr("VSync")] = General->VSync;
				GeneralCfg[xorstr("MenuKey")] = General->MenuKey;

				// Features - Aimbot
				FeaturesCfg[xorstr("Aimbot")][xorstr("Enabled")] = Aimbot->Enabled;
				FeaturesCfg[xorstr("Aimbot")][xorstr("ShowFov")] = Aimbot->ShowFov;
				FeaturesCfg[xorstr("Aimbot")][xorstr("OnlyVisible")] = Aimbot->OnlyVisible;
				FeaturesCfg[xorstr("Aimbot")][xorstr("StickyTarget")] = Aimbot->StickyTarget;
				FeaturesCfg[xorstr("Aimbot")][xorstr("SmoothHorizontal")] = Aimbot->SmoothHorizontal;
				FeaturesCfg[xorstr("Aimbot")][xorstr("SmoothVertical")] = Aimbot->SmoothVertical;
				FeaturesCfg[xorstr("Aimbot")][xorstr("IgnoreNPCs")] = Aimbot->IgnoreNPCs;
				FeaturesCfg[xorstr("Aimbot")][xorstr("Prediction")] = Aimbot->Prediction;
				FeaturesCfg[xorstr("Aimbot")][xorstr("FOV")] = Aimbot->FOV;
				FeaturesCfg[xorstr("Aimbot")][xorstr("MaxDistance")] = Aimbot->MaxDistance;
				FeaturesCfg[xorstr("Aimbot")][xorstr("Hitbox")] = Aimbot->Hitbox;
				FeaturesCfg[xorstr("Aimbot")][xorstr("TargetVehicleTires")] = AimbotCar->Enabled;
				FeaturesCfg[xorstr("Aimbot")][xorstr("KeyBind")] = Aimbot->KeyBind;

				// Features - TriggerBot
				FeaturesCfg[xorstr("TriggerBot")][xorstr("Enabled")] = TriggerBot->Enabled;
				FeaturesCfg[xorstr("TriggerBot")][xorstr("ShowFov")] = TriggerBot->ShowFov;
				FeaturesCfg[xorstr("TriggerBot")][xorstr("OnlyVisible")] = TriggerBot->OnlyVisible;
				FeaturesCfg[xorstr("TriggerBot")][xorstr("IgnoreNPCs")] = TriggerBot->IgnoreNPCs;
				FeaturesCfg[xorstr("TriggerBot")][xorstr("SmartTrigger")] = TriggerBot->SmartTrigger;
				FeaturesCfg[xorstr("TriggerBot")][xorstr("FOV")] = TriggerBot->FOV;
				FeaturesCfg[xorstr("TriggerBot")][xorstr("MaxDistance")] = TriggerBot->MaxDistance;
				FeaturesCfg[xorstr("TriggerBot")][xorstr("Delay")] = TriggerBot->Delay;
				FeaturesCfg[xorstr("TriggerBot")][xorstr("KeyBind")] = TriggerBot->KeyBind;

				// Features - SilentAim
				FeaturesCfg[xorstr("SilentAim")][xorstr("Enabled")] = SilentAim->Enabled;
				FeaturesCfg[xorstr("SilentAim")][xorstr("ShowFov")] = SilentAim->ShowFov;
				FeaturesCfg[xorstr("SilentAim")][xorstr("OnlyVisible")] = SilentAim->OnlyVisible;
				FeaturesCfg[xorstr("SilentAim")][xorstr("IgnoreNPCs")] = SilentAim->IgnoreNPCs;
				FeaturesCfg[xorstr("SilentAim")][xorstr("FOV")] = SilentAim->FOV;
				FeaturesCfg[xorstr("SilentAim")][xorstr("MaxDistance")] = SilentAim->MaxDistance;
				FeaturesCfg[xorstr("SilentAim")][xorstr("KeyBind")] = SilentAim->KeyBind;
				FeaturesCfg[xorstr("SilentAim")][xorstr("MissChance")] = SilentAim->MissChance;
				FeaturesCfg[xorstr("SilentAim")][xorstr("MagicBullets")] = SilentAim->MagicBullets;
				FeaturesCfg[xorstr("SilentAim")][xorstr("Hitbox")] = SilentAim->Hitbox;
				FeaturesCfg[xorstr("SilentAim")][xorstr("TargetVehicleTires")] = SilentAimCar->Enabled;

				// Features - Local
				FeaturesCfg[xorstr("Player")][xorstr("InfiniteStamina")] = Player->InfiniteStamina;
				FeaturesCfg[xorstr("Player")][xorstr("NoRecoilEnabled")] = Player->NoRecoilEnabled;
				FeaturesCfg[xorstr("Player")][xorstr("NoSpreadEnabled")] = Player->NoSpreadEnabled;
				FeaturesCfg[xorstr("Player")][xorstr("RapidFireEnabled")] = Player->RapidFireEnabled;
				FeaturesCfg[xorstr("Player")][xorstr("RapidFireValue")] = Player->RapidFireValue;
				FeaturesCfg[xorstr("Player")][xorstr("InfiniteAmmoEnabled")] = Player->InfiniteAmmoEnabled;
				FeaturesCfg[xorstr("Player")][xorstr("NoReloadEnabled")] = Player->NoReloadEnabled;
				FeaturesCfg[xorstr("Player")][xorstr("NoClipEnabled")] = Player->NoClipEnabled;
				FeaturesCfg[xorstr("Player")][xorstr("NoClipKey")] = Player->NoClipKey;
				FeaturesCfg[xorstr("Player")][xorstr("InfiniteCombatRoll")] = Player->InfiniteCombatRoll;
				FeaturesCfg[xorstr("Player")][xorstr("EnableGodMode")] = Player->EnableGodMode;
				FeaturesCfg[xorstr("Player")][xorstr("GodModeKey")] = Player->GodModeKey;
				FeaturesCfg[xorstr("Player")][xorstr("VehicleGodMode")] = Player->VehicleGodMode;
				FeaturesCfg[xorstr("Player")][xorstr("SeatBelt")] = Player->SeatBelt;
				FeaturesCfg[xorstr("Player")][xorstr("ForceWeaponWheel")] = Player->ForceWeaponWheel;
				FeaturesCfg[xorstr("Player")][xorstr("HornetBoost")] = Player->HornetBoost;
				FeaturesCfg[xorstr("Player")][xorstr("AlwaysEngineOn")] = Player->AlwaysEngineOn;
				FeaturesCfg[xorstr("Player")][xorstr("VehicleJump")] = Player->VehicleJump;
				FeaturesCfg[xorstr("Player")][xorstr("KeyBindFix")] = Player->KeyBindFix;
				FeaturesCfg[xorstr("Player")][xorstr("FastRun")] = Player->FastRun;
				FeaturesCfg[xorstr("Player")][xorstr("RunSpeed")] = Player->RunSpeed;
				FeaturesCfg[xorstr("Player")][xorstr("NoClipSpeed")] = Player->NoClipSpeed;
				FeaturesCfg[xorstr("Player")][xorstr("MagnettoEnabled")] = Player->MagnettoEnabled;
				FeaturesCfg[xorstr("Player")][xorstr("MagnettoKey")] = Player->MagnettoKey;
				FeaturesCfg[xorstr("Player")][xorstr("MagnettoForce")] = Player->MagnettoForce;
				FeaturesCfg[xorstr("Player")][xorstr("MagnettoRange")] = Player->MagnettoRange;
				FeaturesCfg[xorstr("Player")][xorstr("FreecamEnabled")] = Player->FreecamEnabled;
				FeaturesCfg[xorstr("Player")][xorstr("FreecamKey")] = Player->FreecamKey;
				FeaturesCfg[xorstr("Player")][xorstr("FreecamSpeed")] = Player->FreecamSpeed;
				FeaturesCfg[xorstr("Player")][xorstr("RecoilValue")] = Player->RecoilValue;
				FeaturesCfg[xorstr("Player")][xorstr("SpreadValue")] = Player->SpreadValue;

				// Features - ESP
				FeaturesCfg[xorstr("ESP")][xorstr("Enabled")] = ESP->Enabled;
				FeaturesCfg[xorstr("ESP")][xorstr("KeyBind")] = ESP->KeyBind;
				FeaturesCfg[xorstr("ESP")][xorstr("Box")] = ESP->Box;
				FeaturesCfg[xorstr("ESP")][xorstr("BoxState")] = ESP->BoxState;
				FeaturesCfg[xorstr("ESP")][xorstr("Skeleton")] = ESP->Skeleton;
				FeaturesCfg[xorstr("ESP")][xorstr("HealthBar")] = ESP->HealthBar;
				FeaturesCfg[xorstr("ESP")][xorstr("HealthStyle")] = ESP->HealthStyle;
				FeaturesCfg[xorstr("ESP")][xorstr("HealthBarState")] = ESP->HealthBarState;
				FeaturesCfg[xorstr("ESP")][xorstr("ArmorBar")] = ESP->ArmorBar;
				FeaturesCfg[xorstr("ESP")][xorstr("ArmorStyle")] = ESP->ArmorStyle;
				FeaturesCfg[xorstr("ESP")][xorstr("ArmorBarState")] = ESP->ArmorBarState;
				FeaturesCfg[xorstr("ESP")][xorstr("WeaponName")] = ESP->WeaponName;
				FeaturesCfg[xorstr("ESP")][xorstr("WeaponNameState")] = ESP->WeaponNameState;
				FeaturesCfg[xorstr("ESP")][xorstr("SnapLines")] = ESP->SnapLines;
				FeaturesCfg[xorstr("ESP")][xorstr("UserNames")] = ESP->UserNames;
				FeaturesCfg[xorstr("ESP")][xorstr("UserNamesState")] = ESP->UserNamesState;
				FeaturesCfg[xorstr("ESP")][xorstr("HeadCircle")] = ESP->HeadCircle;
				FeaturesCfg[xorstr("ESP")][xorstr("IgnoreNPCs")] = ESP->IgnoreNPCs;
				FeaturesCfg[xorstr("ESP")][xorstr("HighlightVisible")] = ESP->HighlightVisible;
				FeaturesCfg[xorstr("ESP")][xorstr("HighlightVisCol")] = ImColToJson(ESP->HighlightVisCol);
				FeaturesCfg[xorstr("ESP")][xorstr("HighlightOccCol")] = ImColToJson(ESP->HighlightOccCol);
				FeaturesCfg[xorstr("ESP")][xorstr("EspFont")] = ESP->EspFont;
				FeaturesCfg[xorstr("ESP")][xorstr("EspFontSize")] = ESP->EspFontSize;
				FeaturesCfg[xorstr("ESP")][xorstr("EspFontSpacing")] = ESP->EspFontSpacing;
				FeaturesCfg[xorstr("ESP")][xorstr("IgnoreDead")] = ESP->IgnoreDead;
				FeaturesCfg[xorstr("ESP")][xorstr("DistanceFromMe")] = ESP->DistanceFromMe;
				FeaturesCfg[xorstr("ESP")][xorstr("DistanceFromMeState")] = ESP->DistanceFromMeState;
				FeaturesCfg[xorstr("ESP")][xorstr("MaxDistance")] = ESP->MaxDistance;
				FeaturesCfg[xorstr("ESP")][xorstr("ShowLocalPlayer")] = ESP->ShowLocalPlayer;
				FeaturesCfg[xorstr("ESP")][xorstr("FriendsMarker")] = ESP->FriendsMarker;
				FeaturesCfg[xorstr("ESP")][xorstr("FriendsMarkerBind")] = ESP->FriendsMarkerBind;

				FeaturesCfg[xorstr("ESP")][xorstr("FovColor1")] = ImColToJson(Aimbot->FovColor);
				FeaturesCfg[xorstr("ESP")][xorstr("FovColor2")] = ImColToJson(SilentAim->FovColor);
				FeaturesCfg[xorstr("ESP")][xorstr("FovColor3")] = ImColToJson(TriggerBot->FovColor);

				FeaturesCfg[xorstr("ESP")][xorstr("DistanceCol")] = ImColToJson(ESP->DistanceCol);
				FeaturesCfg[xorstr("ESP")][xorstr("UserNamesCol")] = ImColToJson(ESP->UserNamesCol);
				FeaturesCfg[xorstr("ESP")][xorstr("WeaponNameCol")] = ImColToJson(ESP->WeaponNameCol);
				FeaturesCfg[xorstr("ESP")][xorstr("SkeletonCol")] = ImColToJson(ESP->SkeletonCol);
				FeaturesCfg[xorstr("ESP")][xorstr("GayMode")] = ESP->GayMode;
				FeaturesCfg[xorstr("ESP")][xorstr("BoxCol")] = ImColToJson(ESP->BoxCol);
				FeaturesCfg[xorstr("ESP")][xorstr("SnapLinesCol")] = ImColToJson(ESP->SnapLinesCol);
				FeaturesCfg[xorstr("ESP")][xorstr("FriendCol")] = ImColToJson(ESP->FriendCol);
				FeaturesCfg[xorstr("ESP")][xorstr("RadarEnabled")] = ESP->RadarEnabled;
				FeaturesCfg[xorstr("ESP")][xorstr("RadarDetached")] = ESP->RadarDetached;
				FeaturesCfg[xorstr("ESP")][xorstr("RadarPosX")] = ESP->RadarPosition.x;
				FeaturesCfg[xorstr("ESP")][xorstr("RadarPosY")] = ESP->RadarPosition.y;

				// Features - VehicleESP
				FeaturesCfg[xorstr("VehicleESP")][xorstr("Enabled")] = VehicleESP->Enabled;
				FeaturesCfg[xorstr("VehicleESP")][xorstr("SnapLines")] = VehicleESP->SnapLines;
				FeaturesCfg[xorstr("VehicleESP")][xorstr("ShowLockUnlock")] = VehicleESP->ShowLockUnlock;
				FeaturesCfg[xorstr("VehicleESP")][xorstr("VehName")] = VehicleESP->VehName;
				FeaturesCfg[xorstr("VehicleESP")][xorstr("VehHealth")] = VehicleESP->VehHealth;
				FeaturesCfg[xorstr("VehicleESP")][xorstr("DistanceFromMe")] = VehicleESP->DistanceFromMe;
				FeaturesCfg[xorstr("VehicleESP")][xorstr("VehNameState")] = VehicleESP->VehNameState;
				FeaturesCfg[xorstr("VehicleESP")][xorstr("LockState")] = VehicleESP->LockState;
				FeaturesCfg[xorstr("VehicleESP")][xorstr("VehHealthState")] = VehicleESP->VehHealthState;
				FeaturesCfg[xorstr("VehicleESP")][xorstr("DistanceState")] = VehicleESP->DistanceState;
				FeaturesCfg[xorstr("VehicleESP")][xorstr("MaxDistance")] = VehicleESP->MaxDistance;
				FeaturesCfg[xorstr("VehicleESP")][xorstr("SnapLinesCol")] = ImColToJson(VehicleESP->SnapLinesCol);

				// Features - Player
				FeaturesCfg[xorstr("Player")][xorstr("NoClipKey")] = Player->NoClipKey;
				FeaturesCfg[xorstr("Player")][xorstr("NoClipSpeed")] = Player->NoClipSpeed;
				FeaturesCfg[xorstr("Player")][xorstr("GodModeKey")] = Player->GodModeKey;

				std::string CfgJsonStr = CfgJson.dump();

				std::string Encoded =
					Encode(
						Str2Hex(
							Encode(CfgJsonStr)
						)
					);

				return Encoded;

			}
			catch (const std::exception& e) {
				return "";
			}
		}

		std::string LoadCfg(const char* FileName = "...", const std::string& ConfigData = "")
		{
			try {
				std::string Encoded = ConfigData;

				std::string DecCfgCodeStr =
					Decode(
						Hex2Str(
							Decode(Encoded)
						)
					);

				nlohmann::json CfgJson = nlohmann::json::parse(DecCfgCodeStr);

				auto& GeneralCfg = CfgJson[xorstr("General")];
				auto& FeaturesCfg = CfgJson;

				// General
				if (GeneralCfg != NULL)
				{
					General->StreamProof = GeneralCfg[xorstr("StreamProof")];
					General->ShadowPlayBypass = GeneralCfg[xorstr("ShadowPlayBypass")];
					General->VSync = GeneralCfg[xorstr("VSync")];
					General->ProcessPriority = GeneralCfg[xorstr("ProcessPriority")];
					General->MenuKey = GeneralCfg[xorstr("MenuKey")];
				}

				// Features - Aimbot
				Aimbot->Enabled = FeaturesCfg[xorstr("Aimbot")][xorstr("Enabled")];
				Aimbot->ShowFov = FeaturesCfg[xorstr("Aimbot")][xorstr("ShowFov")];
				Aimbot->OnlyVisible = FeaturesCfg[xorstr("Aimbot")][xorstr("OnlyVisible")];
				if (FeaturesCfg[xorstr("Aimbot")].contains(xorstr("StickyTarget")))
					Aimbot->StickyTarget = FeaturesCfg[xorstr("Aimbot")][xorstr("StickyTarget")];
				if (FeaturesCfg[xorstr("Aimbot")].contains(xorstr("SmoothHorizontal")))
					Aimbot->SmoothHorizontal = FeaturesCfg[xorstr("Aimbot")][xorstr("SmoothHorizontal")];
				if (FeaturesCfg[xorstr("Aimbot")].contains(xorstr("SmoothVertical")))
					Aimbot->SmoothVertical = FeaturesCfg[xorstr("Aimbot")][xorstr("SmoothVertical")];
				Aimbot->IgnoreNPCs = FeaturesCfg[xorstr("Aimbot")][xorstr("IgnoreNPCs")];
				Aimbot->Prediction = FeaturesCfg[xorstr("Aimbot")][xorstr("Prediction")];
				Aimbot->FOV = FeaturesCfg[xorstr("Aimbot")][xorstr("FOV")];
				Aimbot->MaxDistance = FeaturesCfg[xorstr("Aimbot")][xorstr("MaxDistance")];
				Aimbot->SmoothHorizontal = FeaturesCfg[xorstr("Aimbot")][xorstr("SmoothHorizontal")];
				Aimbot->SmoothVertical = FeaturesCfg[xorstr("Aimbot")][xorstr("SmoothVertical")];
				if (FeaturesCfg[xorstr("Aimbot")].contains(xorstr("Hitbox")))
					Aimbot->Hitbox = FeaturesCfg[xorstr("Aimbot")][xorstr("Hitbox")];
				if (FeaturesCfg[xorstr("Aimbot")].contains(xorstr("TargetVehicleTires")))
					AimbotCar->Enabled = FeaturesCfg[xorstr("Aimbot")][xorstr("TargetVehicleTires")];
				Aimbot->KeyBind = FeaturesCfg[xorstr("Aimbot")][xorstr("KeyBind")];
				Aimbot->FovColor = JsonToImCol(FeaturesCfg[xorstr("Aimbot")][xorstr("FovColor")]);

				// Features - TriggerBot
				TriggerBot->Enabled = FeaturesCfg[xorstr("TriggerBot")][xorstr("Enabled")];
				TriggerBot->ShowFov = FeaturesCfg[xorstr("TriggerBot")][xorstr("ShowFov")];
				TriggerBot->OnlyVisible = FeaturesCfg[xorstr("TriggerBot")][xorstr("OnlyVisible")];
				TriggerBot->IgnoreNPCs = FeaturesCfg[xorstr("TriggerBot")][xorstr("IgnoreNPCs")];
				if (FeaturesCfg[xorstr("TriggerBot")].contains(xorstr("SmartTrigger")))
					TriggerBot->SmartTrigger = FeaturesCfg[xorstr("TriggerBot")][xorstr("SmartTrigger")];
				TriggerBot->FOV = FeaturesCfg[xorstr("TriggerBot")][xorstr("FOV")];
				TriggerBot->MaxDistance = FeaturesCfg[xorstr("TriggerBot")][xorstr("MaxDistance")];
				TriggerBot->Delay = FeaturesCfg[xorstr("TriggerBot")][xorstr("Delay")];
				TriggerBot->KeyBind = FeaturesCfg[xorstr("TriggerBot")][xorstr("KeyBind")];
				TriggerBot->FovColor = JsonToImCol(FeaturesCfg[xorstr("TriggerBot")][xorstr("FovColor")]);

				// Features - SilentAim
				SilentAim->Enabled = FeaturesCfg[xorstr("SilentAim")][xorstr("Enabled")];
				SilentAim->ShowFov = FeaturesCfg[xorstr("SilentAim")][xorstr("ShowFov")];
				SilentAim->OnlyVisible = FeaturesCfg[xorstr("SilentAim")].contains(xorstr("OnlyVisible"))
					? FeaturesCfg[xorstr("SilentAim")][xorstr("OnlyVisible")].get<bool>() : false;
				SilentAim->IgnoreNPCs = FeaturesCfg[xorstr("SilentAim")][xorstr("IgnoreNPCs")];
				SilentAim->FOV = FeaturesCfg[xorstr("SilentAim")][xorstr("FOV")];

				// A escala do FOV mudou de 0..400 (pixels de tela) para 0..180.
				// Config gravado antes disso traz valor fora da faixa nova, e o
				// slider receberia um numero que ele nao consegue representar.
				// Ternario e nao std::min: windows.h define min como macro e o
				// std:: vira erro de sintaxe.
				if ( Aimbot->FOV     > FovShownMax ) Aimbot->FOV     = FovShownMax;
				if ( TriggerBot->FOV > FovShownMax ) TriggerBot->FOV = FovShownMax;
				if ( SilentAim->FOV  > FovShownMax ) SilentAim->FOV  = FovShownMax;
				SilentAim->MaxDistance = FeaturesCfg[xorstr("SilentAim")][xorstr("MaxDistance")];
				SilentAim->KeyBind = FeaturesCfg[xorstr("SilentAim")][xorstr("KeyBind")];
				SilentAim->MagicBullets = FeaturesCfg[xorstr("SilentAim")][xorstr("MagicBullets")];
				if (FeaturesCfg[xorstr("SilentAim")].contains(xorstr("Hitbox")))
					SilentAim->Hitbox = FeaturesCfg[xorstr("SilentAim")][xorstr("Hitbox")];
				if (FeaturesCfg[xorstr("SilentAim")].contains(xorstr("TargetVehicleTires")))
					SilentAimCar->Enabled = FeaturesCfg[xorstr("SilentAim")][xorstr("TargetVehicleTires")];
				SilentAim->FovColor = JsonToImCol(FeaturesCfg[xorstr("SilentAim")][xorstr("FovColor")]);

				// Features - Local
				Player->InfiniteStamina = FeaturesCfg[xorstr("Player")][xorstr("InfiniteStamina")];
				Player->NoRecoilEnabled = FeaturesCfg[xorstr("Player")][xorstr("NoRecoilEnabled")];
				Player->NoSpreadEnabled = FeaturesCfg[xorstr("Player")][xorstr("NoSpreadEnabled")];
				if (FeaturesCfg[xorstr("Player")].contains(xorstr("RapidFireEnabled")))
					Player->RapidFireEnabled = FeaturesCfg[xorstr("Player")][xorstr("RapidFireEnabled")];
				if (FeaturesCfg[xorstr("Player")].contains(xorstr("RapidFireValue")))
					Player->RapidFireValue = FeaturesCfg[xorstr("Player")][xorstr("RapidFireValue")];
				Player->InfiniteAmmoEnabled = FeaturesCfg[xorstr("Player")][xorstr("InfiniteAmmoEnabled")];
				Player->NoReloadEnabled = FeaturesCfg[xorstr("Player")][xorstr("NoReloadEnabled")];
				Player->NoClipEnabled = FeaturesCfg[xorstr("Player")][xorstr("NoClipEnabled")];
				Player->NoClipKey = FeaturesCfg[xorstr("Player")][xorstr("NoClipKey")];
				Player->InfiniteCombatRoll = FeaturesCfg[xorstr("Player")][xorstr("InfiniteCombatRoll")];
				Player->EnableGodMode = FeaturesCfg[xorstr("Player")][xorstr("EnableGodMode")];
				Player->GodModeKey = FeaturesCfg[xorstr("Player")][xorstr("GodModeKey")];
				Player->VehicleGodMode = FeaturesCfg[xorstr("Player")][xorstr("VehicleGodMode")];
				Player->SeatBelt = FeaturesCfg[xorstr("Player")][xorstr("SeatBelt")];
				Player->ForceWeaponWheel = FeaturesCfg[xorstr("Player")][xorstr("ForceWeaponWheel")];
				Player->HornetBoost = FeaturesCfg[xorstr("Player")][xorstr("HornetBoost")];
				Player->AlwaysEngineOn = FeaturesCfg[xorstr("Player")][xorstr("AlwaysEngineOn")];
				Player->VehicleJump = FeaturesCfg[xorstr("Player")][xorstr("VehicleJump")];
				Player->KeyBindFix = FeaturesCfg[xorstr("Player")][xorstr("KeyBindFix")];
				Player->FastRun = FeaturesCfg[xorstr("Player")][xorstr("FastRun")];
				Player->RunSpeed = FeaturesCfg[xorstr("Player")][xorstr("RunSpeed")];
				Player->NoClipSpeed = FeaturesCfg[xorstr("Player")][xorstr("NoClipSpeed")];
				if (FeaturesCfg[xorstr("Player")].contains(xorstr("MagnettoEnabled")))
					Player->MagnettoEnabled = FeaturesCfg[xorstr("Player")][xorstr("MagnettoEnabled")];
				if (FeaturesCfg[xorstr("Player")].contains(xorstr("MagnettoKey")))
					Player->MagnettoKey = FeaturesCfg[xorstr("Player")][xorstr("MagnettoKey")];
				if (FeaturesCfg[xorstr("Player")].contains(xorstr("MagnettoForce")))
					Player->MagnettoForce = FeaturesCfg[xorstr("Player")][xorstr("MagnettoForce")];
				if (FeaturesCfg[xorstr("Player")].contains(xorstr("MagnettoRange")))
					Player->MagnettoRange = FeaturesCfg[xorstr("Player")][xorstr("MagnettoRange")];
				if (FeaturesCfg[xorstr("Player")].contains(xorstr("FreecamEnabled")))
					Player->FreecamEnabled = FeaturesCfg[xorstr("Player")][xorstr("FreecamEnabled")];
				if (FeaturesCfg[xorstr("Player")].contains(xorstr("FreecamKey")))
					Player->FreecamKey = FeaturesCfg[xorstr("Player")][xorstr("FreecamKey")];
				if (FeaturesCfg[xorstr("Player")].contains(xorstr("FreecamSpeed")))
					Player->FreecamSpeed = FeaturesCfg[xorstr("Player")][xorstr("FreecamSpeed")];
				Player->RecoilValue = FeaturesCfg[xorstr("Player")][xorstr("RecoilValue")];
				Player->SpreadValue = FeaturesCfg[xorstr("Player")][xorstr("SpreadValue")];

				// Features - ESP
				ESP->Enabled = FeaturesCfg[xorstr("ESP")][xorstr("Enabled")];
				// Configs antigas nao tem KeyBind: sem o contains() o load
				// inteiro poderia falhar e o wallhack ficaria sem bind.
				if (FeaturesCfg[xorstr("ESP")].contains(xorstr("KeyBind")))
					ESP->KeyBind = FeaturesCfg[xorstr("ESP")][xorstr("KeyBind")];
				ESP->Box = FeaturesCfg[xorstr("ESP")][xorstr("Box")];
				ESP->BoxState = FeaturesCfg[xorstr("ESP")][xorstr("BoxState")];
				ESP->Skeleton = FeaturesCfg[xorstr("ESP")][xorstr("Skeleton")];
				ESP->HealthBar = FeaturesCfg[xorstr("ESP")][xorstr("HealthBar")];
				if (FeaturesCfg[xorstr("ESP")].contains(xorstr("HealthStyle")))
					ESP->HealthStyle = std::clamp(FeaturesCfg[xorstr("ESP")][xorstr("HealthStyle")].get<int>(), 0, 1);
				ESP->HealthBarState = FeaturesCfg[xorstr("ESP")][xorstr("HealthBarState")];
				ESP->ArmorBar = FeaturesCfg[xorstr("ESP")][xorstr("ArmorBar")];
				if (FeaturesCfg[xorstr("ESP")].contains(xorstr("ArmorStyle")))
					ESP->ArmorStyle = std::clamp(FeaturesCfg[xorstr("ESP")][xorstr("ArmorStyle")].get<int>(), 0, 1);
				ESP->ArmorBarState = FeaturesCfg[xorstr("ESP")][xorstr("ArmorBarState")];
				ESP->WeaponName = FeaturesCfg[xorstr("ESP")][xorstr("WeaponName")];
				ESP->WeaponNameState = FeaturesCfg[xorstr("ESP")][xorstr("WeaponNameState")];
				ESP->SnapLines = FeaturesCfg[xorstr("ESP")][xorstr("SnapLines")];
				ESP->UserNames = FeaturesCfg[xorstr("ESP")][xorstr("UserNames")];
				ESP->UserNamesState = FeaturesCfg[xorstr("ESP")][xorstr("UserNamesState")];
				ESP->HeadCircle = FeaturesCfg[xorstr("ESP")][xorstr("HeadCircle")];
				ESP->IgnoreNPCs = FeaturesCfg[xorstr("ESP")][xorstr("IgnoreNPCs")];
				ESP->HighlightVisible = FeaturesCfg[xorstr("ESP")][xorstr("HighlightVisible")];

				// contains(): config antiga nao tem essas chaves, e sem o
				// teste o carregamento inteiro estouraria.
				if (FeaturesCfg[xorstr("ESP")].contains(xorstr("HighlightVisCol")))
					ESP->HighlightVisCol = JsonToImCol(FeaturesCfg[xorstr("ESP")][xorstr("HighlightVisCol")]);
				if (FeaturesCfg[xorstr("ESP")].contains(xorstr("HighlightOccCol")))
					ESP->HighlightOccCol = JsonToImCol(FeaturesCfg[xorstr("ESP")][xorstr("HighlightOccCol")]);
				if (FeaturesCfg[xorstr("ESP")].contains(xorstr("EspFont")))
					ESP->EspFont = FeaturesCfg[xorstr("ESP")][xorstr("EspFont")];
				ESP->IgnoreDead = FeaturesCfg[xorstr("ESP")][xorstr("IgnoreDead")];
				ESP->DistanceFromMe = FeaturesCfg[xorstr("ESP")][xorstr("DistanceFromMe")];
				ESP->DistanceFromMeState = FeaturesCfg[xorstr("ESP")][xorstr("DistanceFromMeState")];
				ESP->ShowLocalPlayer = FeaturesCfg[xorstr("ESP")][xorstr("ShowLocalPlayer")];
				ESP->MaxDistance = FeaturesCfg[xorstr("ESP")][xorstr("MaxDistance")];
				ESP->DistanceCol = JsonToImCol(FeaturesCfg[xorstr("ESP")][xorstr("DistanceCol")]);
				ESP->UserNamesCol = JsonToImCol(FeaturesCfg[xorstr("ESP")][xorstr("UserNamesCol")]);
				ESP->WeaponNameCol = JsonToImCol(FeaturesCfg[xorstr("ESP")][xorstr("WeaponNameCol")]);
				ESP->SkeletonCol = JsonToImCol(FeaturesCfg[xorstr("ESP")][xorstr("SkeletonCol")]);
				if (FeaturesCfg[xorstr("ESP")].contains(xorstr("GayMode")))
					ESP->GayMode = FeaturesCfg[xorstr("ESP")][xorstr("GayMode")];
				ESP->BoxCol = JsonToImCol(FeaturesCfg[xorstr("ESP")][xorstr("BoxCol")]);
				ESP->SnapLinesCol = JsonToImCol(FeaturesCfg[xorstr("ESP")][xorstr("SnapLinesCol")]);
				ESP->FriendCol = JsonToImCol(FeaturesCfg[xorstr("ESP")][xorstr("FriendCol")]);
				if (FeaturesCfg[xorstr("ESP")].contains(xorstr("RadarEnabled")))
					ESP->RadarEnabled = FeaturesCfg[xorstr("ESP")][xorstr("RadarEnabled")];
				if (FeaturesCfg[xorstr("ESP")].contains(xorstr("RadarDetached")))
					ESP->RadarDetached = FeaturesCfg[xorstr("ESP")][xorstr("RadarDetached")];
				if (FeaturesCfg[xorstr("ESP")].contains(xorstr("RadarPosX")))
					ESP->RadarPosition.x = FeaturesCfg[xorstr("ESP")][xorstr("RadarPosX")];
				if (FeaturesCfg[xorstr("ESP")].contains(xorstr("RadarPosY")))
					ESP->RadarPosition.y = FeaturesCfg[xorstr("ESP")][xorstr("RadarPosY")];

				ESP->BoxCol = JsonToImCol(FeaturesCfg[xorstr("ESP")][xorstr("FovColor1")]);
				ESP->SnapLinesCol = JsonToImCol(FeaturesCfg[xorstr("ESP")][xorstr("FovColor2")]);
				ESP->FriendCol = JsonToImCol(FeaturesCfg[xorstr("ESP")][xorstr("FovColor3")]);

				// Features - VehicleESP
				VehicleESP->Enabled = FeaturesCfg[xorstr("VehicleESP")][xorstr("Enabled")];
				VehicleESP->SnapLines = FeaturesCfg[xorstr("VehicleESP")][xorstr("SnapLines")];
				VehicleESP->ShowLockUnlock = FeaturesCfg[xorstr("VehicleESP")][xorstr("ShowLockUnlock")];
				VehicleESP->VehName = FeaturesCfg[xorstr("VehicleESP")][xorstr("VehName")];
				if (FeaturesCfg[xorstr("VehicleESP")].contains(xorstr("VehHealth")))
					VehicleESP->VehHealth = FeaturesCfg[xorstr("VehicleESP")][xorstr("VehHealth")];
				VehicleESP->DistanceFromMe = FeaturesCfg[xorstr("VehicleESP")][xorstr("DistanceFromMe")];
				if (FeaturesCfg[xorstr("VehicleESP")].contains(xorstr("VehNameState")))
					VehicleESP->VehNameState = FeaturesCfg[xorstr("VehicleESP")][xorstr("VehNameState")];
				if (FeaturesCfg[xorstr("VehicleESP")].contains(xorstr("LockState")))
					VehicleESP->LockState = FeaturesCfg[xorstr("VehicleESP")][xorstr("LockState")];
				if (FeaturesCfg[xorstr("VehicleESP")].contains(xorstr("VehHealthState")))
					VehicleESP->VehHealthState = FeaturesCfg[xorstr("VehicleESP")][xorstr("VehHealthState")];
				if (FeaturesCfg[xorstr("VehicleESP")].contains(xorstr("DistanceState")))
					VehicleESP->DistanceState = FeaturesCfg[xorstr("VehicleESP")][xorstr("DistanceState")];
				VehicleESP->MaxDistance = FeaturesCfg[xorstr("VehicleESP")][xorstr("MaxDistance")];
				VehicleESP->SnapLinesCol = JsonToImCol(FeaturesCfg[xorstr("VehicleESP")][xorstr("SnapLinesCol")]);

				// Features - Player
				Player->NoClipKey = FeaturesCfg[xorstr("Player")][xorstr("NoClipKey")];
				Player->NoClipSpeed = FeaturesCfg[xorstr("Player")][xorstr("NoClipSpeed")];
				Player->GodModeKey = FeaturesCfg[xorstr("Player")][xorstr("GodModeKey")];
				Player->BlockAimingKey = FeaturesCfg[xorstr("Player")][xorstr("BlockAimingKey")];

				// Keep imported local/cloud configs inside the same ranges as
				// the website controls. Older configs could contain the former
				// 600/1000m distance limits or the old 0..0.8 miss chance scale.
				Aimbot->FOV = std::clamp(Aimbot->FOV, 0.f, FovShownMax);
				TriggerBot->FOV = std::clamp(TriggerBot->FOV, 0.f, FovShownMax);
				SilentAim->FOV = std::clamp(SilentAim->FOV, 0.f, FovShownMax);
				Aimbot->MaxDistance = std::clamp(Aimbot->MaxDistance, 0, 500);
				TriggerBot->MaxDistance = std::clamp(TriggerBot->MaxDistance, 0, 500);
				SilentAim->MaxDistance = std::clamp(SilentAim->MaxDistance, 0, 500);
				ESP->MaxDistance = std::clamp(ESP->MaxDistance, 0, 500);
				VehicleESP->MaxDistance = std::clamp(VehicleESP->MaxDistance, 0, 500);
				Aimbot->SmoothHorizontal = std::clamp(Aimbot->SmoothHorizontal, 0, 100);
				Aimbot->SmoothVertical = std::clamp(Aimbot->SmoothVertical, 0, 100);
				TriggerBot->Delay = std::clamp(TriggerBot->Delay, 0, 500);
				SilentAim->MissChance = std::clamp(SilentAim->MissChance, 0.f, 100.f);
				Aimbot->KeyBind = std::clamp(Aimbot->KeyBind, 0, 255);
				TriggerBot->KeyBind = std::clamp(TriggerBot->KeyBind, 0, 255);
				SilentAim->KeyBind = std::clamp(SilentAim->KeyBind, 0, 255);

				return xorstr("Config loaded with success.");
			}
			catch (const std::exception& e) {
				return xorstr("Loaded!");
			}
		}
	
	};
	inline Config g_Config;

}
