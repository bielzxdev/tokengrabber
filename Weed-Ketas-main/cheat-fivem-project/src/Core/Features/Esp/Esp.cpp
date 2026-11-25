#include "pch.h"
#include "Esp.hpp"
#include "WeaponNames.hpp"
#include <cmath>

// g_AimTarget: em quem o aimbot esta puxando, para o Gay Mode.
#include <Core/Features/Aimbot/Aimbot.hpp>

namespace {
	// Largura do texto ja contando o espaco extra entre letras.
	// Precisa bater com o que DrawTextSpaced desenha, senao a centralizacao
	// sai torta.
	float CalcSpacedWidth(ImFont* Font, float Size, const char* Text, float Spacing)
	{
		const float Scale = Size / Font->FontSize;
		float Width = 0.f;

		for (const char* p = Text; *p; p++)
			Width += Font->GetCharAdvance((ImWchar)(unsigned char)*p) * Scale + Spacing;

		if (Width > 0.f) Width -= Spacing;   // nao ha espaco depois da ultima letra
		return Width;
	}

	// O ImGui nao tem tracking, entao desenhamos letra por letra somando o
	// espaco extra no avanco de cada uma.
	void DrawTextSpaced(ImDrawList* DrawList, ImFont* Font, float Size,
		const ImVec2& Pos, ImU32 Color, const char* Text, float Spacing)
	{
		if (Spacing <= 0.f) { DrawList->AddText(Font, Size, Pos, Color, Text); return; }

		const float Scale = Size / Font->FontSize;
		float x = Pos.x;

		for (const char* p = Text; *p; p++)
		{
			const char Buf[2] = { *p, '\0' };
			DrawList->AddText(Font, Size, ImVec2(x, Pos.y), Color, Buf);
			x += Font->GetCharAdvance((ImWchar)(unsigned char)*p) * Scale + Spacing;
		}
	}

	// Contorno fino para manter as letras legiveis sobre o cenario.
	void DrawTextOutlined(ImDrawList* DrawList, ImFont* Font, float Size,
		const ImVec2& Pos, ImU32 Color, const char* Text, float Alpha, float Spacing)
	{
		const ImU32 Outline = ImColor(0.f, 0.f, 0.f, Alpha * 0.65f);
		DrawTextSpaced(DrawList, Font, Size, ImVec2(Pos.x - 1, Pos.y), Outline, Text, Spacing);
		DrawTextSpaced(DrawList, Font, Size, ImVec2(Pos.x + 1, Pos.y), Outline, Text, Spacing);
		DrawTextSpaced(DrawList, Font, Size, ImVec2(Pos.x, Pos.y - 1), Outline, Text, Spacing);
		DrawTextSpaced(DrawList, Font, Size, ImVec2(Pos.x, Pos.y + 1), Outline, Text, Spacing);

		DrawTextSpaced(DrawList, Font, Size, Pos, Color, Text, Spacing);
	}

	float ResolveMaxHealth(float CurrentHealth, float ReportedMaxHealth)
	{
		const float Current = std::isfinite(CurrentHealth) ? ImMax(CurrentHealth, 0.f) : 0.f;
		float Maximum = std::isfinite(ReportedMaxHealth) ? ReportedMaxHealth : 0.f;
		if (Maximum <= 1.f)
			Maximum = 200.f;
		if (Current > Maximum)
			Maximum = ceilf(Current);
		return ImMax(Maximum, 1.f);
	}

	struct NormalizedHealth_t
	{
		float Current;
		float Maximum;
		float Ratio;
	};

	NormalizedHealth_t NormalizeHealth(float CurrentHealth, float ReportedMaxHealth, bool IsPlayer)
	{
		const float RawCurrent = std::isfinite(CurrentHealth) ? ImMax(CurrentHealth, 0.f) : 0.f;
		const float RawMaximum = ResolveMaxHealth(RawCurrent, ReportedMaxHealth);

		// Network players use GTA's reserved 100 HP base (death is around 100),
		// while ambient NPCs use their complete reported health range.
		const float BaseHealth = IsPlayer ? 100.f : 0.f;
		const float EffectiveMaximum = ImMax(RawMaximum - BaseHealth, 1.f);
		const float EffectiveCurrent = ImClamp(RawCurrent - BaseHealth, 0.f, EffectiveMaximum);
		const float Ratio = ImClamp(EffectiveCurrent / EffectiveMaximum, 0.f, 1.f);

		return { Ratio * 100.f, 100.f, Ratio };
	}

	ImColor HealthColorForRatio(float Ratio)
	{
		Ratio = ImClamp(Ratio, 0.f, 1.f);
		return ImColor(
			ImLerp(1.f, g_Col.HealthFull.x, Ratio),
			ImLerp(0.f, g_Col.HealthFull.y, Ratio),
			ImLerp(0.f, g_Col.HealthFull.z, Ratio), 1.f);
	}

	ImColor ArmorColorForRatio(float Ratio)
	{
		Ratio = ImClamp(Ratio, 0.f, 1.f);
		if (Ratio > 0.50f) return ImColor(85, 128, 200, 255);
		if (Ratio > 0.25f) return ImColor(69, 102, 157, 255);
		return ImColor(45, 62, 92, 255);
	}

}

