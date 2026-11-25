#pragma once
#include <Includes/Includes.hpp>
#include <Core/Offsets.hpp>
#include <Core/SDK/SDK.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

extern const char* keys[];

namespace Core
{
	namespace Features
	{
		class cEsp
		{
		private:
			bool m_RadarWindowNeedsPosition = true;
			bool m_RadarWindowHovered = false;
			bool m_RadarWindowVisible = false;

		public:
			void Draw();
			void DrawVehicle();
			void RenderRadar();
			void RenderDetachedRadar();
			void SetRadarDetached( bool Detached );
			void DeallocateRadar();
			void ResetRadarPosition();
			bool IsRadarWindowHovered() const;
		};

		inline cEsp g_Esp;

	}
}

namespace Core
{
	namespace Features
	{
		inline void cEsp::RenderRadar()
		{
			static float RadarRange = 250.f;

			ImGuiWindow* Window = ImGui::GetCurrentWindow();
			if (!Window)
				return;

			ImDrawList* DrawList = Window->DrawList;
			const ImVec2 Available = ImGui::GetContentRegionAvail();
			const float Gap = 10.f;
			const bool ShowStats = Available.x >= 330.f;
			const float StatsWidth = ShowStats
				? ImClamp(Available.x * 0.30f, 126.f, 150.f)
				: 0.f;
			const float CanvasWidth = ShowStats
				? ImMax(180.f, Available.x - StatsWidth - Gap)
				: Available.x;
			const float CanvasHeight = ImMax(180.f, Available.y);

			ImGui::InvisibleButton(
				xorstr("##radar_canvas"),
				ImVec2(CanvasWidth, CanvasHeight)
			);

			const bool RadarHovered = ImGui::IsItemHovered();
			if (RadarHovered && ImGui::GetIO().MouseWheel != 0.f) {
				RadarRange = ImClamp(
					RadarRange - ImGui::GetIO().MouseWheel * 25.f,
					50.f,
					500.f
				);
			}

			const ImVec2 CanvasMin = ImGui::GetItemRectMin();
			const ImVec2 CanvasMax = ImGui::GetItemRectMax();
			const ImVec2 CanvasSize = CanvasMax - CanvasMin;
			const ImVec2 Center = CanvasMin + CanvasSize * 0.5f;
			const float Radius = ImMax(
				40.f,
				ImMin(CanvasSize.x, CanvasSize.y) * 0.45f
			);

			std::vector<Core::SDK::Game::EntityStruct> Snapshot;
			{
				std::lock_guard<std::mutex> Lock(
					Core::SDK::Game::EntityListMutex
				);
				Snapshot = Core::SDK::Game::EntityList;
			}

			CPed* LocalPlayer = Core::SDK::Pointers::pLocalPlayer;
			D3DXVECTOR3 LocalPosition{};
			bool HasLocalPosition = LocalPlayer != nullptr;
			if (HasLocalPosition) {
				LocalPosition = LocalPlayer->GetPos();
				HasLocalPosition =
					std::isfinite(LocalPosition.x) &&
					std::isfinite(LocalPosition.y) &&
					std::isfinite(LocalPosition.z);
			}

			// The game camera stores a horizontal direction vector. Keeping
			// north at the top when no valid camera is available makes the
			// radar useful while the player is still streaming in.
			float ForwardX = 0.f;
			float ForwardY = 1.f;
			if (LocalPlayer) {
				D3DXVECTOR3 Direction{};
				bool HasDirection = false;

				if (auto* Director = LocalPlayer->CamGamePlayDirector()) {
					if (auto* Camera = Director->GetFollowPedCamera()) {
						Direction = Camera->GetViewAngles();
						HasDirection =
							std::isfinite(Direction.x) &&
							std::isfinite(Direction.y) &&
							(std::fabs(Direction.x) + std::fabs(Direction.y)) > 1e-4f;
					}
				}

				if (!HasDirection) {
					Direction = LocalPlayer->vRotation(false);
					HasDirection =
						std::isfinite(Direction.x) &&
						std::isfinite(Direction.y) &&
						(std::fabs(Direction.x) + std::fabs(Direction.y)) > 1e-4f;
				}

				if (HasDirection) {
					const float Length = std::sqrt(
						Direction.x * Direction.x +
						Direction.y * Direction.y
					);
					if (std::isfinite(Length) && Length > 1e-4f) {
						ForwardX = Direction.x / Length;
						ForwardY = Direction.y / Length;
					}
				}
			}

			const float RightX = ForwardY;
			const float RightY = -ForwardX;
			const float Range = ImMax(RadarRange, 1.f);

			int TotalPlayers = 0;
			int FriendlyPlayers = 0;
			int VisiblePlayers = 0;
			int HiddenPlayers = 0;

			const ImU32 RadarBackground = ImGui::GetColorU32(
				ImVec4(0.005f, 0.025f, 0.012f, 1.f)
			);
			const ImU32 RadarBorder = ImGui::GetColorU32(
				ImVec4(g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, 0.72f)
			);
			const ImU32 GridColor = ImGui::GetColorU32(
				ImVec4(g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, 0.15f)
			);
			const ImU32 AxisColor = ImGui::GetColorU32(
				ImVec4(g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, 0.11f)
			);
			const ImU32 YouColor = ImGui::GetColorU32(
				ImVec4(0.95f, 0.97f, 0.98f, 1.f)
			);
			const ImU32 FriendlyColor = ImGui::GetColorU32(
				ImVec4(0.23f, 0.51f, 0.96f, 1.f)
			);
			const ImU32 VisibleColor = ImGui::GetColorU32(
				ImVec4(0.13f, 0.77f, 0.37f, 1.f)
			);
			const ImU32 HiddenColor = ImGui::GetColorU32(
				ImVec4(0.55f, 0.59f, 0.65f, 0.95f)
			);

			DrawList->AddRectFilled(CanvasMin, CanvasMax, RadarBackground, 8.f);
			DrawList->AddRect(CanvasMin, CanvasMax, ImGui::GetColorU32(
				ImVec4(g_Col.ChildBorderCol.x, g_Col.ChildBorderCol.y,
					g_Col.ChildBorderCol.z, 0.9f)), 8.f);

			DrawList->PushClipRect(CanvasMin, CanvasMax, true);

			for (int Ring = 1; Ring <= 4; ++Ring) {
				const float RingRadius = Radius * static_cast<float>(Ring) / 4.f;
				DrawList->AddCircle(
					Center,
					RingRadius,
					Ring == 4 ? RadarBorder : GridColor,
					96,
					Ring == 4 ? 1.25f : 1.f
				);

				if (Ring < 4) {
					char DistanceLabel[16]{};
					std::snprintf(
						DistanceLabel,
						sizeof(DistanceLabel),
						xorstr("%.0fm"),
						Range * static_cast<float>(Ring) / 4.f
					);
					DrawList->AddText(
						ImVec2(Center.x + RingRadius + 4.f, Center.y - 7.f),
						ImGui::GetColorU32(ImVec4(
							g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, 0.52f
						)),
						DistanceLabel
					);
				}
			}

			DrawList->AddLine(
				ImVec2(Center.x - Radius, Center.y),
				ImVec2(Center.x + Radius, Center.y),
				AxisColor,
				1.f
			);
			DrawList->AddLine(
				ImVec2(Center.x, Center.y - Radius),
				ImVec2(Center.x, Center.y + Radius),
				AxisColor,
				1.f
			);
			DrawList->AddLine(
				ImVec2(Center.x - Radius * 0.72f, Center.y - Radius * 0.72f),
				ImVec2(Center.x + Radius * 0.72f, Center.y + Radius * 0.72f),
				AxisColor,
				1.f
			);
			DrawList->AddLine(
				ImVec2(Center.x - Radius * 0.72f, Center.y + Radius * 0.72f),
				ImVec2(Center.x + Radius * 0.72f, Center.y - Radius * 0.72f),
				AxisColor,
				1.f
			);

			for (const auto& Entity : Snapshot) {
				if (!Entity.Ped ||
					Entity.Ped == LocalPlayer ||
					Entity.PedType != 2) {
					continue;
				}

				++TotalPlayers;
				if (Entity.IsFriend)
					++FriendlyPlayers;
				if (Entity.IsVisible)
					++VisiblePlayers;
				else
					++HiddenPlayers;

				if (!HasLocalPosition ||
					!std::isfinite(Entity.Pos.x) ||
					!std::isfinite(Entity.Pos.y) ||
					!std::isfinite(Entity.Pos.z)) {
					continue;
				}

				const float DeltaX = Entity.Pos.x - LocalPosition.x;
				const float DeltaY = Entity.Pos.y - LocalPosition.y;
				const float Distance = std::sqrt(
					DeltaX * DeltaX + DeltaY * DeltaY
				);
				if (!std::isfinite(Distance) || Distance > Range)
					continue;

				const float ForwardDistance =
					DeltaX * ForwardX + DeltaY * ForwardY;
				const float RightDistance =
					DeltaX * RightX + DeltaY * RightY;

				const ImVec2 Dot(
					Center.x + (RightDistance / Range) * Radius,
					Center.y - (ForwardDistance / Range) * Radius
				);

				const ImU32 DotColor = Entity.IsFriend
					? FriendlyColor
					: Entity.IsVisible ? VisibleColor : HiddenColor;
				const float DotRadius = Distance < 8.f ? 4.f : 3.f;

				if (Entity.IsVisible) {
					DrawList->AddCircle(
						Dot,
						DotRadius + 2.f,
						ImGui::GetColorU32(ImVec4(
							g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, 0.28f
						)),
						20,
						1.f
					);
				}
				DrawList->AddCircleFilled(Dot, DotRadius, DotColor, 20);
			}

			if (HasLocalPosition) {
				DrawList->AddCircle(
					Center,
					7.f,
					YouColor,
					24,
					1.f
				);
				DrawList->AddCircleFilled(Center, 4.f, YouColor, 20);
				DrawList->AddLine(
					Center,
					ImVec2(
						Center.x + ForwardX * 13.f,
						Center.y - ForwardY * 13.f
					),
					YouColor,
					1.5f
				);
			}

			const float LegendWidth = 116.f;
			const float LegendHeight = 67.f;
			const ImVec2 LegendMin(
				CanvasMin.x + 10.f,
				CanvasMax.y - LegendHeight - 10.f
			);
			const ImVec2 LegendMax(
				LegendMin.x + LegendWidth,
				LegendMin.y + LegendHeight
			);
			DrawList->AddRectFilled(
				LegendMin,
				LegendMax,
				ImGui::GetColorU32(ImVec4(0.01f, 0.02f, 0.015f, 0.92f)),
				8.f
			);
			DrawList->AddRect(
				LegendMin,
				LegendMax,
				ImGui::GetColorU32(ImVec4(
					g_Col.ChildBorderCol.x, g_Col.ChildBorderCol.y,
					g_Col.ChildBorderCol.z, 0.95f
				)),
				8.f
			);

			const auto DrawLegendItem = [&](float Y, ImU32 Color, const char* Label) {
				DrawList->AddCircleFilled(
					ImVec2(LegendMin.x + 11.f, LegendMin.y + Y + 5.f),
					3.2f,
					Color,
					16
				);
				DrawList->AddText(
					ImVec2(LegendMin.x + 21.f, LegendMin.y + Y),
					ImGui::GetColorU32(g_Col.SecundaryFeaturesText),
					Label
				);
			};
			DrawLegendItem(8.f, YouColor, Utils::Translate(xorstr("You")));
			DrawLegendItem(22.f, FriendlyColor, Utils::Translate(xorstr("Friendly")));
			DrawLegendItem(36.f, VisibleColor, Utils::Translate(xorstr("Visible")));
			DrawLegendItem(50.f, HiddenColor, Utils::Translate(xorstr("Hidden")));

			if (!HasLocalPosition || Snapshot.empty()) {
				const char* Waiting = Utils::Translate(
					xorstr("Waiting for data...")
				);
				const ImVec2 WaitingSize = ImGui::CalcTextSize(Waiting);
				DrawList->AddText(
					ImVec2(
						Center.x - WaitingSize.x * 0.5f,
						Center.y + Radius * 0.16f
					),
					ImGui::GetColorU32(ImVec4(
						g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, 0.50f
					)),
					Waiting
				);
			}

			const char* ZoomHint = Utils::Translate(
				xorstr("scroll to zoom")
			);
			const ImVec2 ZoomSize = ImGui::CalcTextSize(ZoomHint);
			const ImVec2 ZoomMin(
				CanvasMax.x - ZoomSize.x - 18.f,
				CanvasMax.y - ZoomSize.y - 12.f
			);
			DrawList->AddRectFilled(
				ZoomMin - ImVec2(7.f, 4.f),
				CanvasMax - ImVec2(8.f, 8.f),
				ImGui::GetColorU32(ImVec4(0.01f, 0.02f, 0.015f, 0.78f)),
				9.f
			);
			DrawList->AddText(
				ZoomMin,
				ImGui::GetColorU32(g_Col.SecundaryText),
				ZoomHint
			);

			DrawList->PopClipRect();

			if (!ShowStats)
				return;

			ImGui::SameLine(0.f, Gap);
			ImGui::BeginGroup();

			const ImFont* Font = ImGui::GetFont();
			const float FontSize = ImGui::GetFontSize();

			const auto DrawPanel = [&](const char* Title, float Height) {
				const ImVec2 PanelMin = ImGui::GetCursorScreenPos();
				const ImVec2 PanelMax = PanelMin + ImVec2(StatsWidth, Height);
				ImGui::Dummy(ImVec2(StatsWidth, Height));
				DrawList->AddRectFilled(
					PanelMin,
					PanelMax,
					ImGui::GetColorU32(ImVec4(
						g_Col.ChildCol.x + 0.005f,
						g_Col.ChildCol.y + 0.005f,
						g_Col.ChildCol.z + 0.005f,
						0.96f
					)),
					12.f
				);
				DrawList->AddRect(
					PanelMin,
					PanelMax,
					ImGui::GetColorU32(ImVec4(
						g_Col.ChildBorderCol.x,
						g_Col.ChildBorderCol.y,
						g_Col.ChildBorderCol.z,
						0.95f
					)),
					12.f
				);
				DrawList->AddText(
					Font,
					FontSize,
					PanelMin + ImVec2(14.f, 10.f),
					ImGui::GetColorU32(g_Col.SecundaryText),
					Title
				);
				return PanelMin;
			};

			const auto DrawValue = [&](const ImVec2& PanelMin,
				float OffsetY, const char* Label, const std::string& Value,
				ImU32 ValueColor) {
				DrawList->AddText(
					Font,
					FontSize,
					PanelMin + ImVec2(14.f, OffsetY),
					ImGui::GetColorU32(g_Col.SecundaryFeaturesText),
					Label
				);
				const ImVec2 ValueSize = ImGui::CalcTextSize(Value.c_str());
				DrawList->AddText(
					Font,
					FontSize,
					ImVec2(
						PanelMin.x + StatsWidth - 14.f - ValueSize.x,
						PanelMin.y + OffsetY
					),
					ValueColor,
					Value.c_str()
				);
			};

			const ImVec2 ConnectionPanel = DrawPanel(
				Utils::Translate(xorstr("CONNECTION")),
				62.f
			);
			DrawValue(
				ConnectionPanel,
				34.f,
				Utils::Translate(xorstr("Status")),
				HasLocalPosition
					? std::string(Utils::Translate(xorstr("Online")))
					: std::string(Utils::Translate(xorstr("Offline"))),
				HasLocalPosition
					? VisibleColor
					: HiddenColor
			);

			ImGui::Dummy(ImVec2(0.f, 8.f));

			const ImVec2 LocalPanel = DrawPanel(
				Utils::Translate(xorstr("LOCAL PLAYER")),
				76.f
			);
			DrawValue(
				LocalPanel,
				34.f,
				Utils::Translate(xorstr("Health")),
				HasLocalPosition
					? std::to_string(static_cast<int>(ImMax(
						0.f, LocalPlayer->GetHealth()
					)))
					: std::string(xorstr("--")),
				HasLocalPosition ? YouColor : HiddenColor
			);
			DrawValue(
				LocalPanel,
				53.f,
				Utils::Translate(xorstr("Armor")),
				HasLocalPosition
					? std::to_string(static_cast<int>(ImMax(
						0.f, LocalPlayer->GetArmor()
					)))
					: std::string(xorstr("--")),
				HasLocalPosition ? YouColor : HiddenColor
			);

			ImGui::Dummy(ImVec2(0.f, 8.f));

			const ImVec2 PlayersPanel = DrawPanel(
				Utils::Translate(xorstr("PLAYERS")),
				140.f
			);
			DrawValue(
				PlayersPanel,
				34.f,
				Utils::Translate(xorstr("Total")),
				std::to_string(TotalPlayers),
				YouColor
			);
			DrawValue(
				PlayersPanel,
				53.f,
				Utils::Translate(xorstr("Friendly")),
				std::to_string(FriendlyPlayers),
				FriendlyColor
			);
			DrawValue(
				PlayersPanel,
				72.f,
				Utils::Translate(xorstr("Visible")),
				std::to_string(VisiblePlayers),
				VisibleColor
			);
			DrawValue(
				PlayersPanel,
				91.f,
				Utils::Translate(xorstr("Hidden")),
				std::to_string(HiddenPlayers),
				HiddenColor
			);

			ImGui::EndGroup();
		}

