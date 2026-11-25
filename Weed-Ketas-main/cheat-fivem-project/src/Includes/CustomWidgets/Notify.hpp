#pragma once
#include <Includes/Includes.hpp>
#include <Includes/Utils.hpp>
#include <Includes/CustomWidgets/Custom.hpp>
#include <chrono>
#include <map>

namespace NotifyManager
{
	enum eType {
		None,
		Info,
		Warning
	};

	enum eState {
		In,
		Current,
		Out,
		Expired
	};

	class NotifyClass {
	private:
		std::string Title;
		std::string Description;
		time_t ExpireTime = 0;
		time_t CreationTime = 0;
		eType Type = eType::None;
		eState CurrentState = eState::In;
	public:
		void SetTitle( std::string NewTitle ) { this->Title = NewTitle; }
		void SetDescription( std::string NewDesc ) { this->Description = NewDesc; }
		void SetType( eType NewType ) { this->Type = NewType; }
		void SetState( eState NewState ) { this->CurrentState = NewState; }
		void SetCreationTime( time_t NewCreationTime ) { this->CreationTime = NewCreationTime; }
	public:
		std::string GetTitle( ) { return Title; }
		std::string GetDescription( ) { return Description; }
		time_t GetExpireTime( ) { return ExpireTime; }
		time_t GetCreationTime( ) { return CreationTime; }
		eType GetType( ) { return Type; }
		eState GetCurrentState( ) { return CurrentState; }
	public:
		time_t GetNowTime( ) {
			using namespace std::chrono;
			return duration_cast< milliseconds >( steady_clock::now( ).time_since_epoch( ) ).count( );
		}

		time_t GetTimeDiff( ) {
			return ( time_t ) ( GetNowTime( ) - CreationTime );
		}

		NotifyClass( eType Type, time_t ExpireTime = 4000 )
		{
			this->Type = Type;
			this->ExpireTime = ExpireTime;
			this->CreationTime = GetNowTime( );
		}
	};

	inline std::vector<NotifyClass> NotifyList;
	inline std::mutex NotifyMutex;

    inline void DeleteNotify( int Index )
	{
		NotifyList.erase( NotifyList.begin( ) + Index );
	}