void Core::Features::cEsp::Draw()
{
	auto Cfg = g_Config.ESP;

	if (!Cfg->Enabled)
		return;

	static std::unordered_map<CPed*, Core::SDK::Game::EspAnim> vEspAnimations;

	// Copia sob lock e desenha em cima da copia. Segurar o mutex durante o
	// desenho inteiro travaria a thread de fundo todo frame; iterar sem lock
	// era use-after-free, porque a thread de fundo troca o vetor com
	// std::move e libera o buffer que estivessemos percorrendo.
	static std::vector<Core::SDK::Game::EntityStruct> Snapshot;
	{
		std::lock_guard<std::mutex> Lock(Core::SDK::Game::EntityListMutex);
		Snapshot = Core::SDK::Game::EntityList;
	}

	// Bottom bars and text share a root-relative foot anchor, independent of idle animation.
	struct InfoAnchor_t { float Bottom, Top; int Frame, Id; bool Dead; };
	static std::unordered_map<CPed*, InfoAnchor_t> InfoAnchors;
	static DWORD InfoPid = 0;
	const DWORD Pid = g_LastFiveMPid.load();
	if (InfoPid != Pid) { InfoAnchors.clear(); InfoPid = Pid; }
	const int InfoFrame = ImGui::GetFrameCount();
	if ((InfoFrame % 600) == 0)
		for (auto It = InfoAnchors.begin(); It != InfoAnchors.end(); )
			It = InfoFrame - It->second.Frame > 120 ? InfoAnchors.erase(It) : std::next(It);
	const bool NeedsBottomAnchor = (Cfg->UserNames && Cfg->UserNamesState == 2)
		|| (Cfg->WeaponName && Cfg->WeaponNameState == 2)
		|| (Cfg->DistanceFromMe && Cfg->DistanceFromMeState == 2)
		|| (Cfg->HealthBar && Cfg->HealthBarState == 2)
		|| (Cfg->ArmorBar && Cfg->ArmorBarState == 2);
	D3DXMATRIX InfoView{};
	const bool HasInfoView = NeedsBottomAnchor && Core::SDK::Pointers::pViewPort
		&& Mem.ReadBuffer(Core::SDK::Pointers::pViewPort + 0x24C, &InfoView, sizeof(InfoView));
	if (HasInfoView) D3DXMatrixTranspose(&InfoView, &InfoView);

	std::stable_sort(Snapshot.begin(), Snapshot.end(), [](const auto& A, const auto& B) {
		return A.Distance < B.Distance;
	});
	for (auto& Entity : Snapshot)
	{
		CPed* Ped = Entity.Ped;
		auto DrawList = ImGui::GetBackgroundDrawList();

		auto& CurrentESPAnim = vEspAnimations[Ped];
		CurrentESPAnim.CanFadeOut = false;

		float Distance = Entity.Distance;
		float Health = Entity.Health;
		int PedType = Entity.PedType;
		bool IsFriend = Entity.IsFriend;
		bool IsLocalPlayer = Ped == Core::SDK::Pointers::pLocalPlayer;

		if (!Cfg->ShowLocalPlayer && IsLocalPlayer)
			continue;

		if (Distance > Cfg->MaxDistance)
			continue;

		if (Cfg->IgnoreNPCs && PedType != 2 && !IsLocalPlayer)
			continue;

		if (Cfg->IgnoreDead && !IsLocalPlayer)
		{
			if (!std::isfinite(Health) || ((PedType == 2) ? Health <= 101.f : Health <= 1.f))
				continue;
		}

		D3DXVECTOR3 HeadPos = Ped->GetBonePosDefault(0);
		D3DXVECTOR2 EntityTop = Core::SDK::Game::WorldToScreen(HeadPos + D3DXVECTOR3(0, 0, 0.08f));

		const D3DXVECTOR3 BottomPos = Ped->GetBonePosDefault(8)
			- (g_Offsets.CurrentBuild >= 2802 ? D3DXVECTOR3(0, 0, 1.6f) : D3DXVECTOR3(0, 0, 1.1f));
		D3DXVECTOR2 EntityBottom = Core::SDK::Game::WorldToScreen(BottomPos);

		if (!Core::SDK::Game::IsOnScreen(EntityTop) || !Core::SDK::Game::IsOnScreen(EntityBottom))
			continue;

		// Detect dead/ragdoll state: player dead if health <= 101, NPC dead if health <= 1
		bool IsDead = (PedType == 2) ? (Health <= 101.f) : (Health <= 1.f);

		float Height = EntityBottom.y - EntityTop.y;
		float Width = Height * 0.20f;

		// ------------------------------------------------------------------
		// Caixa dinamica, a partir do esqueleto projetado na tela.
		//
		// Fallback: caixa por altura da entidade, usada so quando o esqueleto
		// nao pode ser lido (streaming, ped atras da camera). Sem ela a caixa
		// sumiria justo no momento em que voce mais quer ver.
		// ------------------------------------------------------------------
		ImVec2 BoxMin( EntityTop.x - Height * 0.20f, EntityTop.y );
		ImVec2 BoxMax( EntityBottom.x + Height * 0.20f, EntityBottom.y );

		{
			Core::SDK::Game::PedBounds Bounds;

			if (Core::SDK::Game::CalculateDynamicPedBounds(Ped, Bounds))
			{
				// Small margins cover body volume around the bone centers.
				const float S = Bounds.Height;
				const float PadX = ImMax(1.f, S * 0.012f);
				const float PadTop = ImMax(1.f, S * 0.05f);
				const float PadBottom = ImMax(0.5f, S * 0.006f);

				ImVec2 TargetMin(Bounds.Left - PadX, Bounds.Top - PadTop);
				ImVec2 TargetMax(Bounds.Right + PadX, Bounds.Bottom + PadBottom);

				// Follow the current pose directly. Interpolating screen-space
				// extrema retains the previous stride and leaves the box trailing.
				BoxMin = TargetMin;
				BoxMax = TargetMax;

			}
		}

		ImVec2 BoxCenter = ImVec2((BoxMin.x + BoxMax.x) * 0.5f, (BoxMin.y + BoxMax.y) * 0.5f);

		// Recalculate Height/Width from final box for health bar sizing
		Height = BoxMax.y - BoxMin.y;
		float BoxLeft = BoxMin.x;
		float BoxRight = BoxMax.x;


		float InfoCenterX = BoxCenter.x, InfoBottom = BoxMax.y;
		float InfoHeight = Height;
		const D3DXVECTOR3 Root = HasInfoView ? Ped->GetPos() : D3DXVECTOR3(0, 0, 0);
		if (HasInfoView && std::isfinite(Root.x) && std::isfinite(Root.y) && std::isfinite(Root.z))
		{
			const auto LeftFoot = Core::SDK::Game::GetBonePosFast(Ped, SKEL_L_Foot);
			const auto RightFoot = Core::SDK::Game::GetBonePosFast(Ped, SKEL_R_Foot);
			float BottomOffset = ImMin(LeftFoot.z, RightFoot.z) - Root.z - 0.04f;
			float TopOffset = HeadPos.z - Root.z + 0.08f;
			// Missing bones must not put the anchor at the pelvis or below the ground.
			if (!std::isfinite(BottomOffset) || BottomOffset >= -0.1f || BottomOffset < -2.5f)
				BottomOffset = -0.95f;
			if (!std::isfinite(TopOffset) || fabsf(TopOffset) > 3.f) TopOffset = 0.85f;
			auto It = InfoAnchors.find(Ped);
			if (It == InfoAnchors.end())
				It = InfoAnchors.emplace(Ped, InfoAnchor_t{BottomOffset, TopOffset, InfoFrame, Entity.Id, IsDead}).first;
			auto& Anchor = It->second;
			const bool Reset = InfoFrame - Anchor.Frame > 5 || Anchor.Id != Entity.Id || Anchor.Dead != IsDead;
			// Keep idle offsets fixed; posture changes update them. Camera and root
			// movement are projected every frame, without screen-space interpolation.
			if (Reset || fabsf(BottomOffset - Anchor.Bottom) > 0.15f) Anchor.Bottom = BottomOffset;
			if (Reset || fabsf(TopOffset - Anchor.Top) > 0.15f) Anchor.Top = TopOffset;
			Anchor.Frame = InfoFrame; Anchor.Id = Entity.Id; Anchor.Dead = IsDead;
			const auto Bottom = Core::SDK::Game::ProjectWorldToScreen(Root + D3DXVECTOR3(0, 0, Anchor.Bottom), InfoView);
			const auto Top = Core::SDK::Game::ProjectWorldToScreen(Root + D3DXVECTOR3(0, 0, Anchor.Top), InfoView);
			if (std::isfinite(Bottom.x) && std::isfinite(Bottom.y) && (Bottom.x != 0.f || Bottom.y != 0.f))
			{
				InfoCenterX = Bottom.x;
				InfoBottom = Bottom.y;

				if (std::isfinite(Top.y) && (Top.x != 0.f || Top.y != 0.f))
					InfoHeight = ImMax(1.f, fabsf(Bottom.y - Top.y));
			}
		}

		const float BottomBarWidth = ImMax(1.f, InfoHeight * 0.35f);
		const float BottomBarLeft = InfoCenterX - BottomBarWidth * 0.5f;
		const float BottomBarThickness = ImClamp(InfoHeight * 0.025f, 1.5f, 4.5f);
		const bool BottomHealth = Cfg->HealthBar && Cfg->HealthStyle == 0 && Cfg->HealthBarState == 2;
		const bool HasArmor = std::isfinite(Entity.Armor) && Entity.Armor > 0.f;
		const bool BottomArmor = Cfg->ArmorBar && Cfg->ArmorStyle == 0
			&& Cfg->ArmorBarState == 2 && HasArmor;
		const float BottomHealthY = InfoBottom + 4.f;
		const float BottomArmorY = BottomHealthY + (BottomHealth ? BottomBarThickness + 2.f : 0.f);
		float FirstTextBoxTop = BoxMin.y - 16;
		float FirstTextBoxBottom = InfoBottom + 4.f;
		if (BottomArmor) FirstTextBoxBottom = BottomArmorY + BottomBarThickness + 3.f;
		else if (BottomHealth) FirstTextBoxBottom = BottomHealthY + BottomBarThickness + 3.f;

		if (Cfg->HealthBar && Cfg->HealthStyle == 0 && Cfg->HealthBarState == 0
			&& Cfg->ArmorBar && Cfg->ArmorStyle == 0 && Cfg->ArmorBarState == 0 && HasArmor) {
			FirstTextBoxTop -= 3;
		}
		else if (Cfg->HealthBar && Cfg->HealthStyle == 0 && Cfg->HealthBarState == 0) {
			FirstTextBoxTop -= 3;
		}
		else if (Cfg->ArmorBar && Cfg->ArmorStyle == 0 && Cfg->ArmorBarState == 0 && HasArmor) {
			FirstTextBoxTop -= 3;
		}

		if (Cfg->FriendsMarker && !IsLocalPlayer)
		{
			float radius = 2 / Distance;

			const static int FovRadius = 60;
			const static int Delay = 300;
			int Key = Cfg->FriendsMarkerBind;

			int FovSize = std::hypot(BoxCenter.x - g_Variables.g_vGameWindowCenter.x, BoxCenter.y - g_Variables.g_vGameWindowCenter.y);

			ImGui::PushFont(g_Variables.m_FontEsp);

			if (IsFriend && FovSize < FovRadius)
			{
				if (Utils::KeyPressedWithDelay(Key, Delay))
				{
					IsFriend = false;
					Core::SDK::Game::FriendMap[Ped->GetID()] = IsFriend;
				}
			}
			else if (!IsFriend && FovSize < FovRadius)
			{
				if (Utils::KeyPressedWithDelay(Key, Delay))
				{
					IsFriend = true;
					Core::SDK::Game::FriendMap[Ped->GetID()] = IsFriend;
				}
			}
			ImGui::PopFont();
		}

		if (Cfg->SnapLines)
		{
			DrawList->AddLine(ImVec2(g_Variables.g_vGameWindowCenter.x, g_Variables.g_vGameWindowPos.y + g_Variables.g_vGameWindowSize.y), ImVec2(BoxCenter.x, BoxMax.y), ImColor(0, 0, 0), 2.5);
			DrawList->AddLine(ImVec2(g_Variables.g_vGameWindowCenter.x, g_Variables.g_vGameWindowPos.y + g_Variables.g_vGameWindowSize.y), ImVec2(BoxCenter.x, BoxMax.y), Cfg->SnapLinesCol, 1.5);
		}

		if (Cfg->Box)
		{
			if (Cfg->FilledBox)
				DrawList->AddRectFilled(BoxMin, BoxMax, Cfg->FilledBoxCol, 0, 0);

			DrawList->AddRect({ BoxMin.x - 1, BoxMin.y - 1 }, { BoxMax.x + 1, BoxMax.y + 1 }, ImColor(0, 0, 0, 200), 0, 0, 1.5);
			DrawList->AddRect({ BoxMin.x + 1, BoxMin.y + 1 }, { BoxMax.x - 1, BoxMax.y - 1 }, ImColor(0, 0, 0, 200), 0, 0, 1.5);
			DrawList->AddRect(BoxMin, BoxMax, Cfg->BoxCol, 0, 0, 1.0);
		}

		// O esqueleto so desenha se a caixa dinamica conseguiu ler as matrizes
		// deste ped -- e ela acabou de le-las. Este bloco refazia o mesmo
		// carregamento por conta propria: mais sete leituras por ped, por frame.
		if (Cfg->Skeleton && Ped == Core::SDK::Game::g_BoneOwner) {

			ImColor skeletonColor = Cfg->SkeletonCol;
			ImColor FriendColor = Cfg->FriendCol;

			if (IsFriend) {
				static float friend_col[4] = { g_Config.ESP->FriendCol.Value.x, g_Config.ESP->FriendCol.Value.y, g_Config.ESP->FriendCol.Value.z, g_Config.ESP->FriendCol.Value.w };

				FriendColor = ImColor(
					g_Config.ESP->FriendCol.Value.x,
					g_Config.ESP->FriendCol.Value.y,
					g_Config.ESP->FriendCol.Value.z,
					g_Config.ESP->FriendCol.Value.w
				);

				skeletonColor = ImColor(friend_col[0], friend_col[1], friend_col[2], friend_col[3]);
			}
			// Gay Mode tem prioridade sobre Highlight Visible: enquanto o
			// aimbot esta puxando neste ped, o esqueleto dele cicla entre as
			// cores. Sai do ar sozinho quando a tecla e solta, porque o aimbot
			// zera o alvo em todo caminho que solta o motor.
			else if (Cfg->GayMode && Ped == Core::Features::g_AimTarget.load(std::memory_order_relaxed)) {
				static const ImVec4 Cycle[] = {
					ImVec4( 1.00f, 1.00f, 1.00f, 1.f ),   // branco
					ImVec4( 1.00f, 0.35f, 0.70f, 1.f ),   // rosa
					ImVec4( 0.25f, 0.58f, 1.00f, 1.f ),   // azul
					ImVec4( 0.30f, 0.90f, 0.42f, 1.f ),   // verde
				};
				constexpr int Count = IM_ARRAYSIZE( Cycle );

				// Interpola entre a cor atual e a proxima em vez de trocar
				// seco: assim e um degrade suave e nao um pisca-pisca.
				const float T = ( float ) ImGui::GetTime( ) * 4.5f;
				const int   I = ( int ) T % Count;
				const float F = T - floorf( T );

				const ImVec4 C = ImLerp( Cycle[ I ], Cycle[ ( I + 1 ) % Count ], F );

				skeletonColor = ImColor( C.x, C.y, C.z, Cfg->SkeletonCol.Value.w );
			}
			else if (Cfg->HighlightVisible) {
				// Cores editaveis na engrenagem do menu.
				// Padrao: branco = com linha de visao, vermelho = ocluido.
				skeletonColor = Entity.IsVisible
					? Cfg->HighlightVisCol
					: Cfg->HighlightOccCol;
			}

			D3DXVECTOR3 PelvisPos = Core::SDK::Game::GetBonePosFast(Ped, SKEL_Pelvis);
			D3DXVECTOR3 NeckPos = Core::SDK::Game::GetBonePosFast(Ped, SKEL_Neck_1);
			D3DXVECTOR3 HeadBonePos = Core::SDK::Game::GetBonePosFast(Ped, SKEL_Head);
			D3DXVECTOR3 LeftUperarmPos = Core::SDK::Game::GetBonePosFast(Ped, SKEL_L_UpperArm);
			D3DXVECTOR3 RightUperarmPos = Core::SDK::Game::GetBonePosFast(Ped, SKEL_R_UpperArm);
			D3DXVECTOR3 RightFormArmPos = Core::SDK::Game::GetBonePosFast(Ped, SKEL_R_Forearm);
			D3DXVECTOR3 LeftFormArmPos = Core::SDK::Game::GetBonePosFast(Ped, SKEL_L_Forearm);
			D3DXVECTOR3 RightHandPos = Core::SDK::Game::GetBonePosFast(Ped, SKEL_R_Hand);
			D3DXVECTOR3 LeftHandPos = Core::SDK::Game::GetBonePosFast(Ped, SKEL_L_Hand);
			D3DXVECTOR3 LeftThighPos = Core::SDK::Game::GetBonePosFast(Ped, SKEL_L_Thigh);
			D3DXVECTOR3 LeftCalfPos = Core::SDK::Game::GetBonePosFast(Ped, SKEL_L_Calf);
			D3DXVECTOR3 RightThighPos = Core::SDK::Game::GetBonePosFast(Ped, SKEL_R_Thigh);
			D3DXVECTOR3 RightCalfPos = Core::SDK::Game::GetBonePosFast(Ped, SKEL_R_Calf);
			D3DXVECTOR3 LfootPos = Core::SDK::Game::GetBonePosFast(Ped, SKEL_L_Foot);
			D3DXVECTOR3 RfootPos = Core::SDK::Game::GetBonePosFast(Ped, SKEL_R_Foot);

			D3DXVECTOR2 Pelvis = Core::SDK::Game::WorldToScreen(PelvisPos);
			D3DXVECTOR2 Neck = Core::SDK::Game::WorldToScreen(NeckPos);
			D3DXVECTOR2 HeadBone = Core::SDK::Game::WorldToScreen(HeadBonePos);
			D3DXVECTOR2 LeftUperarm = Core::SDK::Game::WorldToScreen(LeftUperarmPos);
			D3DXVECTOR2 RightUperarm = Core::SDK::Game::WorldToScreen(RightUperarmPos);
			D3DXVECTOR2 RightFormArm = Core::SDK::Game::WorldToScreen(RightFormArmPos);
			D3DXVECTOR2 LeftFormArm = Core::SDK::Game::WorldToScreen(LeftFormArmPos);
			D3DXVECTOR2 RightHand = Core::SDK::Game::WorldToScreen(RightHandPos);
			D3DXVECTOR2 LeftHand = Core::SDK::Game::WorldToScreen(LeftHandPos);
			D3DXVECTOR2 LeftThigh = Core::SDK::Game::WorldToScreen(LeftThighPos);
			D3DXVECTOR2 LeftCalf = Core::SDK::Game::WorldToScreen(LeftCalfPos);
			D3DXVECTOR2 RightThigh = Core::SDK::Game::WorldToScreen(RightThighPos);
			D3DXVECTOR2 RightCalf = Core::SDK::Game::WorldToScreen(RightCalfPos);
			D3DXVECTOR2 Lfoot = Core::SDK::Game::WorldToScreen(LfootPos);
			D3DXVECTOR2 Rfoot = Core::SDK::Game::WorldToScreen(RfootPos);

			if (!Core::SDK::Game::IsOnScreen(Lfoot) || !Core::SDK::Game::IsOnScreen(Rfoot) ||
				!Core::SDK::Game::IsOnScreen(Pelvis) || !Core::SDK::Game::IsOnScreen(Neck) ||
				!Core::SDK::Game::IsOnScreen(LeftUperarm) || !Core::SDK::Game::IsOnScreen(RightUperarm) ||
				!Core::SDK::Game::IsOnScreen(RightFormArm) || !Core::SDK::Game::IsOnScreen(LeftFormArm) ||
				!Core::SDK::Game::IsOnScreen(RightHand) || !Core::SDK::Game::IsOnScreen(LeftHand) ||
				!Core::SDK::Game::IsOnScreen(LeftThigh) || !Core::SDK::Game::IsOnScreen(RightThigh) ||
				!Core::SDK::Game::IsOnScreen(RightCalf))
				continue;

			auto Bone = [&](const D3DXVECTOR2& A, const D3DXVECTOR2& B) {
				DrawList->AddLine(ImVec2(A.x, A.y), ImVec2(B.x, B.y), skeletonColor, 1.0);
				};

			// O osso SKEL_Head fica na altura dos olhos, entao a linha inteira
			// subia ate ali. Encurtando para 70% do caminho pescoco -> cabeca
			// ela para na altura da boca, que e onde a referencia termina.
			const D3DXVECTOR2 HeadTip(
				Neck.x + (HeadBone.x - Neck.x) * 0.70f,
				Neck.y + (HeadBone.y - Neck.y) * 0.70f);

			Bone(HeadTip, Neck);
			Bone(Neck, Pelvis);

			// Os bracos saem do pescoco em diagonal, e so. Ligar um ombro no
			// outro desenha uma barra atravessando o peito que a referencia
			// nao tem.
			Bone(Neck, LeftUperarm);
			Bone(Neck, RightUperarm);

			Bone(RightUperarm, RightFormArm);
			Bone(LeftUperarm, LeftFormArm);
			Bone(RightFormArm, RightHand);
			Bone(LeftFormArm, LeftHand);
			Bone(Pelvis, LeftThigh);
			Bone(Pelvis, RightThigh);
			Bone(LeftThigh, LeftCalf);
			Bone(RightThigh, RightCalf);
			Bone(LeftCalf, Lfoot);
			Bone(RightCalf, Rfoot);
		}

		if (Cfg->HeadCircle)
		{
			if (HeadPos != D3DXVECTOR3(0, 0, 0)) {
				D3DXVECTOR2 head_screen_pos = SDK::Game::WorldToScreen(HeadPos);
				if (head_screen_pos != D3DXVECTOR2(0, 0)) {

					int CircleValue = Height / 15;
					if (CircleValue <= 10) { Height / 20; }

					ImVec2 center(head_screen_pos.x, head_screen_pos.y);
					DrawList->AddCircle(center, CircleValue, Cfg->SkeletonCol, 999, 1.0);
				}
			}
		}

		// Measure each label and reserve its actual height, including the shadow.
		const float TextFontSize = Cfg->EspFontSize;
		const float TextSpacing = Cfg->EspFontSpacing;
		const float LabelPitch = TextFontSize * 0.7f;
		const NormalizedHealth_t NormalizedHealth = NormalizeHealth(Health, Entity.MaxHealth, PedType == 2);
		const float DisplayHealth = NormalizedHealth.Current;
		const float MaxHealth = NormalizedHealth.Maximum;
		const float HealthRatio = NormalizedHealth.Ratio;
		const ImColor HealthColor = HealthColorForRatio(HealthRatio);
		constexpr float MaxArmor = 100.f;
		const float DisplayArmor = std::isfinite(Entity.Armor)
			? ImClamp(Entity.Armor, 0.f, MaxArmor) : 0.f;
		const float ArmorRatio = DisplayArmor / MaxArmor;
		const ImColor ArmorColor = ArmorColorForRatio(ArmorRatio);

		struct Label_t { std::string Text; int State; ImColor Color; ImVec2 Size, Pos; };
		std::vector<Label_t> Labels;
		Labels.reserve(5);
		const auto AddLabel = [&](std::string Text, int State, ImColor Color) {
			if (Text.empty()) return;
			ImVec2 Size = g_Variables.m_FontEsp->CalcTextSizeA(TextFontSize, FLT_MAX, 0.f, Text.c_str());
			Size.x = CalcSpacedWidth(g_Variables.m_FontEsp, TextFontSize, Text.c_str(), TextSpacing);
			Labels.push_back({std::move(Text), State, Color, Size, ImVec2(0, 0)});
		};
		if (Cfg->HealthBar && Cfg->HealthStyle == 1)
		{
			char HealthText[32];
			snprintf(HealthText, sizeof(HealthText), xorstr("%d/%d"),
				(int)lroundf(DisplayHealth), (int)MaxHealth);
			AddLabel(HealthText, Cfg->HealthBarState, HealthColor);
		}
		if (Cfg->ArmorBar && Cfg->ArmorStyle == 1 && HasArmor)
		{
			char ArmorText[32];
			snprintf(ArmorText, sizeof(ArmorText), xorstr("%d/%d"),
				(int)lroundf(DisplayArmor), (int)MaxArmor);
			AddLabel(ArmorText, Cfg->ArmorBarState, ArmorColor);
		}
		if (Cfg->UserNames) AddLabel(Entity.NetworkInfo.UserName, Cfg->UserNamesState, Cfg->UserNamesCol);
		if (Cfg->WeaponName) {
			std::string Weapon = Core::Features::WeaponNames::FromHash(Entity.WeaponHash);
			if (Weapon.empty()) {
				Weapon = Entity.WeaponName;
				std::transform(Weapon.begin(), Weapon.end(), Weapon.begin(), [](unsigned char C) { return (char)std::toupper(C); });
				if (Weapon.rfind("WEAPON_", 0) == 0) Weapon = Weapon.substr(7);
			}
			if (Weapon.empty()) Weapon = xorstr("UNARMED");
			if (Weapon == "UNARMED") Weapon = xorstr("Unarmed");
			AddLabel(std::move(Weapon), Cfg->WeaponNameState, Cfg->WeaponNameCol);
		}
		if (Cfg->DistanceFromMe) AddLabel(std::to_string((int)Distance) + xorstr("m"), Cfg->DistanceFromMeState, Cfg->DistanceCol);
		float BottomCursor = FirstTextBoxBottom;
		float TopCursor = FirstTextBoxTop;
		for (auto& Label : Labels) {
			const bool Below = Label.State != 0;
			const float CenterX = Below ? InfoCenterX : BoxCenter.x;
			if (Below) {
				Label.Pos = ImVec2(CenterX - Label.Size.x * 0.5f, BottomCursor);
				BottomCursor += LabelPitch;
			} else {
				Label.Pos = ImVec2(CenterX - Label.Size.x * 0.5f, TopCursor);
				TopCursor -= LabelPitch;
			}
		}
		for (const auto& Label : Labels)
			DrawTextOutlined(DrawList, g_Variables.m_FontEsp, TextFontSize, Label.Pos,
				Label.Color, Label.Text.c_str(), Label.Color.Value.w, TextSpacing);

		// Health/armor bar dimensions escalam com a caixa dinamica: barra fina
		// quando o ped esta longe, mais grossa quando esta perto. Piso e teto
		// evitam pixel invisivel de longe e barra grosseira colada num ped
		// enorme, e o rounding proporcional acompanha para os cantos nao ficarem
		// grandes demais numa barra fininha.
		const float BarThickness = ImClamp(Height * 0.025f, 1.5f, 4.5f);
		const float BarRounding  = BarThickness * 0.5f;

		const auto DrawHealthBarV = [&DrawList, BarRounding](ImVec2 pos, ImVec2 dim, ImColor col, int background) {
			if (background == 1) { //Bar
				DrawList->AddRectFilled(ImVec2(pos.x, pos.y), ImVec2(pos.x + dim.x, pos.y - (dim.y + 1)), col, BarRounding);
			}
			else { //BackGround
				DrawList->AddRectFilled(ImVec2(pos.x - 1, pos.y + 1), ImVec2(pos.x + dim.x + 1, pos.y - (dim.y + 2)), ImColor(0, 0, 0, 255), BarRounding + 1.f);
				DrawList->AddRectFilled(ImVec2(pos.x, pos.y - 1), ImVec2(pos.x + dim.x, pos.y - (dim.y + 2)), ImColor(80, 80, 80, 125), BarRounding);
			}
			};
		const auto DrawHealthBarH = [&DrawList](ImVec2 pos, ImVec2 dim, ImColor col, int background) {
			const float BarRounding = dim.x * 0.5f;
			if (background == 1) { //Bar
				DrawList->AddRectFilled(ImVec2(pos.x, pos.y), ImVec2(pos.x + dim.y, pos.y + dim.x), col, BarRounding);
			}
			else { //BackGround
				DrawList->AddRectFilled(ImVec2(pos.x - 1, pos.y - 1), ImVec2(pos.x + dim.y + 1, pos.y + dim.x + 1), ImColor(0, 0, 0, 255), BarRounding + 1.f);
				DrawList->AddRectFilled(ImVec2(pos.x, pos.y), ImVec2(pos.x + dim.y, pos.y + dim.x), ImColor(80, 80, 80, 125), BarRounding);
			}
			};

		if (Cfg->HealthBar && Cfg->HealthStyle == 0)
		{
			CurrentESPAnim.Health = ImLerp(CurrentESPAnim.Health, DisplayHealth, ImGui::GetIO().DeltaTime * 4);
			float AnimHealth = CurrentESPAnim.Health;

			float FullHealthBar = Height / 100 * 100;
			float DecreaseHealthBar = FullHealthBar * (AnimHealth / MaxHealth);

			float Width2 = BoxRight - BoxLeft;
			float FullHealthBarH = Width2 / 100 * 100;
			float DecreaseHealthBarH = FullHealthBarH * (AnimHealth / MaxHealth);

			if (DecreaseHealthBarH > FullHealthBarH) DecreaseHealthBarH = FullHealthBarH;
			if (DecreaseHealthBar > FullHealthBar)   DecreaseHealthBar = FullHealthBar;

			// Gradient: green (full health) -> red (empty)
			float ratio = ImClamp(AnimHealth / MaxHealth, 0.f, 1.f);
			// Vermelho na vida zerada -> g_Col.HealthFull na vida cheia. Antes o
			// canal verde ia direto para 1.0, ou seja verde puro.
			ImColor BarColor = HealthColorForRatio(ratio);
			ImColor FullHealth = (ImVec4(ImColor(80, 80, 80, 200)));

			switch (Cfg->HealthBarState)
			{
			case 0: //Top
				DrawHealthBarH(ImVec2(BoxMin.x, BoxMin.y - 4), ImVec2(BarThickness, FullHealthBarH), (ImVec4(FullHealth)), 0);
				DrawHealthBarH(ImVec2(BoxMin.x, BoxMin.y - 4), ImVec2(BarThickness, DecreaseHealthBarH), (ImVec4(BarColor)), 1);
				break;
			case 1: //Right
				DrawHealthBarV(ImVec2(BoxMax.x + 5, BoxMax.y), ImVec2(BarThickness, FullHealthBar), (ImVec4(FullHealth)), 0);
				DrawHealthBarV(ImVec2(BoxMax.x + 5, BoxMax.y), ImVec2(BarThickness, DecreaseHealthBar), (ImVec4(BarColor)), 1);
				break;
			case 2: //Bottom
				DrawHealthBarH(ImVec2(BottomBarLeft, BottomHealthY), ImVec2(BottomBarThickness, BottomBarWidth), (ImVec4(FullHealth)), 0);
				DrawHealthBarH(ImVec2(BottomBarLeft, BottomHealthY), ImVec2(BottomBarThickness, BottomBarWidth * ImClamp(AnimHealth / MaxHealth, 0.f, 1.f)), (ImVec4(BarColor)), 1);
				break;
			case 3: //Left
				DrawHealthBarV(ImVec2(BoxMin.x - 5, BoxMax.y), ImVec2(BarThickness, FullHealthBar), (ImVec4(FullHealth)), 0);
				DrawHealthBarV(ImVec2(BoxMin.x - 5, BoxMax.y), ImVec2(BarThickness, DecreaseHealthBar), (ImVec4(BarColor)), 1);
				break;
			default:
				DrawHealthBarV(ImVec2(BoxMin.x - 5, BoxMax.y), ImVec2(BarThickness, FullHealthBar), (ImVec4(FullHealth)), 0);
				DrawHealthBarV(ImVec2(BoxMin.x - 5, BoxMax.y), ImVec2(BarThickness, DecreaseHealthBar), (ImVec4(BarColor)), 1);
				break;
			}
		}

		if (Cfg->ArmorBar && Cfg->ArmorStyle == 0)
		{
			if (HasArmor)
			{
				CurrentESPAnim.Armor = ImLerp(CurrentESPAnim.Armor, DisplayArmor, ImGui::GetIO().DeltaTime * 4);
				float AnimArmor = CurrentESPAnim.Armor;

				float FullArmorBar = Height / 100 * 100;
				float DecreaseHealthBar = FullArmorBar * (AnimArmor / MaxArmor);

				float Width2 = BoxRight - BoxLeft;
				float FullArmorBarH = Width2 / 100 * 100;
				float DecreaseArmorBarH = FullArmorBarH * (AnimArmor / MaxArmor);


				if (DecreaseHealthBar > FullArmorBar)
				{
					DecreaseHealthBar = FullArmorBar;
				}

				if (DecreaseArmorBarH > FullArmorBarH)
				{
					DecreaseArmorBarH = FullArmorBarH;
				}

				ImColor BarColor = ArmorColorForRatio(AnimArmor / MaxArmor);
				ImColor FullArmor = ImVec4(ImColor(80, 80, 80, 200));

				switch (Cfg->ArmorBarState)
				{

				case 0: //Top
					if (Cfg->HealthBar && Cfg->HealthStyle == 0 && Cfg->HealthBarState == Cfg->ArmorBarState)
					{
						DrawHealthBarH(ImVec2(BoxMin.x, BoxMin.y - 7), ImVec2(BarThickness, FullArmorBarH), FullArmor, 0);
						DrawHealthBarH(ImVec2(BoxMin.x, BoxMin.y - 7), ImVec2(BarThickness, DecreaseArmorBarH), BarColor, 1);
					}
					else
					{
						DrawHealthBarH(ImVec2(BoxMin.x, BoxMin.y - 4), ImVec2(BarThickness, FullArmorBarH), FullArmor, 0);
						DrawHealthBarH(ImVec2(BoxMin.x, BoxMin.y - 4), ImVec2(BarThickness, DecreaseArmorBarH), BarColor, 1);
					}
					break;
				case 1: //Right
					if (Cfg->HealthBar && Cfg->HealthStyle == 0 && Cfg->HealthBarState == Cfg->ArmorBarState)
					{
						DrawHealthBarV(ImVec2(BoxMax.x + 10, BoxMax.y), ImVec2(BarThickness, FullArmorBar), FullArmor, 0);
						DrawHealthBarV(ImVec2(BoxMax.x + 10, BoxMax.y), ImVec2(BarThickness, DecreaseHealthBar), BarColor, 1);
					}
					else
					{
						DrawHealthBarV(ImVec2(BoxMax.x + 5, BoxMax.y), ImVec2(BarThickness, FullArmorBar), FullArmor, 0);
						DrawHealthBarV(ImVec2(BoxMax.x + 5, BoxMax.y), ImVec2(BarThickness, DecreaseHealthBar), BarColor, 1);
					}
					break;
				case 2: //Bottom
					DrawHealthBarH(ImVec2(BottomBarLeft, BottomArmorY), ImVec2(BottomBarThickness, BottomBarWidth), FullArmor, 0);
					DrawHealthBarH(ImVec2(BottomBarLeft, BottomArmorY), ImVec2(BottomBarThickness, BottomBarWidth * ImClamp(AnimArmor / MaxArmor, 0.f, 1.f)), BarColor, 1);
					break;
				case 3: //Left
					if (Cfg->HealthBar && Cfg->HealthStyle == 0 && Cfg->HealthBarState == Cfg->ArmorBarState)
					{
						DrawHealthBarV(ImVec2(BoxMin.x - 10, BoxMax.y), ImVec2(BarThickness, FullArmorBar), FullArmor, 0);
						DrawHealthBarV(ImVec2(BoxMin.x - 10, BoxMax.y), ImVec2(BarThickness, DecreaseHealthBar), BarColor, 1);
					}
					else
					{
						DrawHealthBarV(ImVec2(BoxMin.x - 5, BoxMax.y), ImVec2(BarThickness, FullArmorBar), FullArmor, 0);
						DrawHealthBarV(ImVec2(BoxMin.x - 5, BoxMax.y), ImVec2(BarThickness, DecreaseHealthBar), BarColor, 1);
					}
					break;

				default:
					DrawHealthBarV(ImVec2(BoxMin.x - 5, BoxMax.y), ImVec2(BarThickness, FullArmorBar), FullArmor, 0);
					DrawHealthBarV(ImVec2(BoxMin.x - 5, BoxMax.y), ImVec2(BarThickness, DecreaseHealthBar), BarColor, 1);
					break;
				}

			}

		}
	}
}