	inline void cEsp::SetRadarDetached( bool Detached )
	{
		if ( !g_Config.ESP )
			return;

		if ( g_Config.ESP->RadarDetached == Detached )
			return;

		g_Config.ESP->RadarDetached = Detached;
		m_RadarWindowNeedsPosition = Detached;
	}

	// Public entry point used by the Radar page. "Deallocate" means removing
	// the radar from the menu card and putting it on the independent overlay.
	inline void cEsp::DeallocateRadar()
	{
		SetRadarDetached( true );
	}

	inline void cEsp::ResetRadarPosition()
	{
		if ( !g_Config.ESP )
			return;

		g_Config.ESP->RadarPosition = ImVec2( 24.f, 24.f );
		m_RadarWindowNeedsPosition = true;
	}

	inline bool cEsp::IsRadarWindowHovered() const
	{
		return m_RadarWindowHovered;
	}

	inline void cEsp::RenderDetachedRadar()
	{
		m_RadarWindowVisible = false;

		if ( !g_Config.ESP
			|| !g_Config.ESP->RadarEnabled
			|| !g_Config.ESP->RadarDetached )
		{
			m_RadarWindowHovered = false;
			return;
		}

		ImGuiIO & IO = ImGui::GetIO();
		const float DisplayWidth = IO.DisplaySize.x > 0.f ? IO.DisplaySize.x : 1920.f;
		const float DisplayHeight = IO.DisplaySize.y > 0.f ? IO.DisplaySize.y : 1080.f;
		const ImVec2 RadarWindowSize( 500.f, 390.f );
		const float Margin = 6.f;

		const float MaxX = ImMax( Margin, DisplayWidth - RadarWindowSize.x - Margin );
		const float MaxY = ImMax( Margin, DisplayHeight - RadarWindowSize.y - Margin );
		const ImVec2 RequestedPosition = g_Config.ESP->RadarPosition;
		const ImVec2 SafePosition(
			ImClamp( RequestedPosition.x, Margin, MaxX ),
			ImClamp( RequestedPosition.y, Margin, MaxY )
		);

		if ( m_RadarWindowNeedsPosition )
		{
			ImGui::SetNextWindowPos( SafePosition, ImGuiCond_Always );
			ImGui::SetNextWindowSize( RadarWindowSize, ImGuiCond_Always );
			m_RadarWindowNeedsPosition = false;
		}
		else
		{
			ImGui::SetNextWindowSize( RadarWindowSize, ImGuiCond_FirstUseEver );
		}

		const ImGuiWindowFlags WindowFlags =
			ImGuiWindowFlags_NoCollapse
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoFocusOnAppearing
			| ImGuiWindowFlags_NoScrollbar;

		ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, g_MenuInfo.CardRounding );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 8.f, 8.f ) );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 1.f );
		ImGui::PushStyleColor( ImGuiCol_WindowBg, g_Col.ChildCol );
		ImGui::PushStyleColor( ImGuiCol_Border, g_Col.ChildBorderCol );
		ImGui::PushFont( g_Variables.m_FontNormal );

		ImGui::Begin( xorstr( "WebRadar##DetachedRadar" ), nullptr, WindowFlags );
		{
			m_RadarWindowVisible = true;
			m_RadarWindowHovered = ImGui::IsWindowHovered(
				ImGuiHoveredFlags_AllowWhenBlockedByPopup
				| ImGuiHoveredFlags_AllowWhenBlockedByActiveItem
			);

			const ImVec2 ActualSize = ImGui::GetWindowSize();
			const float ActualMaxX = ImMax( Margin, DisplayWidth - ActualSize.x - Margin );
			const float ActualMaxY = ImMax( Margin, DisplayHeight - ActualSize.y - Margin );
			const ImVec2 ActualPosition = ImGui::GetWindowPos();
			const ImVec2 ClampedPosition(
				ImClamp( ActualPosition.x, Margin, ActualMaxX ),
				ImClamp( ActualPosition.y, Margin, ActualMaxY )
			);

			if ( ActualPosition.x != ClampedPosition.x
				|| ActualPosition.y != ClampedPosition.y )
			{
				ImGui::SetWindowPos( ClampedPosition );
			}

			g_Config.ESP->RadarPosition = ClampedPosition;
			RenderRadar();
		}
		ImGui::End();

		ImGui::PopFont();
		ImGui::PopStyleColor( 2 );
		ImGui::PopStyleVar( 3 );
	}
	}
}
