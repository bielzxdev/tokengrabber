#pragma once
#include <Includes/Includes.hpp>
#include <chrono>
#include <map>

namespace Custom
{
	namespace WaterMark
	{
		struct WaterMarkAnim_t {
			float Alpha = 0.f;
			float Offset = 0.f;
		};

		static std::map<std::string, WaterMarkAnim_t> g_WaterMarkAnim;

		float GetTime()
		{
			using namespace std::chrono;
			return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count() / 1000.f;
		}

		void RenderGlowBorder(ImDrawList* DrawList, ImRect Rect, float Rounding, ImVec4 Color, float Thickness, float Alpha)
		{
			ImVec2 Min = Rect.Min;
			ImVec2 Max = Rect.Max;
			
			float AnimTime = GetTime();
			float AnimProgress = fmod(AnimTime * 0.5f, 1.0f);
			
			ImVec2 Size = Max - Min;
			float Perimeter = (Size.x + Size.y) * 2.f;
			float StartPos = AnimProgress * Perimeter;
			
			ImU32 GlowCol = ImGui::GetColorU32(ImVec4(Color.x, Color.y, Color.z, Color.w * Alpha));
			
			auto DrawSegment = [&](ImVec2 p1, ImVec2 p2)
			{
				float Distance = sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2));
				DrawList->AddLine(p1, p2, GlowCol, Thickness);
			};
			
			float TopSide = (Max.x - Min.x);
			float RightSide = (Max.y - Min.y);
			float BottomSide = (Max.x - Min.x);
			float LeftSide = (Max.y - Min.y);
			
			ImVec2 p1 = ImVec2(Min.x, Min.y + Rounding);
			ImVec2 p2 = ImVec2(Min.x, Max.y - Rounding);
			ImVec2 p3 = ImVec2(Max.x, Max.y - Rounding);
			ImVec2 p4 = ImVec2(Max.x, Min.y + Rounding);
			
			float SegmentPos = 0.f;
			float SegmentLength = LeftSide;
			
			if (StartPos >= 0 && StartPos < SegmentLength) {
				float t = StartPos / SegmentLength;
				ImVec2 pos = ImLerp(p1, p2, t);
				ImVec2 end = ImLerp(p1, p2, ImMin(1.f, (StartPos + 20.f) / SegmentLength));
				DrawSegment(pos, end);
			}
			
			SegmentPos = SegmentLength;
			SegmentLength += TopSide;
			if (StartPos >= SegmentPos && StartPos < SegmentLength) {
				float t = (StartPos - SegmentPos) / (SegmentLength - SegmentPos);
				ImVec2 pos = ImLerp(p2, p3, t);
				ImVec2 end = ImLerp(p2, p3, ImMin(1.f, (StartPos - SegmentPos + 20.f) / (SegmentLength - SegmentPos)));
				DrawSegment(pos, end);
			}
			
			SegmentPos = SegmentLength;
			SegmentLength += RightSide;
			if (StartPos >= SegmentPos && StartPos < SegmentLength) {
				float t = (StartPos - SegmentPos) / (SegmentLength - SegmentPos);
				ImVec2 pos = ImLerp(p3, p4, t);
				ImVec2 end = ImLerp(p3, p4, ImMin(1.f, (StartPos - SegmentPos + 20.f) / (SegmentLength - SegmentPos)));
				DrawSegment(pos, end);
			}
			
			SegmentPos = SegmentLength;
			SegmentLength += BottomSide;
			if (StartPos >= SegmentPos && StartPos < SegmentLength) {
				float t = (StartPos - SegmentPos) / (SegmentLength - SegmentPos);
				ImVec2 pos = ImLerp(p4, p1, t);
				ImVec2 end = ImLerp(p4, p1, ImMin(1.f, (StartPos - SegmentPos + 20.f) / (SegmentLength - SegmentPos)));
				DrawSegment(pos, end);
			}
		}

		void Render()
		{
			ImDrawList* DrawList = ImGui::GetForegroundDrawList();
			const ImVec2 WindowSize = g_Variables.g_vGameWindowSize;
			
			const float Padding = 14.f;
			const float Rounding = 8.f;
			const char* Title = Utils::Translate( xorstr("Weed Cheat") );
			const char* Version = Utils::Translate( xorstr("v1.0") );
			
			auto TitleSize = Utils::CalcTextSize(g_Variables.m_FontNormal, g_Variables.m_FontNormal->FontSize, Title);
			auto VersionSize = Utils::CalcTextSize(g_Variables.m_FontSecundary, g_Variables.m_FontSecundary->FontSize, Version);
			
			float ContentWidth = ImMax(TitleSize.x, VersionSize.x);
			float ContentHeight = TitleSize.y + VersionSize.y + 3.f;
			
			ImVec2 WaterMarkSize = ImVec2(
				Padding * 2.f + ContentWidth,
				Padding * 2.f + ContentHeight
			);
			
			ImVec2 WaterMarkPos = ImVec2(
				WindowSize.x - WaterMarkSize.x - 16.f,
				10.f
			);
			
			const std::string AnimKey = xorstr("watermark_main");
			auto AnimIt = g_WaterMarkAnim.find(AnimKey);
			if (AnimIt == g_WaterMarkAnim.end()) {
				g_WaterMarkAnim.insert({ AnimKey, WaterMarkAnim_t() });
				AnimIt = g_WaterMarkAnim.find(AnimKey);
			}
			
			AnimIt->second.Alpha = ImLerp(AnimIt->second.Alpha, 1.f, ImGui::GetIO().DeltaTime * 4.f);
			
			ImRect WaterMarkRect(WaterMarkPos, WaterMarkPos + WaterMarkSize);
			
			DrawList->AddShadowRect(
				WaterMarkRect.Min,
				WaterMarkRect.Max,
				ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, 0.45f)),
				22.f,
				ImVec2(0.f, 6.f),
				ImDrawFlags_ShadowCutOutShapeBackground,
				Rounding
			);
			
			DrawList->AddRectFilled(
				WaterMarkRect.Min,
				WaterMarkRect.Max,
				ImGui::GetColorU32(g_Col.NotifyBackground),
				Rounding
			);
			
			DrawList->AddRect(
				WaterMarkRect.Min,
				WaterMarkRect.Max,
				ImGui::GetColorU32(g_Col.NotifyBorder),
				Rounding
			);
			
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, AnimIt->second.Alpha);
			
			RenderGlowBorder(DrawList, WaterMarkRect, Rounding, g_Col.Base, 1.2f, 0.6f);
			
			ImGui::PopStyleVar();
			
			float TextX = WaterMarkRect.Min.x + Padding;
			float TextY = WaterMarkRect.Min.y + Padding;
			
			DrawList->AddText(
				g_Variables.m_FontNormal,
				g_Variables.m_FontNormal->FontSize,
				ImVec2(TextX, TextY),
				ImGui::GetColorU32(g_Col.PrimaryText),
				Title
			);
			
			DrawList->AddText(
				g_Variables.m_FontSecundary,
				g_Variables.m_FontSecundary->FontSize,
				ImVec2(TextX, TextY + TitleSize.y + 3.f),
				ImGui::GetColorU32(g_Col.FeaturesText),
				Version
			);
		}
	}

	namespace ArrayList
	{
		void Render()
		{

		}
	}

}