void Core::Features::cEsp::DrawVehicle()
{
	auto Cfg = g_Config.VehicleESP;
	if (!Cfg->Enabled) return;

	std::vector<Core::SDK::Game::VehicleStructure> Vehicles;
	{
		std::lock_guard<std::mutex> Lock(Core::SDK::Game::VehicleListMutex);
		Vehicles = Core::SDK::Game::VehicleList;
	}

	for (const auto& Entity : Vehicles)
	{
		auto DrawList = ImGui::GetBackgroundDrawList();

		const float Distance = Entity.Dist;
		if (Distance > Cfg->MaxDistance)
			continue;

		const D3DXVECTOR2 VehicleLocation = Core::SDK::Game::WorldToScreen(Entity.Pos);
		if (!VehicleLocation) continue;
		if (!SDK::Game::IsOnScreen(VehicleLocation)) continue;

		if (Cfg->SnapLines)
		{
			DrawList->AddLine(
				ImVec2(g_Variables.g_vGameWindowCenter.x, g_Variables.g_vGameWindowPos.y + g_Variables.g_vGameWindowSize.y),
				ImVec2(VehicleLocation.x, VehicleLocation.y + 30),
				ImColor(0, 0, 0),
				2.5
			);
			DrawList->AddLine(
				ImVec2(g_Variables.g_vGameWindowCenter.x, g_Variables.g_vGameWindowPos.y + g_Variables.g_vGameWindowSize.y),
				ImVec2(VehicleLocation.x, VehicleLocation.y + 30),
				Cfg->SnapLinesCol,
				1.5
			);
		}

		// Layout antigo: linhas simples empilhadas a partir da origem do veiculo,
		// sem caixa, modelo 3D ou coordenadas compartilhadas com o preview.
		float YOffset = 0.f;
		const float FontSize = g_Config.ESP->EspFontSize;
		const auto DrawLine = [&](const std::string& Text, ImColor Color)
		{
			if (Text.empty()) return;
			ImVec2 Size = g_Variables.m_FontEsp->CalcTextSizeA(
				FontSize, FLT_MAX, 0.f, Text.c_str());
			Size.x = CalcSpacedWidth(g_Variables.m_FontEsp, FontSize,
				Text.c_str(), g_Config.ESP->EspFontSpacing);
			DrawTextOutlined(DrawList, g_Variables.m_FontEsp, FontSize,
				ImVec2(VehicleLocation.x - Size.x * 0.5f, VehicleLocation.y + YOffset),
				Color, Text.c_str(), Color.Value.w, g_Config.ESP->EspFontSpacing);
			YOffset += Size.y + 1.f;
		};

		if (Cfg->VehName)
			DrawLine(Entity.Name, ImColor(255, 255, 255, 255));
		if (Cfg->ShowLockUnlock)
			DrawLine(Entity.IsLocked ? xorstr("Trancado") : xorstr("Destrancado"),
				ImColor(255, 205, 80, 255));
		if (Cfg->VehHealth)
		{
			char HealthText[32];
			snprintf(HealthText, sizeof(HealthText), xorstr("%.0f"), Entity.Health);
			DrawLine(HealthText, ImColor(255, 255, 255, 255));
		}
		if (Cfg->DistanceFromMe)
			DrawLine(std::to_string(static_cast<int>(Distance)) + xorstr("m"),
				ImColor(255, 255, 255, 255));
	}
}