    inline void Render( )
	{
		std::lock_guard<std::mutex> Lock( NotifyMutex );
        const auto DrawList = ImGui::GetForegroundDrawList( );
        const auto WindowSize = g_Variables.g_vGameWindowSize;
        float NextHeight = 0.f;

        for ( auto i = 0; i < NotifyList.size( ); i++ )
        {
            auto & Notify = NotifyList.at( i );

            struct NotifyWid_t {
                float SlideX = 0.f;
                float YPos = 0.f;
                float Alpha = 0.f;
            };

            static std::map<std::string, NotifyWid_t> anim;
            const auto Id = Notify.GetDescription( ) + std::to_string( Notify.GetCreationTime( ) );
            auto NotifyAnim = anim.find( Id );
            if ( NotifyAnim == anim.end( ) )
            {
                anim.insert( { Id, NotifyWid_t( ) } );
                NotifyAnim = anim.find( Id );
            }

            if ( Notify.GetCurrentState( ) == eState::Expired )
            {
                Notify.SetState( eState::In );
                DeleteNotify( i );
                anim.erase( Id );
                i--;
                continue;
            }

            auto TitleTxtSize = Utils::CalcTextSize( g_Variables.m_FontNormal, g_Variables.m_FontNormal->FontSize, Notify.GetTitle( ).c_str( ) );
            auto DescTxtSize = Utils::CalcTextSize( g_Variables.m_FontSecundary, g_Variables.m_FontSecundary->FontSize, Notify.GetDescription( ).c_str( ) );

            // Card layout: round icon badge on the left, title over description on the right
            const float Padding = 14.f;
            const float Rounding = 8.f;
            const float BadgeRadius = 13.f;
            const float BadgeSpacing = 12.f;

            static ImVec2 NotifySize;

            const float TextWidth = ImMax( TitleTxtSize.x, DescTxtSize.x );
            const float TextHeight = TitleTxtSize.y + DescTxtSize.y + 3.f;

            NotifySize = ImVec2(
                Padding + ( BadgeRadius * 2.f ) + BadgeSpacing + TextWidth + Padding,
                ImMax( TextHeight, BadgeRadius * 2.f ) + ( Padding * 2.f ) );

            if ( Notify.GetCurrentState( ) == eState::In || Notify.GetCurrentState( ) == eState::Current )
            {
                NotifyAnim->second.SlideX = ImLerp( NotifyAnim->second.SlideX, NotifySize.x, ImGui::GetIO( ).DeltaTime * 8 );
                NotifyAnim->second.Alpha = ImLerp( NotifyAnim->second.Alpha, 1.f, ImGui::GetIO( ).DeltaTime * 8 );
            }

            if ( NotifyAnim->second.SlideX >= NotifySize.x - 1.f ) {
                Notify.SetState( eState::Current );
            }

            if ( Notify.GetCurrentState( ) == eState::Current && Notify.GetTimeDiff( ) > Notify.GetExpireTime( ) ) {
                Notify.SetState( eState::Out );
            }

            if ( Notify.GetCurrentState( ) == eState::Out )
            {
                NotifyAnim->second.SlideX = ImLerp( NotifyAnim->second.SlideX, 0.f, ImGui::GetIO( ).DeltaTime * 8 );
                NotifyAnim->second.Alpha = ImLerp( NotifyAnim->second.Alpha, 0.f, ImGui::GetIO( ).DeltaTime * 6 );
                if ( NotifyAnim->second.SlideX <= 2.f )
                {
                    Notify.SetState( eState::Expired );
                }
            }

            NotifyAnim->second.YPos = ImLerp( NotifyAnim->second.YPos, NextHeight, ImGui::GetIO( ).DeltaTime * 8 );

            float NotifyStartX = WindowSize.x - NotifySize.x - 16.f;
            float NotifyStartY = WindowSize.y - NotifySize.y - 16.f - NotifyAnim->second.YPos;


            if ( Notify.GetCurrentState( ) != eState::In )
            {
               // NotifyAnim->second.YPos = ImLerp( NotifyAnim->second.YPos, NotifyStartY, ImGui::GetIO( ).DeltaTime * 8 );
            }
            else {
               // NotifyAnim->second.YPos = NotifyStartY;
            }

            ImRect NotifyPos(
                ImVec2( NotifyStartX + NotifySize.x - NotifyAnim->second.SlideX, NotifyStartY ), // Min
                ImVec2( NotifyStartX + NotifySize.x - NotifyAnim->second.SlideX + NotifySize.x, NotifyStartY + NotifySize.y ) // Max
            );

            ImGui::PushStyleVar( ImGuiStyleVar_Alpha, NotifyAnim->second.Alpha );

            // Body
            DrawList->AddShadowRect( NotifyPos.Min, NotifyPos.Max, ImGui::GetColorU32( ImVec4( 0.f, 0.f, 0.f, 0.45f ) ), 22.f, ImVec2( 0.f, 6.f ), ImDrawFlags_ShadowCutOutShapeBackground, Rounding );
            DrawList->AddRectFilled( NotifyPos.Min, NotifyPos.Max, ImGui::GetColorU32( g_Col.NotifyBackground ), Rounding );
            DrawList->AddRect( NotifyPos.Min, NotifyPos.Max, ImGui::GetColorU32( g_Col.NotifyBorder ), Rounding );

            // Same travelling accent glow used by the cards and the profile panel
            Custom::GlowBorder( DrawList, NotifyPos.Min, NotifyPos.Max, Rounding, g_Col.Base, 1.f, 0.22f, NotifyAnim->second.Alpha );

            // Round icon badge
            ImVec2 BadgeCenter( NotifyPos.Min.x + Padding + BadgeRadius, NotifyPos.Min.y + NotifySize.y / 2.f );

            DrawList->AddCircleFilled( BadgeCenter, BadgeRadius, ImGui::GetColorU32( g_Col.NotifyIconCol ), 48 );

            auto GlyphSize = Utils::CalcTextSize( g_Variables.FontAwesomeSolidSmall, g_Variables.FontAwesomeSolidSmall->FontSize, ICON_FA_INFO );
            DrawList->AddText( g_Variables.FontAwesomeSolidSmall, g_Variables.FontAwesomeSolidSmall->FontSize,
                ImVec2( BadgeCenter.x - GlyphSize.x / 2.f, BadgeCenter.y - GlyphSize.y / 2.f ),
                ImGui::GetColorU32( g_Col.NotifyIconGlyph ), ICON_FA_INFO );

            // Title over description, vertically centered as a block
            const float TextX = NotifyPos.Min.x + Padding + ( BadgeRadius * 2.f ) + BadgeSpacing;
            const float TextY = NotifyPos.Min.y + ( NotifySize.y - TextHeight ) / 2.f;

            DrawList->AddText( g_Variables.m_FontNormal, g_Variables.m_FontNormal->FontSize, ImVec2( TextX, TextY ), ImGui::GetColorU32( g_Col.PrimaryText ), Notify.GetTitle( ).c_str( ) );
            DrawList->AddText( g_Variables.m_FontSecundary, g_Variables.m_FontSecundary->FontSize, ImVec2( TextX, TextY + TitleTxtSize.y + 3.f ), ImGui::GetColorU32( g_Col.FeaturesText ), Notify.GetDescription( ).c_str( ) );

            // Remaining lifetime, hairline pinned to the bottom edge
            float Progress = ImClamp( 1.0f - ( ( float ) Notify.GetTimeDiff( ) / ( float ) Notify.GetExpireTime( ) ), 0.f, 1.f );
            ImVec2 ProgressMin( NotifyPos.Min.x + Rounding, NotifyPos.Max.y - 2.f );
            ImVec2 ProgressMax( ProgressMin.x + ( ( NotifySize.x - Rounding * 2.f ) * Progress ), NotifyPos.Max.y - 1.f );

            DrawList->AddRectFilled( ProgressMin, ProgressMax, ImGui::GetColorU32( ImVec4( g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, 0.85f ) ), 1.f );

            ImGui::PopStyleVar( );

            NextHeight += NotifySize.y + 10.f;
        }
	}

    inline void Send( std::string Description, time_t ExpireTime )
    {
        NotifyManager::NotifyClass Notify( NotifyManager::eType::Info, ExpireTime );
        Notify.SetTitle( Utils::Translate( xorstr( "System Client Alert" ) ) );
        Notify.SetDescription( Description );
		std::lock_guard<std::mutex> Lock( NotifyMutex );
        NotifyList.push_back( Notify );
    }

}
