#pragma once
#include <Includes/includes.hpp>
#include <Includes/Utils.hpp>
#include <map>


namespace Custom {

	using namespace ImGui;

	inline double EaseInOutCirc( double t ) 
	{
		if ( t < 0.5 )
			return ( 1 - std::sqrt( 1 - 2 * t ) ) * 0.5;
		else
			return ( 1 + std::sqrt( 2 * t - 1 ) ) * 0.5;
	}

	// Shared page/subtab transition. Fades the current content out quickly, swaps it only when it is
	// fully invisible, then eases the new content back in. Returns the smoothstepped alpha so both
	// the opacity and the slide offset accelerate/decelerate instead of snapping.
	inline float PageTransition( float & Alpha, int & Current, int Target )
	{
		const float Delta = ImGui::GetIO( ).DeltaTime;

		if ( Current != Target )
		{
			Alpha = ImClamp( Alpha - Delta * g_MenuInfo.TransitionOut, 0.f, 1.f );

			if ( Alpha <= 0.001f )
				Current = Target;
		}
		else
		{
			Alpha = ImClamp( Alpha + Delta * g_MenuInfo.TransitionIn, 0.f, 1.f );
		}

		return Alpha * Alpha * ( 3.f - 2.f * Alpha );
	}

	inline void GlowBorder( ImDrawList * Draw, const ImVec2 & Min, const ImVec2 & Max, float Rounding,
		const ImVec4 & Color, float Thickness = 1.f, float Speed = 0.22f, float Intensity = 1.f )
	{
		if ( Intensity <= 0.01f || Max.x - Min.x < 4.f || Max.y - Min.y < 4.f )
			return;

		// AddLine offsets its vertices by half a pixel. This path therefore
		// lands on exactly the same pixels as the static AddRect border.
		Draw->PathClear( );
		Draw->PathRect( Min, Max - ImVec2( 1.f, 1.f ), Rounding );
		const int PathCount = Draw->_Path.Size;
		if ( PathCount < 3 )
		{
			Draw->PathClear( );
			return;
		}

		ImVector<ImVec2> Points;
		Points.resize( PathCount );
		memcpy( Points.Data, Draw->_Path.Data, sizeof( ImVec2 ) * PathCount );
		Draw->PathClear( );

		ImVector<float> Distances;
		Distances.resize( PathCount + 1 );
		Distances[ 0 ] = 0.f;
		for ( int i = 0; i < PathCount; ++i )
		{
			const ImVec2 Delta = Points[ ( i + 1 ) % PathCount ] - Points[ i ];
			Distances[ i + 1 ] = Distances[ i ] + ImSqrt( ImLengthSqr( Delta ) );
		}
		const float Perimeter = Distances[ PathCount ];
		if ( Perimeter < 1.f )
			return;

		auto PointAt = [&]( float Distance )
		{
			float Wrapped = ImFmod( Distance, Perimeter );
			if ( Wrapped < 0.f )
				Wrapped += Perimeter;
			for ( int i = 0; i < PathCount; ++i )
			{
				if ( Wrapped <= Distances[ i + 1 ] )
				{
					const float Length = Distances[ i + 1 ] - Distances[ i ];
					const float T = Length > 0.f ? ( Wrapped - Distances[ i ] ) / Length : 0.f;
					return ImLerp( Points[ i ], Points[ ( i + 1 ) % PathCount ], T );
				}
			}
			return Points[ 0 ];
		};

		// Short travelling segment: sample the rounded perimeter every pixel.
		const float Head = ImFmod( static_cast<float>( ImGui::GetTime() ) * Speed, 1.f ) * Perimeter;
		const float TailLength = ImClamp( Perimeter * 0.14f, 80.f, 150.f );
		const int Steps = static_cast<int>( ImCeil( TailLength ) );
		const float StepLength = TailLength / Steps;
		for ( int Step = 0; Step < Steps; ++Step )
		{
			const float T = ( Step + 0.5f ) / Steps;
			const float Fade = T * T * ( 3.f - 2.f * T );
			const float Alpha = Fade * Color.w * Intensity;
			if ( Alpha < 0.01f )
				continue;

			const ImVec2 P0 = PointAt( Head - TailLength + Step * StepLength );
			const ImVec2 P1 = PointAt( Head - TailLength + ( Step + 1 ) * StepLength );
			const float Highlight = 0.22f * T;
			const ImVec4 Gradient(
				ImLerp( Color.x, 1.f, Highlight ),
				ImLerp( Color.y, 1.f, Highlight ),
				ImLerp( Color.z, 1.f, Highlight ), Alpha );
			Draw->AddLine( P0, P1,
				ImGui::GetColorU32( ImVec4( Gradient.x, Gradient.y, Gradient.z, Alpha * 0.20f ) ),
				Thickness * 3.f );
			Draw->AddLine( P0, P1, ImGui::GetColorU32( Gradient ), Thickness * 1.35f );
		}
	}

	inline int rotation_start_index;
	inline void ImRotateStart( )
	{
		rotation_start_index = ImGui::GetWindowDrawList( )->VtxBuffer.Size;
	}

	inline ImVec2 ImRotationCenter( )
	{
		ImVec2 l( FLT_MAX, FLT_MAX ), u( -FLT_MAX, -FLT_MAX ); // bounds

		const auto & buf = ImGui::GetWindowDrawList( )->VtxBuffer;
		for ( int i = rotation_start_index; i < buf.Size; i++ )
			l = ImMin( l, buf[ i ].pos ), u = ImMax( u, buf[ i ].pos );

		return ImVec2( ( l.x + u.x ) / 2, ( l.y + u.y ) / 2 ); // or use _ClipRectStack?
	}

	inline void ImRotateEnd( float rad, ImVec2 center = ImRotationCenter( ) )
	{
		float s = sin( rad ), c = cos( rad );
		center = ImRotate( center, s, c ) - center;

		auto & buf = ImGui::GetWindowDrawList( )->VtxBuffer;
		for ( int i = rotation_start_index; i < buf.Size; i++ )
			buf[ i ].pos = ImRotate( buf[ i ].pos, s, c ) - center;
	}

	// Forward declarations: implemented further down this file, but DrawBackground (right below)
	// needs to call ProfileBar before that point in the source, and ProfileBar itself needs to
	// call into the language selector, which in turn needs the flag drawing helper.
	inline void DrawFlagIcon( ImDrawList * DrawList, const ImVec2 & pos, float size, Utils::Language Lang = Utils::g_CurrentLanguage );
	inline void LanguageSelector( ImDrawList * DrawList, const ImVec2 & PanelMin, const ImVec2 & PanelMax );
	inline void ProfileBar( );

	// Small, texture-free Weed mark. The old PNG was visually noisy at 16px
	// in the title bar, so keep the logo as a crisp W with a green outline.
	inline void DrawWeedMark( ImDrawList * Draw, const ImVec2 & Pos, float Size )
	{
		if ( !Draw || Size <= 0.f )
			return;

		const float Scale = Size / 16.f;
		const ImVec2 Points[ 5 ] = {
			ImVec2( Pos.x + 1.f * Scale,  Pos.y + 3.f * Scale ),
			ImVec2( Pos.x + 4.f * Scale,  Pos.y + 13.f * Scale ),
			ImVec2( Pos.x + 8.f * Scale,  Pos.y + 7.f * Scale ),
			ImVec2( Pos.x + 12.f * Scale, Pos.y + 13.f * Scale ),
			ImVec2( Pos.x + 15.f * Scale, Pos.y + 3.f * Scale )
		};

		const float Outline = ImMax( 1.f, Size * 0.20f );
		const float Stroke = ImMax( 1.f, Size * 0.105f );
		const ImU32 Green = ImGui::GetColorU32( g_Col.Base );
		const ImU32 White = ImGui::GetColorU32( g_Col.PrimaryText );

		for ( int Index = 0; Index < 4; ++Index )
		{
			Draw->AddLine( Points[ Index ], Points[ Index + 1 ], Green, Outline );
			Draw->AddLine( Points[ Index ], Points[ Index + 1 ], White, Stroke );
		}
	}

	inline void DrawBackground( bool Logged ) {
		const ImVec2 pos = ImGui::GetWindowPos( );
		ImDrawList * draw = ImGui::GetWindowDrawList( );

		const float Rounding = g_MenuInfo.WindowRounding;
		const ImVec2 Max = ImVec2( pos.x + g_MenuInfo.MenuSize.x, pos.y + g_MenuInfo.MenuSize.y );

		//Background
		draw->AddRectFilled( pos, Max, ImGui::GetColorU32( g_Col.BackgroundCol ), Rounding );

		if ( Logged )
		{
			const float SideBarW = g_MenuInfo.SideBarWidth;
			const ImVec2 SideMax = ImVec2( pos.x + SideBarW, Max.y );

			//SideBar
			draw->AddRectFilled( pos, SideMax, ImGui::GetColorU32( g_Col.SideBar ), Rounding, ImDrawFlags_RoundCornersLeft );

			draw->AddRectFilledMultiColor(
				ImVec2( pos.x + Rounding, pos.y + Rounding ),
				ImVec2( SideMax.x, pos.y + 60.f ),
				ImGui::GetColorU32( g_Col.SideBarTop ), ImGui::GetColorU32( g_Col.SideBarTop ),
				ImGui::GetColorU32( ImVec4( g_Col.SideBarTop.x, g_Col.SideBarTop.y, g_Col.SideBarTop.z, 0.f ) ),
				ImGui::GetColorU32( ImVec4( g_Col.SideBarTop.x, g_Col.SideBarTop.y, g_Col.SideBarTop.z, 0.f ) ) );

			//SideBar divider: comeca abaixo da barra de titulo
			const float TitleH = g_MenuInfo.TitleBarHeight;
			draw->AddLine( ImVec2( SideMax.x, pos.y + TitleH ), ImVec2( SideMax.x, SideMax.y - 1.f ), ImGui::GetColorU32( g_Col.SideBarBorder ), 1.f );

			// ---- Barra de titulo ----
			// Atravessa a janela inteira, por cima da barra lateral, como na
			// referencia. Cantos superiores arredondados para acompanhar a borda.
			draw->AddRectFilled(
				pos, ImVec2( Max.x, pos.y + TitleH ),
				ImGui::GetColorU32( g_Col.TitleBar ), Rounding, ImDrawFlags_RoundCornersTop );

			draw->AddLine(
				ImVec2( pos.x + 1.f, pos.y + TitleH ),
				ImVec2( Max.x - 1.f, pos.y + TitleH ),
				ImGui::GetColorU32( g_Col.TitleBarBorder ), 1.f );

			{
				const std::string Title = xorstr( "Weed Fivem 2.0" );
				ImFont * F = g_Variables.m_FontSecundary;
				const ImVec2 TS = F->CalcTextSizeA( F->FontSize, FLT_MAX, 0.f, Title.c_str( ) );

				draw->AddText( F, F->FontSize,
					ImVec2( pos.x + 12.f, pos.y + ( TitleH - TS.y ) * 0.5f ),
					ImGui::GetColorU32( g_Col.PrimaryText ), Title.c_str( ) );
			}

			ProfileBar( );
		}

		//Window Border
		draw->AddRect( pos, Max, ImGui::GetColorU32( g_Col.BorderCol ), Rounding );

		if ( Logged )
		{
			// A marca foi para a barra de titulo, canto direito -- na barra
			// lateral ela competia com o titulo por atencao no mesmo topo.
			const float LogoSize = 16.f;
			DrawWeedMark( draw,
				ImVec2( Max.x - LogoSize - 10.f,
					pos.y + ( g_MenuInfo.TitleBarHeight - LogoSize ) * 0.5f ),
				LogoSize );
		}
		else
		{
			// Login chrome is intentionally subtle: keep the mark small and
			// reserve the far-right corner for the close control.
			const float LogoSize = 16.f;
			DrawWeedMark( draw,
				ImVec2( Max.x - LogoSize - 44.f, pos.y + 7.f ),
				LogoSize );
		}
	}

	// Main category tab (sidebar): icon + label. The active state follows the
	// website: dark green surface, green icon/text and an animated right edge.
	inline bool MainTab( const char * icon, const char * label, bool active )
	{
		struct MainTab_t {
			ImVec4 BackgroundColor;
			ImVec4 BorderColor;
			ImVec4 IconColor;
			ImVec4 TextColor;
			float Indicator;
		};

		ImGuiWindow * window = ImGui::GetCurrentWindow( );
		ImDrawList * DrawList = window->DrawList;

		if ( window->SkipItems ) {
			return false;
		}

		ImGuiContext & g = *GImGui;
		const ImGuiStyle & style = g.Style;
		const ImGuiID id = window->GetID( label );
		const ImGuiIO IO = g.IO;

		const float Height = g_MenuInfo.TabHeight;
		const float Width = g_MenuInfo.TabWidth;
		const float PaddingX = 10.f;

		const ImVec2 pos = window->DC.CursorPos;
		const ImRect rect( pos, pos + ImVec2( Width, Height ) );
		ImGui::ItemSize( rect, 0.f );
		if ( !ImGui::ItemAdd( rect, id ) ) {
			return false;
		}

		bool hovered, held;
		bool pressed = ImGui::ButtonBehavior( rect, id, &hovered, &held );
		if ( pressed ) { ImGui::MarkItemEdited( id ); }

		static std::map<ImGuiID, MainTab_t> anim;
		auto TabAnim = anim.find( id );

		if ( TabAnim == anim.end( ) ) {
			anim.insert( { id, MainTab_t( ) } );
			TabAnim = anim.find( id );
		}

		float NormalizedTime = ImClamp( IO.DeltaTime * 10.f, 0.0f, 1.0f );

		ImVec4 TargetBackground = active
			? ImVec4( g_Col.ActiveItemCol )
			: hovered
				? ImVec4( g_Col.HoverCol )
				: ImVec4( g_Col.HoverCol.x, g_Col.HoverCol.y, g_Col.HoverCol.z, 0.f );

		ImVec4 TargetBorder = active
			? ImVec4( g_Col.ActiveBorderCol.x, g_Col.ActiveBorderCol.y, g_Col.ActiveBorderCol.z, 0.22f )
			: ImVec4( g_Col.ActiveBorderCol.x, g_Col.ActiveBorderCol.y, g_Col.ActiveBorderCol.z, 0.f );

		TabAnim->second.BackgroundColor = ImLerp( TabAnim->second.BackgroundColor, TargetBackground, Custom::EaseInOutCirc( NormalizedTime ) );
		TabAnim->second.BorderColor = ImLerp( TabAnim->second.BorderColor, TargetBorder, Custom::EaseInOutCirc( NormalizedTime ) );
		TabAnim->second.IconColor = ImLerp( TabAnim->second.IconColor, active ? ImColor( g_Col.BaseLight ) : hovered ? ImColor( g_Col.IconCol ) : ImColor( g_Col.IconDisabledCol ), Custom::EaseInOutCirc( NormalizedTime ) );
		TabAnim->second.TextColor = ImLerp( TabAnim->second.TextColor, active ? ImColor( g_Col.PrimaryText ) : hovered ? ImColor( g_Col.FeaturesText ) : ImColor( g_Col.SecundaryText ), Custom::EaseInOutCirc( NormalizedTime ) );
		TabAnim->second.Indicator = ImLerp( TabAnim->second.Indicator, active ? 1.f : 0.f, IO.DeltaTime * 12.f );

		const float rounding = g_MenuInfo.ControlRounding;
		DrawList->AddRectFilled( rect.Min, rect.Max, ImGui::GetColorU32( TabAnim->second.BackgroundColor ), rounding );
		DrawList->AddRect( rect.Min, rect.Max, ImGui::GetColorU32( TabAnim->second.BorderColor ), rounding );

		// Right edge indicator, scales vertically from the center. The web
		// panel uses a full-height indicator.
		if ( TabAnim->second.Indicator > 0.01f )
		{
			const float HalfBar = ( Height * 0.50f ) * TabAnim->second.Indicator;
			const float CenterY = rect.Min.y + Height / 2.f;
			DrawList->AddRectFilled(
				ImVec2( rect.Max.x - 2.f, CenterY - HalfBar ),
				ImVec2( rect.Max.x, CenterY + HalfBar ),
				ImGui::GetColorU32( ImVec4( g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, TabAnim->second.Indicator ) ), 1.f );
		}

		auto IconTextSize = Utils::CalcTextSize( g_Variables.FontAwesomeSolidSmall, g_Variables.FontAwesomeSolidSmall->FontSize, icon );
		DrawList->AddText( g_Variables.FontAwesomeSolidSmall, g_Variables.FontAwesomeSolidSmall->FontSize,
			ImVec2( rect.Min.x + PaddingX, pos.y + ( Height - IconTextSize.y ) / 2.f ),
			ImGui::GetColorU32( TabAnim->second.IconColor ), icon );

		auto TextSize = Utils::CalcTextSize( g_Variables.m_FontNormal, g_Variables.m_FontNormal->FontSize, label );
		DrawList->AddText( g_Variables.m_FontNormal, g_Variables.m_FontNormal->FontSize,
			ImVec2( rect.Min.x + PaddingX + IconTextSize.x + 8.f, pos.y + ( Height - TextSize.y ) / 2.f ),
			ImGui::GetColorU32( TabAnim->second.TextColor ), label );

		IMGUI_TEST_ENGINE_ITEM_INFO( id, label, g.LastItemData.StatusFlags );
		return pressed;
	}

	// Subtab list item (sidebar): bullet + text only. No background, no border, no pill.
	// Inactive: gray bullet + gray text. Hover: white bullet + white text.
	// Active: green bullet + light green text.
	// Recessed ~30px from the sidebar's left margin to create hierarchy under the active MainTab.
	inline bool SubTabItem( const char * label, bool active )
	{
		struct SubTabItem_t {
			ImVec4 BulletColor;
			ImVec4 TextColor;
		};

		ImGuiWindow * window = ImGui::GetCurrentWindow( );
		ImDrawList * DrawList = window->DrawList;

		if ( window->SkipItems ) {
			return false;
		}

		ImGuiContext & g = *GImGui;
		const ImGuiStyle & style = g.Style;
		std::string IdStr = std::string( xorstr( "##subtabitem_" ) ) + label;
		const ImGuiID id = window->GetID( IdStr.c_str( ) );
		const ImGuiIO IO = g.IO;

		const float IndentLeft = g_MenuInfo.SubTabIndent;
		const float BulletDiameter = 6.f;
		const float BulletTextSpacing = 9.f;
		const float Height = g_MenuInfo.SubTabHeight;

		const ImVec2 pos = window->DC.CursorPos;
		auto TextSize = Utils::CalcTextSize( g_Variables.m_FontSecundary, g_Variables.m_FontSecundary->FontSize, label );
		const float Width = IndentLeft + BulletDiameter + BulletTextSpacing + TextSize.x + 6.f;

		const ImRect rect( pos, pos + ImVec2( Width, Height ) );
		ImGui::ItemSize( ImVec2( Width, Height ), 0.f );
		if ( !ImGui::ItemAdd( rect, id ) ) {
			return false;
		}

		bool hovered, held;
		bool pressed = ImGui::ButtonBehavior( rect, id, &hovered, &held );
		if ( pressed ) { ImGui::MarkItemEdited( id ); }

		static std::map<ImGuiID, SubTabItem_t> anim;
		auto ItemAnim = anim.find( id );

		if ( ItemAnim == anim.end( ) ) {
			anim.insert( { id, SubTabItem_t( ) } );
			ItemAnim = anim.find( id );
		}

		float NormalizedTime = ImClamp( IO.DeltaTime * 10.f, 0.0f, 1.0f );

		ImVec4 TargetBulletColor = active ? ImVec4( g_Col.Base ) : hovered ? ImVec4( g_Col.FeaturesText ) : ImVec4( g_Col.IconDisabledCol );
		ImVec4 TargetTextColor = active ? ImVec4( g_Col.BaseLight ) : hovered ? ImVec4( g_Col.FeaturesText ) : ImVec4( g_Col.SecundaryText );

		ItemAnim->second.BulletColor = ImLerp( ItemAnim->second.BulletColor, TargetBulletColor, Custom::EaseInOutCirc( NormalizedTime ) );
		ItemAnim->second.TextColor = ImLerp( ItemAnim->second.TextColor, TargetTextColor, Custom::EaseInOutCirc( NormalizedTime ) );

		ImVec2 BulletCenter( rect.Min.x + IndentLeft + BulletDiameter / 2.f, rect.Min.y + Height / 2.f );

		if ( active )
			DrawList->AddCircleFilled( BulletCenter, BulletDiameter / 2.f + 2.5f, ImGui::GetColorU32( ImVec4( g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, 0.18f ) ), 16 );

		DrawList->AddCircleFilled( BulletCenter, BulletDiameter / 2.f, ImGui::GetColorU32( ItemAnim->second.BulletColor ), 16 );

		ImVec2 TextPos( BulletCenter.x + BulletDiameter / 2.f + BulletTextSpacing, rect.Min.y + ( ( Height - TextSize.y ) / 2.f ) - 0.5f );
		DrawList->AddText( g_Variables.m_FontSecundary, g_Variables.m_FontSecundary->FontSize, TextPos, ImGui::GetColorU32( ItemAnim->second.TextColor ), label );

		IMGUI_TEST_ENGINE_ITEM_INFO( id, label, g.LastItemData.StatusFlags );
		return pressed;
	}

	//////////////////////////////////////////////////////////////////////////
	// Card container: every page panel shares the exact same body, header,
	// border, shadow, rounding and padding so no panel ever looks different.
	//////////////////////////////////////////////////////////////////////////

	inline int CardColumn = 0;
	inline int CardLastColumns = 1;
	inline float CardSlideY = 0.f;

	// Origem e largura da grade de cards. Sao variaveis (e nao g_MenuInfo
	// direto) porque BeginScrollCards abre um filho e la dentro SetCursorPos e
	// relativo ao filho: a origem tem que virar (0,0) e a largura tem que abrir
	// espaco para a barra de rolagem.
	inline float CardOriginX = 0.f;
	inline float CardOriginY = 0.f;
	inline float CardWidthCur = 0.f;

	// Espaco reservado a direita para a barra de rolagem, tirado da largura das
	// colunas -- assim a regiao ocupa exatamente a mesma caixa das paginas sem
	// scroll e nada encosta na borda da janela.
	inline constexpr float CardScrollGutter = 12.f;

	// Called once per page before laying out its cards.
	inline void BeginCards( float SlideY = 0.f )
	{
		CardColumn = 0;
		CardSlideY = SlideY;
		CardOriginX = g_MenuInfo.CardX;
		CardOriginY = g_MenuInfo.CardY;
		CardWidthCur = g_MenuInfo.CardWidth;
	}

	// Grade de cards rolavel: mesma caixa das paginas normais, mas o conteudo
	// pode passar da altura visivel. Use HeightOverride/OffsetY nos cards --
	// dentro dela CardHeight (altura cheia da coluna) nao faz sentido.
	inline void BeginScrollCards( float SlideY = 0.f )
	{
		const float AreaW = g_MenuInfo.CardWidth * 2.f + g_MenuInfo.CardGap;

		ImGui::SetCursorPos( ImVec2( g_MenuInfo.CardX, g_MenuInfo.CardY ) );

		ImGui::PushStyleColor( ImGuiCol_ChildBg, ImVec4( 0.f, 0.f, 0.f, 0.f ) );
		ImGui::BeginChild( xorstr( "##scrollcards" ), ImVec2( AreaW, g_MenuInfo.CardHeight ), false );

		CardColumn = 0;
		CardSlideY = SlideY;
		CardOriginX = 0.f;
		CardOriginY = 0.f;
		// Duas colunas dividem o que sobra depois da calha da rolagem.
		CardWidthCur = g_MenuInfo.CardWidth - CardScrollGutter / 2.f;
	}

	// Linha de cor no mesmo ritmo das outras linhas do menu: rotulo a
	// esquerda, amostra ancorada a direita, na mesma coluna dos checkboxes.
	//
	// O ColorEdit4 do ImGui desenhava a amostra com a altura de frame dele,
	// que e maior que a linha de 21px do menu -- por isso as amostras saiam
	// encavaladas umas nas outras e desalinhadas do proprio rotulo.
	//
	// O float[4] e local: cada bloco da pagina tinha o seu como static,
	// inicializado uma unica vez, entao importar outro config depois nao
	// atualizava a amostra, que continuava mostrando a cor antiga.
	inline bool ColorRow( const char * Label, ImColor & Col )
	{
		ImGuiWindow * Window = ImGui::GetCurrentWindow( );
		if ( Window->SkipItems )
			return false;

		ImGuiContext & g = *GImGui;

		const float Width = ImGui::GetWindowContentRegionMax( ).x - ImGui::GetWindowContentRegionMin( ).x - ( Window->ScrollbarY ? 2.f : 0.f );
		const float RowHeight = g_MenuInfo.RowHeight;

		const ImVec2 Pos = Window->DC.CursorPos;
		const ImRect Rect( Pos, Pos + ImVec2( Width, RowHeight ) );

		ImGui::PushID( Label );

		const ImGuiID Id = Window->GetID( xorstr( "##swatch" ) );

		ImGui::ItemSize( Rect, 0.f );
		if ( !ImGui::ItemAdd( Rect, Id ) )
		{
			ImGui::PopID( );
			return false;
		}

		// Circulo, nao retangulo: o quadrado arredondado ainda lia como caixa
		// ao lado dos checkboxes.
		const float Diameter = g_MenuInfo.CheckBoxSize;
		const float Radius = Diameter * 0.5f;
		const ImVec2 Center( Rect.Max.x - Radius, Rect.Min.y + RowHeight * 0.5f );

		bool Hovered, Held;
		if ( ImGui::ButtonBehavior( Rect, Id, &Hovered, &Held ) )
			ImGui::OpenPopup( xorstr( "##picker" ) );

		// Xadrez por baixo: sem ele uma cor quase transparente fica igual a uma
		// opaca escura, e nao da para saber qual e qual.
		// Base opaca por baixo para uma cor quase transparente nao sumir contra
		// o fundo do card -- o xadrez do ImGui so existe para retangulo.
		Window->DrawList->AddCircleFilled( Center, Radius, ImGui::GetColorU32( ImVec4( g_Col.TrackCol ) ), 32 );
		Window->DrawList->AddCircleFilled( Center, Radius, ImGui::GetColorU32( Col.Value ), 32 );
		Window->DrawList->AddCircle( Center, Radius, ImGui::GetColorU32( ImVec4( g_Col.InputBorder ) ), 32 );

		const ImVec2 TextSize = ImGui::CalcTextSize( Label );
		Window->DrawList->AddText( ImVec2( Rect.Min.x, Rect.Min.y + ( RowHeight - TextSize.y ) / 2.f ),
			ImGui::GetColorU32( Hovered ? ImVec4( g_Col.FeaturesText ) : ImVec4( g_Col.SecundaryFeaturesText ) ), Label );

		bool Changed = false;

		if ( ImGui::BeginPopup( xorstr( "##picker" ) ) )
		{
			float V[ 4 ] = { Col.Value.x, Col.Value.y, Col.Value.z, Col.Value.w };

			if ( ImGui::ColorPicker4( xorstr( "##p" ), V, ImGuiColorEditFlags_AlphaBar ) )
			{
				Col.Value = ImVec4( V[ 0 ], V[ 1 ], V[ 2 ], V[ 3 ] );
				Changed = true;
			}

			ImGui::EndPopup( );
		}

		ImGui::PopID( );
		return Changed;
	}

	// cColors guarda os acentos como ImVec4, enquanto as cores de configuracao
	// sao ImColor. Faz a ponte sem mexer no widget acima e sem alterar o valor
	// quando o picker nao e usado.
	inline bool ColorRow( const char * Label, ImVec4 & Col )
	{
		ImColor Temp = Col;
		if ( !ColorRow( Label, Temp ) )
			return false;
		Col = Temp.Value;
		return true;
	}

	// Slider de FOV: 0 a 180, linear.
	//
	// NoRoundToFormat e o ponto todo: sem ele a ImGui arredondaria o float para
	// o inteiro que aparece no "%.0f", e todo valor dentro do mesmo numero
	// viraria o mesmo tamanho de circulo. Com ele o valor guarda a fracao,
	// entao duas posicoes que mostram "0" podem desenhar circulos diferentes.
	inline bool FovSlider( const char * Label, float * Fov )
	{
		return ImGui::SliderFloat( Label, Fov, 0.f, FovShownMax, xorstr( "%.0f" ),
			ImGuiSliderFlags_NoRoundToFormat );
	}

	// Volta para a primeira coluna. A grade rolavel dispoe os cards em linhas,
	// e nao numa fileira unica como as paginas normais.
	inline void CardRow( ) { CardColumn = 0; }

	// TotalHeight fixa a altura do conteudo: os cards sao desenhados por
	// SetCursorPos absoluto, entao o ImGui so descobre a extensao real pelo
	// item que fecha a regiao.
	inline void EndScrollCards( float TotalHeight )
	{
		ImGui::SetCursorPos( ImVec2( 0.f, TotalHeight ) );
		ImGui::Dummy( ImVec2( 1.f, 1.f ) );

		ImGui::EndChild( );
		ImGui::PopStyleColor( );

		CardOriginX = g_MenuInfo.CardX;
		CardOriginY = g_MenuInfo.CardY;
		CardWidthCur = g_MenuInfo.CardWidth;
	}

	// Columns          : how many grid columns the card spans
	// HeightOverride   : 0 uses the full column height
	// OffsetY          : lets a column hold several stacked cards
	// AdvanceColumn    : false keeps the next card in the same column
	inline void BeginCard( const char * Label, int Columns = 1, float HeightOverride = 0.f, float OffsetY = 0.f, bool AdvanceColumn = true, const char * Icon = nullptr, ImGuiWindowFlags ChildFlags = 0 )
	{
		ImGuiWindow * Window = ImGui::GetCurrentWindow( );
		ImDrawList * Draw = Window->DrawList;

		const float Width = CardWidthCur * Columns + g_MenuInfo.CardGap * ( Columns - 1 );
		const float Height = HeightOverride > 0.f ? HeightOverride : g_MenuInfo.CardHeight;
		const float HeaderH = g_MenuInfo.CardHeaderHeight;
		const float Rounding = g_MenuInfo.CardRounding;

		CardLastColumns = AdvanceColumn ? Columns : 0;

		ImGui::SetCursorPos( ImVec2(
			CardOriginX + CardColumn * ( CardWidthCur + g_MenuInfo.CardGap ),
			CardOriginY + CardSlideY + OffsetY ) );

		const ImVec2 Min = Window->DC.CursorPos;
		const ImVec2 Max = Min + ImVec2( Width, Height );
		const ImVec2 HeaderMax = ImVec2( Max.x, Min.y + HeaderH );

		// Depth
		Draw->AddShadowRect( Min, Max, ImGui::GetColorU32( ImVec4( 0.f, 0.f, 0.f, 0.30f ) ), 12.f, ImVec2( 0.f, 2.f ), ImDrawFlags_ShadowCutOutShapeBackground, Rounding );

		// Body
		Draw->AddRectFilled( Min, Max, ImGui::GetColorU32( g_Col.ChildCol ), Rounding );

		// Header base + subtle green wash across the rounded top corners.
		Draw->AddRectFilled( Min, HeaderMax, ImGui::GetColorU32( g_Col.HeaderRightCol ), Rounding, ImDrawFlags_RoundCornersTop );
		Draw->AddRectFilledMultiColor(
			Min, HeaderMax,
			ImGui::GetColorU32( g_Col.HeaderLeftCol ),
			ImGui::GetColorU32( ImVec4( g_Col.HeaderLeftCol.x, g_Col.HeaderLeftCol.y, g_Col.HeaderLeftCol.z, 0.f ) ),
			ImGui::GetColorU32( ImVec4( g_Col.HeaderLeftCol.x, g_Col.HeaderLeftCol.y, g_Col.HeaderLeftCol.z, 0.f ) ),
			ImGui::GetColorU32( g_Col.HeaderLeftCol ), Rounding, ImDrawFlags_RoundCornersTop );

		Draw->AddLine( ImVec2( Min.x + 1.f, HeaderMax.y ), ImVec2( Max.x - 1.f, HeaderMax.y ), ImGui::GetColorU32( ImVec4( g_Col.LinesCol.x, g_Col.LinesCol.y, g_Col.LinesCol.z, 0.35f ) ), 1.f );

		// Header content
		const char * LabelEnd = ImGui::FindRenderedTextEnd( Label );

		ImVec2 TextSize = Utils::CalcTextSize( g_Variables.m_FontSecundary, g_Variables.m_FontSecundary->FontSize, Label );
		float TextX = Min.x + g_MenuInfo.CardPadding;

		if ( Icon && Icon[ 0 ] )
		{
			const ImVec2 IconSize = Utils::CalcTextSize( g_Variables.FontAwesomeSolidSmall, g_Variables.FontAwesomeSolidSmall->FontSize, Icon );
			Draw->AddText( g_Variables.FontAwesomeSolidSmall, g_Variables.FontAwesomeSolidSmall->FontSize,
				ImVec2( TextX, Min.y + ( HeaderH - IconSize.y ) / 2.f ),
				ImGui::GetColorU32( g_Col.BaseLight ), Icon );
			TextX += IconSize.x + 7.f;
		}

		Draw->AddText( g_Variables.m_FontSecundary, g_Variables.m_FontSecundary->FontSize,
			ImVec2( TextX, Min.y + ( HeaderH - TextSize.y ) / 2.f ),
			ImGui::GetColorU32( g_Col.PrimaryText ), Label, LabelEnd );

		// Border last so it sits above the fills, then the travelling accent glow on top of it
		Draw->AddRect( Min, Max, ImGui::GetColorU32( g_Col.ChildBorderCol ), Rounding );
		GlowBorder( Draw, Min, Max, Rounding, g_Col.Base );

		// Child is inset by 1px on every side so the border stays crisp
		ImGui::SetCursorScreenPos( ImVec2( Min.x, HeaderMax.y + 1.f ) );

		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( g_MenuInfo.CardPadding, g_MenuInfo.CardPadding ) );
		ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 8.f, g_MenuInfo.RowSpacing ) );
		ImGui::PushStyleColor( ImGuiCol_ChildBg, ImVec4( 0.f, 0.f, 0.f, 0.f ) );

		const float ChildWidth = Width;
		const float ChildHeight = Height - HeaderH - 2.f;

		ImGui::BeginChild( Label, ImVec2( ChildWidth, ChildHeight ), false,
			ImGuiWindowFlags_AlwaysUseWindowPadding | ChildFlags );
		ImGui::PushFont( g_Variables.m_FontSmaller );
	}

	inline void EndCard( )
	{
		ImGui::PopFont( );
		ImGui::EndChild( );

		ImGui::PopStyleColor( );
		ImGui::PopStyleVar( 2 );

		CardColumn += CardLastColumns;
	}

	// Muted group title used inside cards ("Globals", "Globals Vector", "Settings"...).
	inline void SectionLabel( const char * Label )
	{
		ImGuiWindow * Window = ImGui::GetCurrentWindow( );
		if ( Window->SkipItems )
			return;

		ImVec2 TextSize = Utils::CalcTextSize( g_Variables.m_FontSmaller, g_Variables.m_FontSmaller->FontSize, Label );
		const ImVec2 Pos = Window->DC.CursorPos;

		// Section titles breathe a little more than regular rows
		ImGui::ItemSize( ImVec2( TextSize.x, TextSize.y + 7.f ), 0.f );

		Window->DrawList->AddText( g_Variables.m_FontSmaller, g_Variables.m_FontSmaller->FontSize, Pos,
			ImGui::GetColorU32( g_Col.SecundaryText ), Label );

		const float LineStart = Pos.x + TextSize.x + 9.f;
		const float LineEnd = Window->Pos.x + ImGui::GetWindowContentRegionMax( ).x;
		if ( LineEnd > LineStart )
			Window->DrawList->AddLine(
				ImVec2( LineStart, Pos.y + TextSize.y * 0.5f ),
				ImVec2( LineEnd, Pos.y + TextSize.y * 0.5f ),
				ImGui::GetColorU32( ImVec4( g_Col.LinesCol.x, g_Col.LinesCol.y,
					g_Col.LinesCol.z, 0.30f ) ) );
	}

	//////////////////////////////////////////////////////////////////////////
	// Single switch renderer shared by every boolean row (Toggle, CheckBox,
	// CheckBoxCfg, CheckBoxPage) so all of them animate and look identical.
	//////////////////////////////////////////////////////////////////////////
	// Caixa QUADRADA arredondada com a marca de confirmacao, ancorada na
	// direita da linha.
	//
	// A marca vem do RenderCheckMark do proprio ImGui em vez de dois AddLine
	// desenhados a mao: as proporcoes e o centro dele ja estao certos, enquanto
	// os tracos manuais sairam tortos e fora de centro.
	//
	// Knob e o quanto a marca ja apareceu (0..1); o nome ficou porque os
	// widgets chamadores guardam esse float.
	inline void DrawSwitch( ImDrawList * Draw, const ImRect & Row, float RowHeight, bool Checked, bool Hovered,
		float & Knob, float & Glow, ImVec4 & TrackColor, ImVec4 & KnobColor )
	{
		ImGuiContext & g = *GImGui;

		const float Size = g_MenuInfo.CheckBoxSize;
		const float Rounding = 4.f;

		Knob = ImClamp( ImLerp( Knob, Checked ? 1.f : 0.f, g.IO.DeltaTime * 16.f ), 0.f, 1.f );
		Glow = ImClamp( ImLerp( Glow, Checked ? ( Hovered ? 0.35f : 0.22f ) : 0.f, g.IO.DeltaTime * 10.f ), 0.f, 1.f );

		TrackColor = ImLerp( TrackColor, Checked ? ImVec4( Hovered ? g_Col.ToggleOnHover : g_Col.ToggleOn )
			: Hovered ? ImVec4( ImColor( 56, 56, 63 ) ) : ImVec4( g_Col.TrackCol ), g.IO.DeltaTime * 10.f );
		KnobColor = ImLerp( KnobColor, ImVec4( g_Col.ToggleMark ), g.IO.DeltaTime * 10.f );

		const ImVec2 Min( Row.Min.x, Row.Min.y + ( RowHeight - Size ) / 2.f );
		const ImVec2 Max = Min + ImVec2( Size, Size );

		if ( Glow > 0.01f )
			Draw->AddRectFilled( Min - ImVec2( 2.f, 2.f ), Max + ImVec2( 2.f, 2.f ),
				ImGui::GetColorU32( ImVec4( g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, Glow * 0.45f ) ), Rounding + 2.f );

		Draw->AddRectFilled( Min, Max, ImGui::GetColorU32( TrackColor ), Rounding );

		if ( Knob <= 0.01f )
		{
			Draw->AddRect( Min, Max, ImGui::GetColorU32( ImVec4( g_Col.InputBorder ) ), Rounding );
			return;
		}

		// Mesmo recuo que o checkbox nativo do ImGui usa, para a marca ficar
		// centrada com folga igual dos quatro lados.
		const float Pad = ImMax( 1.f, IM_FLOOR( Size / 6.f ) );

		ImGui::RenderCheckMark( Draw, Min + ImVec2( Pad, Pad ),
			ImGui::GetColorU32( ImVec4( KnobColor.x, KnobColor.y, KnobColor.z, Knob ) ),
			Size - Pad * 2.f );
	}

	// Descricao da funcao: aparece so depois de ~1s com o mouse parado em cima
	// da linha, quebrada em varias linhas se precisar.
	//
	// Vai no ForegroundDrawList de proposito: a linha do toggle vive dentro do
	// child do card, e qualquer coisa desenhada no draw list da janela seria
	// cortada na borda do card.
	inline constexpr float DescTipDelay = 0.65f;   // segundos parado em cima
	inline constexpr float DescTipWrap = 260.f;    // largura maxima do texto

	inline void DescTooltip( ImGuiID Id, const char * Desc, bool Hovered )
	{
		if ( !Desc || !Desc[ 0 ] )
			return;

		Desc = Utils::Translate( Desc );

		struct DescTip_t {
			float Hold = 0.f;
			float Alpha = 0.f;
		};

		static std::map<ImGuiID, DescTip_t> anim;

		auto It = anim.find( Id );
		if ( It == anim.end( ) )
			It = anim.insert( { Id, DescTip_t( ) } ).first;

		ImGuiIO & IO = ImGui::GetIO( );

		// Zera assim que o mouse sai: passar de raspao por cima nao acumula
		// tempo, tem que ficar parado o intervalo inteiro.
		It->second.Hold = Hovered ? It->second.Hold + IO.DeltaTime : 0.f;

		const bool Show = It->second.Hold >= DescTipDelay;
		It->second.Alpha = ImClamp( ImLerp( It->second.Alpha, Show ? 1.f : 0.f, IO.DeltaTime * 12.f ), 0.f, 1.f );

		if ( It->second.Alpha <= 0.01f )
			return;

		ImFont * Font = g_Variables.m_FontSmaller;
		const float FontSize = Font->FontSize;
		const ImVec2 Text = Font->CalcTextSizeA( FontSize, FLT_MAX, DescTipWrap, Desc );

		const float Pad = 8.f;
		ImVec2 Min = ImGui::GetMousePos( ) + ImVec2( 14.f, 18.f );
		ImVec2 Max = Min + ImVec2( Text.x + Pad * 2.f, Text.y + Pad * 2.f );

		// Empurra de volta para dentro da tela quando a caixa passa da borda.
		const ImVec2 Screen = IO.DisplaySize;
		if ( Max.x > Screen.x - 4.f ) { const float d = Max.x - ( Screen.x - 4.f ); Min.x -= d; Max.x -= d; }
		if ( Max.y > Screen.y - 4.f ) { const float d = Max.y - ( Screen.y - 4.f ); Min.y -= d; Max.y -= d; }

		const float A = It->second.Alpha;
		ImDrawList * Draw = ImGui::GetForegroundDrawList( );

		Draw->AddShadowRect( Min, Max, ImGui::GetColorU32( ImVec4( 0.f, 0.f, 0.f, 0.45f * A ) ), 14.f, ImVec2( 0.f, 3.f ), ImDrawFlags_ShadowCutOutShapeBackground, 4.f );
		Draw->AddRectFilled( Min, Max, ImGui::GetColorU32( ImVec4( g_Col.NotifyBackground.x, g_Col.NotifyBackground.y, g_Col.NotifyBackground.z, A ) ), 4.f );
		Draw->AddRect( Min, Max, ImGui::GetColorU32( ImVec4( g_Col.NotifyBorder.x, g_Col.NotifyBorder.y, g_Col.NotifyBorder.z, A ) ), 4.f );

		Draw->AddText( Font, FontSize, ImVec2( Min.x + Pad, Min.y + Pad ),
			ImGui::GetColorU32( ImVec4( g_Col.FeaturesText.x, g_Col.FeaturesText.y, g_Col.FeaturesText.z, A ) ),
			Desc, nullptr, DescTipWrap );
	}

	// Pill toggle: label on the left, animated track/knob pinned to the right edge.
	inline bool Toggle( const char * Label, bool * Value, const char * Desc = nullptr )
	{
		struct Toggle_t {
			ImVec4 TrackColor = g_Col.TrackCol;
			ImVec4 KnobColor = ImColor( 122, 122, 132 );
			ImVec4 LabelColor = g_Col.SecundaryFeaturesText;
			float Knob = 0.f;
			float Glow = 0.f;
		};

		ImGuiWindow * Window = ImGui::GetCurrentWindow( );
		if ( Window->SkipItems )
			return false;

		ImGuiContext & g = *GImGui;
		const ImGuiStyle & style = g.Style;

		std::string UniqueID = ( std::string ) Label + std::to_string( ( int ) Value );
		const ImGuiID id = Window->GetID( UniqueID.c_str( ) );

		const float Width = ImGui::GetWindowContentRegionMax( ).x - ImGui::GetWindowContentRegionMin( ).x - ( Window->ScrollbarY ? 2.f : 0.f );
		const float RowHeight = g_MenuInfo.RowHeight;

		const ImVec2 Pos = Window->DC.CursorPos;
		const ImRect Rect( Pos, Pos + ImVec2( Width, RowHeight ) );

		ImGui::ItemSize( Rect, 0.f );
		if ( !ImGui::ItemAdd( Rect, id ) )
			return false;

		bool Hovered, Held;
		bool Pressed = ImGui::ButtonBehavior( Rect, id, &Hovered, &Held );
		if ( Pressed ) {
			*Value = !( *Value );
			ImGui::MarkItemEdited( id );
		}

		static std::map<ImGuiID, Toggle_t> anim;
		auto ToggleAnim = anim.find( id );

		if ( ToggleAnim == anim.end( ) ) {
			anim.insert( { id, Toggle_t( ) } );
			ToggleAnim = anim.find( id );
		}

		ToggleAnim->second.LabelColor = ImLerp( ToggleAnim->second.LabelColor, *Value ? ImVec4( g_Col.FeaturesText ) : ImVec4( g_Col.SecundaryFeaturesText ), g.IO.DeltaTime * 8.f );

		DrawSwitch( Window->DrawList, Rect, RowHeight, *Value, Hovered,
			ToggleAnim->second.Knob, ToggleAnim->second.Glow, ToggleAnim->second.TrackColor, ToggleAnim->second.KnobColor );

		ImVec2 TextSize = Utils::CalcTextSize( g_Variables.m_FontSmaller, g_Variables.m_FontSmaller->FontSize, Label );
		Window->DrawList->AddText( g_Variables.m_FontSmaller, g_Variables.m_FontSmaller->FontSize,
			ImVec2( Rect.Min.x + g_MenuInfo.CheckBoxSize + 9.f,
				Rect.Min.y + ( RowHeight - TextSize.y ) / 2.f ),
			ImGui::GetColorU32( ToggleAnim->second.LabelColor ), Label );

		DescTooltip( id, Desc, Hovered && g_MenuInfo.IsOpen );

		IMGUI_TEST_ENGINE_ITEM_INFO( id, Label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | ( *Value ? ImGuiItemStatusFlags_Checked : 0 ) );
		return Pressed;
	}

	inline void InvisibleShadow( const float alpha, const ImVec2 pos_min, const ImVec2 pos_max, ImU32 color, float shadow_tickness )
	{
		ImGui::PushStyleVar( ImGuiStyleVar_Alpha, alpha );
		ImGui::PushClipRect( pos_min, pos_max, true );
		ImGui::GetWindowDrawList( )->AddShadowCircle( ImVec2( ImGui::GetMousePos( ) ), 1.f, ImGui::GetColorU32( color ), shadow_tickness, ImVec2( 0, 0 ), 1000.f );
		ImGui::PopClipRect( );
		ImGui::PopStyleVar( );
	}

	inline void ToolTip( const char * Label, const char * Desc, const char * Icon, bool State ) {
		//bool Check = CheckBox( Label, Checked );
		bool Hovered = State;
		Desc = Utils::Translate( Desc );

		ImVec2 Pos = ImGui::GetMousePos( ) + ImVec2( 14, 0 );
		auto DrawList = ImGui::GetForegroundDrawList( );
		float Padding = 8;
		float Rounding = 3;
		float Spacing = 4;

		struct ToolTip_t {
			ImVec4 BackGroundColor = ImColor( 20, 20, 22 );
			ImVec4 TextColor = ImColor( 200, 200, 200, 0 );
			ImVec4 IconColor = ImColor( 245, 158, 66, 0 );

			float GlobalAlpha = 0.f;
			float SlideX = 0.f;
		};

		static std::map<std::string, ToolTip_t> anim;
		std::string a1 = std::string( Label ) + std::string( Desc );
		auto ToolTipAnim = anim.find( a1 );

		if ( ToolTipAnim == anim.end( ) ) {
			// Sem reatribuir o iterador ele continuava em end(), e todo o
			// resto da funcao desreferenciava o no sentinela do map.
			ToolTipAnim = anim.insert( { a1, ToolTip_t( ) } ).first;
		}

		ImVec2 TextSize = Utils::CalcTextSize( g_Variables.m_FontSmaller, g_Variables.m_FontSmaller->FontSize, Desc );
		ImVec2 IconTextSize = Utils::CalcTextSize( g_Variables.FontAwesomeSolidSmall, g_Variables.FontAwesomeSolidSmall->FontSize, Icon );

		ToolTipAnim->second.GlobalAlpha = ImLerp( ToolTipAnim->second.GlobalAlpha, Hovered ? 1.f : 0.f, ImGui::GetIO( ).DeltaTime * 8 );
		ToolTipAnim->second.SlideX = ImLerp( ToolTipAnim->second.SlideX, Hovered ? ( IconTextSize.x + TextSize.x + Padding + Spacing ) : 0.f, ImGui::GetIO( ).DeltaTime * 8 );
		ToolTipAnim->second.TextColor = ImLerp( ToolTipAnim->second.TextColor, Hovered ? ImColor( 180, 180, 180 ) : ImColor( 180, 180, 180, 0 ), ImGui::GetIO( ).DeltaTime * 12 );
		ToolTipAnim->second.IconColor = ImLerp( ToolTipAnim->second.IconColor, Hovered ? ImVec4( g_Col.Base ) : ImVec4( g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, 0.f / 255.f ), ImGui::GetIO( ).DeltaTime * 8 );

		ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ToolTipAnim->second.GlobalAlpha );


		ImVec2 RectEndPos = ImVec2( Pos.x + ToolTipAnim->second.SlideX, Pos.y + TextSize.y + Padding );
		DrawList->AddShadowRect( Pos, RectEndPos, ImGui::GetColorU32( ImVec4( 0.f, 0.f, 0.f, 0.45f ) ), 12.f, ImVec2( 0.f, 3.f ), ImDrawFlags_ShadowCutOutShapeBackground, Rounding );
		DrawList->AddRectFilled( Pos, RectEndPos, ImGui::GetColorU32( g_Col.NotifyBackground ), Rounding );
		DrawList->AddRect( Pos, RectEndPos, ImGui::GetColorU32( g_Col.NotifyBorder ), Rounding );


		DrawList->AddText( g_Variables.FontAwesomeSolidSmall, g_Variables.FontAwesomeSolidSmall->FontSize, ImVec2( Pos.x + Padding / 2, Pos.y + ( ( TextSize.y + Padding ) / 2 - IconTextSize.y / 2 ) ), ImGui::GetColorU32( ToolTipAnim->second.IconColor ), Icon );

		if ( ToolTipAnim->second.SlideX < TextSize.x + Padding ) {

			//ChatGPT patrocina
			std::string ClippedDesc = Desc;
			while ( ImGui::CalcTextSize( ClippedDesc.c_str( ) ).x > ToolTipAnim->second.SlideX - Padding && !ClippedDesc.empty( ) ) {
				ClippedDesc.pop_back( );
			}

			DrawList->AddText( g_Variables.m_FontSmaller, g_Variables.m_FontSmaller->FontSize, ImVec2( Pos.x + Padding / 2 + ( IconTextSize.x + Spacing ), Pos.y + Padding / 2 ), ImGui::GetColorU32( ToolTipAnim->second.TextColor ), ClippedDesc.c_str( ) );
		}
		else {
			DrawList->AddText( g_Variables.m_FontSmaller, g_Variables.m_FontSmaller->FontSize, ImVec2( Pos.x + Padding / 2 + ( IconTextSize.x + Spacing ), Pos.y + Padding / 2 ), ImGui::GetColorU32( ToolTipAnim->second.TextColor ), Desc );
		}

		ImGui::PopStyleVar( );

		//return Check;
	}

	inline bool CheckBox( const char * Label, bool * Checked, const char * Desc = nullptr, bool bToolTip = false, const char * ToolTipMsg = xorstr( "" ), const char * ToolTipIcon = xorstr( "" ) ) {
		ImGuiWindow * Window = ImGui::GetCurrentWindow( );
		if ( Window->SkipItems )
			return false;

		ImGuiContext & g = *GImGui;
		const ImGuiStyle & style = g.Style;

		std::string UniqueID = ( std::string ) Label + std::to_string( ( int ) Checked );
		const ImGuiID id = Window->GetID( UniqueID.c_str( ) );

		float Width = ImGui::GetWindowContentRegionMax( ).x - ImGui::GetWindowContentRegionMin( ).x - ( Window->ScrollbarY ? 2.f : 0.f );
		//if ( CustomWidth != 0.f ) {
		//	Width = CustomWidth - ImGui::GetWindowContentRegionMin( ).x - ( Window->ScrollbarY ? 2.f : 0.f );
		//}

		ImVec2 TextSize = ImGui::CalcTextSize( Label );
		const float RowHeight = g_MenuInfo.RowHeight;

		const ImVec2 Pos = Window->DC.CursorPos;
		const ImRect Rect( Pos, Pos + ImVec2( Width, RowHeight ) );
		const ImRect Clickable( Rect.Min, Rect.Max );

		ImGui::ItemSize( Rect, 0.f );

		if ( !ImGui::ItemAdd( Rect, id, &Clickable ) ) {
			IMGUI_TEST_ENGINE_ITEM_INFO( id, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | ( *v ? ImGuiItemStatusFlags_Checked : 0 ) );
			return false;
		}

		bool Hovered, Held;
		bool Pressed = ImGui::ButtonBehavior( Clickable, id, &Hovered, &Held );
		if ( Pressed ) {
			*Checked = !( *Checked );
			ImGui::MarkItemEdited( id );
		}

		struct WidCheckBox_t {
			ImVec4 TrackColor = g_Col.TrackCol;
			ImVec4 KnobColor = ImColor( 122, 122, 132 );
			ImVec4 LabelColor = g_Col.SecundaryFeaturesText;

			float Knob = 0.f;
			float Glow = 0.f;
		};

		static std::map<ImGuiID, WidCheckBox_t> anim;
		auto CheckBoxAnim = anim.find( id );

		if ( CheckBoxAnim == anim.end( ) )
		{
			anim.insert( { id, WidCheckBox_t( ) } );
			CheckBoxAnim = anim.find( id );
		}

		CheckBoxAnim->second.LabelColor = ImLerp( CheckBoxAnim->second.LabelColor, *Checked || Hovered ? ImVec4( g_Col.FeaturesText ) : ImVec4( g_Col.SecundaryFeaturesText ), g.IO.DeltaTime * 8.f );

		// Animated switch pinned to the right edge so every row reads label-left / control-right
		DrawSwitch( Window->DrawList, Rect, RowHeight, *Checked, Hovered,
			CheckBoxAnim->second.Knob, CheckBoxAnim->second.Glow, CheckBoxAnim->second.TrackColor, CheckBoxAnim->second.KnobColor );

		if ( bToolTip )
		{
			ToolTip( Label, ToolTipMsg, ToolTipIcon, Hovered && g_MenuInfo.IsOpen );
		}

		Window->DrawList->AddText(
			ImVec2( Rect.Min.x + g_MenuInfo.CheckBoxSize + 9.f,
				Rect.Min.y + ( RowHeight - TextSize.y ) / 2.f ),
			ImGui::GetColorU32( CheckBoxAnim->second.LabelColor ), Label );

		DescTooltip( id, Desc, Hovered && g_MenuInfo.IsOpen );

		//Window->DrawList->AddText( g_Variables.FontAwesomeSolid, g_Variables.FontAwesomeSolid->FontSize, ImVec2( Rect.Min.x + CheckBoxSize.x + 8.f, Pos.y + Rect.Max.y / 2 - ( Pos.y + TextSize.y ) / 2 ) + ImVec2( TextSize.x + 8, 0 ), ImGui::GetColorU32( ImVec4( ImColor( 204, 76, 67 ) ) ), ICON_FA_CIRCLE_EXCLAMATION );
		IMGUI_TEST_ENGINE_ITEM_INFO( id, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | ( *Checked ? ImGuiItemStatusFlags_Checked : 0 ) );
		return Pressed;
	}

	inline bool CheckBoxCfg( const char * Label, bool * Checked, const char * Desc, std::function<void( )> Components, bool bToolTip = false, const char * ToolTipMsg = xorstr( "" ), const char * ToolTipIcon = xorstr( "" ) ) {
		ImGuiWindow * Window = ImGui::GetCurrentWindow( );
		if ( Window->SkipItems )
			return false;

		ImGuiContext & g = *GImGui;
		const ImGuiStyle & style = g.Style;

		std::string UniqueID = ( std::string ) Label + std::to_string( ( int ) Checked );
		const ImGuiID id = Window->GetID( UniqueID.c_str( ) );

		float Width = ImGui::GetWindowContentRegionMax( ).x - ImGui::GetWindowContentRegionMin( ).x - ( Window->ScrollbarY ? 2.f : 0.f );

		ImVec2 TextSize = ImGui::CalcTextSize( Label );
		const float RowHeight = g_MenuInfo.RowHeight;

		const ImVec2 Pos = Window->DC.CursorPos;
		const ImRect Rect( Pos, Pos + ImVec2( Width, RowHeight ) );

		// The gear hit box is carved out first, so the whole rest of the row (label AND switch) toggles
		ImVec2 IconTextSize = Utils::CalcTextSize( g_Variables.FontAwesomeSolid, g_Variables.FontAwesomeSolid->FontSize - 4, ICON_FA_GEAR ); //ICON_FA_ELLIPSIS
		ImVec2 IconPos( Rect.Max.x - IconTextSize.x, Rect.Min.y + ( RowHeight - IconTextSize.y ) / 2.f );
		ImVec2 MinPos = IconPos - ImVec2( 4.f, 3.f );
		ImVec2 MaxPos = IconPos + ImVec2( IconTextSize.x + 4.f, IconTextSize.y + 3.f );

		ImVec2 MousePos = ImGui::GetMousePos( );
		const bool IconHovered = ( MousePos.x > MinPos.x && MousePos.x < MaxPos.x && MousePos.y > MinPos.y && MousePos.y < MaxPos.y );

		ImGui::ItemSize( Rect, 0.f );

		if ( !ImGui::ItemAdd( Rect, id ) ) {
			IMGUI_TEST_ENGINE_ITEM_INFO( id, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | ( *v ? ImGuiItemStatusFlags_Checked : 0 ) );
			return false;
		}

		bool Hovered, Held;
		bool Pressed = ImGui::ButtonBehavior( Rect, id, &Hovered, &Held );

		if ( Pressed && IconHovered )
			Pressed = false; // that area belongs to the gear button

		if ( Pressed ) {
			*Checked = !( *Checked );
			ImGui::MarkItemEdited( id );
		}

		struct WidCheckBox_t {
			ImVec4 TrackColor = g_Col.TrackCol;
			ImVec4 KnobColor = ImColor( 122, 122, 132 );
			ImVec4 LabelColor = g_Col.SecundaryFeaturesText;

			float Knob = 0.f;
			float Glow = 0.f;
			float PopupAlpha = 0.f;

			ImVec4 IconColor = ImColor( 245, 158, 66, 0 );
			float GearRotation = 0.f;
			bool PopupActive = false;
		};

		static std::map<ImGuiID, WidCheckBox_t> anim;
		auto CheckBoxAnim = anim.find( id );

		if ( CheckBoxAnim == anim.end( ) )
		{
			anim.insert( { id, WidCheckBox_t( ) } );
			CheckBoxAnim = anim.find( id );
		}

		CheckBoxAnim->second.LabelColor = ImLerp( CheckBoxAnim->second.LabelColor, *Checked || Hovered ? ImVec4( g_Col.FeaturesText ) : ImVec4( g_Col.SecundaryFeaturesText ), g.IO.DeltaTime * 8.f );

		DrawSwitch( Window->DrawList, Rect, RowHeight, *Checked, Hovered,
			CheckBoxAnim->second.Knob, CheckBoxAnim->second.Glow, CheckBoxAnim->second.TrackColor, CheckBoxAnim->second.KnobColor );

		if ( bToolTip ) {
			ToolTip( Label, ToolTipMsg, ToolTipIcon, Hovered && g_MenuInfo.IsOpen );
		}

		ImVec2 TextPos = ImVec2( Rect.Min.x + g_MenuInfo.CheckBoxSize + 9.f,
			Rect.Min.y + ( RowHeight - TextSize.y ) / 2.f );
		Window->DrawList->AddText( TextPos, ImGui::GetColorU32( CheckBoxAnim->second.LabelColor ), Label );

		DescTooltip( id, Desc, Hovered && g_MenuInfo.IsOpen );

		////////////////////////////
		// CFG Popup
		////////////////////////////
		static ImVec2 PopupSize( 150, 70 );
		ImVec2 PopupMin = ImVec2( MaxPos.x - PopupSize.x, MaxPos.y + 6.f );

		if ( IconHovered && g.IO.MouseClicked[ 0 ] ) {
			CheckBoxAnim->second.PopupActive = !CheckBoxAnim->second.PopupActive;
		}
		else if ( !IconHovered && g.IO.MouseClicked[ 0 ] && CheckBoxAnim->second.PopupActive ) {
			if ( !( MousePos.x > PopupMin.x && MousePos.x < PopupMin.x + PopupSize.x && MousePos.y > PopupMin.y && MousePos.y < PopupMin.y + PopupSize.y ) ) {
				CheckBoxAnim->second.PopupActive = false;
			}
		}

		CheckBoxAnim->second.GearRotation = ImLerp( CheckBoxAnim->second.GearRotation, CheckBoxAnim->second.PopupActive ? 1.f : -1.f, g.IO.DeltaTime * 10.f );
		CheckBoxAnim->second.IconColor = ImLerp( CheckBoxAnim->second.IconColor, CheckBoxAnim->second.PopupActive ? ImColor( g_Col.Base ) : ImColor( 60, 60, 60 ), g.IO.DeltaTime * 10.f );

		ImRotateStart( );
		Window->DrawList->AddText( g_Variables.FontAwesomeSolid, g_Variables.FontAwesomeSolid->FontSize - 4, IconPos, ImGui::GetColorU32( CheckBoxAnim->second.IconColor ), ICON_FA_GEAR ); //ICON_FA_ELLIPSIS
		ImRotateEnd( 1.57f * CheckBoxAnim->second.GearRotation );

		CheckBoxAnim->second.PopupAlpha = ImClamp( CheckBoxAnim->second.PopupAlpha + ( 6.f * g.IO.DeltaTime * ( CheckBoxAnim->second.PopupActive && g_MenuInfo.IsOpen ? 1.f : -1.f ) ), 0.f, 1.f );

		if ( CheckBoxAnim->second.PopupAlpha >= 0.001f ) {
			PushStyleVar( ImGuiStyleVar_Alpha, CheckBoxAnim->second.PopupAlpha );
			PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 8, 8 ) );
			PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 6, 3 ) );
			PushStyleVar( ImGuiStyleVar_CellPadding, ImVec2( 6, 6 ) );
			PushStyleVar( ImGuiStyleVar_WindowRounding, g_MenuInfo.CardRounding );
			PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 8, g_MenuInfo.RowSpacing ) );
			PushStyleVar( ImGuiStyleVar_PopupBorderSize, 1 );
			PushStyleColor( ImGuiCol_Border, GetColorU32( g_Col.ChildBorderCol ) );
			PushStyleColor( ImGuiCol_PopupBg, GetColorU32( g_Col.ChildCol ) );

			SetNextWindowSize( ImVec2( PopupSize.x, 0 ) );
			SetNextWindowPos( PopupMin );

			Begin( Label, NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_WithoutScrollClamp | ImGuiWindowFlags_Tooltip );
			{
				PopupSize = ImGui::GetWindowSize( );
				Components( );
			}

			PopStyleVar( 7 );
			PopStyleColor( 2 );

			End( );
		}
		/////////////////////


		IMGUI_TEST_ENGINE_ITEM_INFO( id, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | ( *Checked ? ImGuiItemStatusFlags_Checked : 0 ) );
		return Pressed;
	}

	inline bool CfgButton( const char * Label, std::function<void( )> Components )
	{
		ImGuiWindow * Window = ImGui::GetCurrentWindow( );
		if ( Window->SkipItems )
			return false;

		ImGuiContext & g = *GImGui;
		const ImGuiStyle & style = g.Style;

		std::string IdStr = ( std::string ) Label;
		const ImGuiID id = Window->GetID( IdStr.c_str( ) );

		ImVec2 IconTextSize = Utils::CalcTextSize( g_Variables.FontAwesomeSolid, g_Variables.FontAwesomeSolid->FontSize - 4, ICON_FA_GEAR ); //ICON_FA_ELLIPSIS
		ImVec2 MousePos = ImGui::GetMousePos( );

		const ImVec2 Pos = Window->DC.CursorPos;
		const ImRect Rect( Pos, Pos + ImVec2( IconTextSize.x, IconTextSize.y ) );

		ImGui::ItemSize( Rect, 0.f );

		if ( !ImGui::ItemAdd( Rect, id ) ) {
			return false;
		}

		bool Hovered, Held;
		bool Pressed = ImGui::ButtonBehavior( Rect, id, &Hovered, &Held );
		if ( Pressed ) {
			ImGui::MarkItemEdited( id );
		}

		struct CfgBtn_t {
			ImVec4 IconColor = ImColor( 245, 158, 66, 0 );
			float GearRotation = 0.f;
			float PopupAlpha = 0.f;
			bool PopupActive = false;
		};

		static std::map<ImGuiID, CfgBtn_t> anim;
		auto CfgAnim = anim.find( id );

		if ( CfgAnim == anim.end( ) )
		{
			anim.insert( { id, CfgBtn_t( ) } );
			CfgAnim = anim.find( id );
		}

		static ImVec2 PopupSize( 150, 70 );
		ImVec2 PopupMin = Pos + ImVec2( IconTextSize.x + 4, IconTextSize.y + 4 );

		if ( Hovered && g.IO.MouseClicked[ 0 ] ) {
			CfgAnim->second.PopupActive = !CfgAnim->second.PopupActive;
		}
		else if ( !Hovered && g.IO.MouseClicked[ 0 ] && CfgAnim->second.PopupActive ) {
			if ( !( MousePos.x > PopupMin.x && MousePos.x < PopupMin.x + PopupSize.x && MousePos.y > PopupMin.y && MousePos.y < PopupMin.y + PopupSize.y ) ) {
				CfgAnim->second.PopupActive = false;
			}
		}

		CfgAnim->second.GearRotation = ImLerp( CfgAnim->second.GearRotation, CfgAnim->second.PopupActive ? 1.f : -1.f, g.IO.DeltaTime * 10.f );
		CfgAnim->second.IconColor = ImLerp( CfgAnim->second.IconColor, CfgAnim->second.PopupActive ? ImColor( g_Col.Base ) : ImColor( 60, 60, 60 ), g.IO.DeltaTime * 10.f );

		ImRotateStart( );
		Window->DrawList->AddText( g_Variables.FontAwesomeSolid, g_Variables.FontAwesomeSolid->FontSize - 4, Pos, ImGui::GetColorU32( CfgAnim->second.IconColor ), ICON_FA_GEAR ); //ICON_FA_ELLIPSIS
		ImRotateEnd( 1.57f * CfgAnim->second.GearRotation );

		CfgAnim->second.PopupAlpha = ImClamp( CfgAnim->second.PopupAlpha + ( 6.f * g.IO.DeltaTime * ( CfgAnim->second.PopupActive && g_MenuInfo.IsOpen ? 1.f : -1.f ) ), 0.f, 1.f );

		if ( CfgAnim->second.PopupAlpha >= 0.001f ) {
			PushStyleVar( ImGuiStyleVar_Alpha, CfgAnim->second.PopupAlpha );
			PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 8, 8 ) );
			PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 6, 3 ) );
			PushStyleVar( ImGuiStyleVar_CellPadding, ImVec2( 6, 6 ) );
			PushStyleVar( ImGuiStyleVar_WindowRounding, g_MenuInfo.CardRounding );
			PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 8, g_MenuInfo.RowSpacing ) );
			PushStyleVar( ImGuiStyleVar_PopupBorderSize, 1 );
			PushStyleColor( ImGuiCol_Border, GetColorU32( g_Col.ChildBorderCol ) );
			PushStyleColor( ImGuiCol_PopupBg, GetColorU32( g_Col.ChildCol ) );

			SetNextWindowSize( ImVec2( PopupSize.x, 0 ) );
			SetNextWindowPos( PopupMin );

			Begin( Label, NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_WithoutScrollClamp | ImGuiWindowFlags_Tooltip );
			{
				PopupSize = ImGui::GetWindowSize( );
				Components( );
			}

			PopStyleVar( 7 );
			PopStyleColor( 2 );

			End( );
		}
	}

	inline bool CheckBoxPage( const char * Label, bool * Checked, const char * Desc, std::function<void( )> Code, bool bToolTip = false, const char * ToolTipMsg = xorstr( "" ) ) {
		ImGuiWindow * Window = ImGui::GetCurrentWindow( );
		if ( Window->SkipItems )
			return false;

		ImGuiContext & g = *GImGui;
		const ImGuiStyle & style = g.Style;

		std::string UniqueID = ( std::string ) Label + std::to_string( ( int ) Checked );
		const ImGuiID id = Window->GetID( UniqueID.c_str( ) );

		float Width = ImGui::GetWindowContentRegionMax( ).x - ImGui::GetWindowContentRegionMin( ).x - ( Window->ScrollbarY ? 2.f : 0.f );

		ImVec2 TextSize = ImGui::CalcTextSize( Label );
		const float RowHeight = g_MenuInfo.RowHeight;

		const ImVec2 Pos = Window->DC.CursorPos;
		const ImRect Rect( Pos, Pos + ImVec2( Width, RowHeight ) );

		// The link icon hit box is carved out first, so the whole rest of the row (label AND switch) toggles
		ImVec2 IconTextSize = Utils::CalcTextSize( g_Variables.FontAwesomeSolid, g_Variables.FontAwesomeSolid->FontSize - 4, ICON_FA_GEAR );
		ImVec2 IconPos( Rect.Max.x - IconTextSize.x, Rect.Min.y + ( RowHeight - IconTextSize.y ) / 2.f );
		ImVec2 MinPos = IconPos - ImVec2( 4.f, 3.f );
		ImVec2 MaxPos = IconPos + ImVec2( IconTextSize.x + 4.f, IconTextSize.y + 3.f );

		ImVec2 MousePos = ImGui::GetMousePos( );
		const bool IconHovered = ( MousePos.x > MinPos.x && MousePos.x < MaxPos.x && MousePos.y > MinPos.y && MousePos.y < MaxPos.y );

		ImGui::ItemSize( Rect, 0.f );

		if ( !ImGui::ItemAdd( Rect, id ) ) {
			IMGUI_TEST_ENGINE_ITEM_INFO( id, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | ( *v ? ImGuiItemStatusFlags_Checked : 0 ) );
			return false;
		}

		bool Hovered, Held;
		bool Pressed = ImGui::ButtonBehavior( Rect, id, &Hovered, &Held );

		if ( Pressed && IconHovered )
			Pressed = false; // that area belongs to the link button

		if ( Pressed ) {
			*Checked = !( *Checked );
			ImGui::MarkItemEdited( id );
		}

		struct WidCheckBox_t {
			ImVec4 TrackColor = g_Col.TrackCol;
			ImVec4 KnobColor = ImColor( 122, 122, 132 );
			ImVec4 LabelColor = g_Col.SecundaryFeaturesText;

			float Knob = 0.f;
			float Glow = 0.f;

			float SlideUp = 0.f;
			ImVec4 IconColor = ImColor( 245, 158, 66, 0 );
		};

		static std::map<ImGuiID, WidCheckBox_t> anim;
		auto CheckBoxAnim = anim.find( id );

		if ( CheckBoxAnim == anim.end( ) )
		{
			anim.insert( { id, WidCheckBox_t( ) } );
			CheckBoxAnim = anim.find( id );
		}

		CheckBoxAnim->second.LabelColor = ImLerp( CheckBoxAnim->second.LabelColor, *Checked || Hovered ? ImVec4( g_Col.FeaturesText ) : ImVec4( g_Col.SecundaryFeaturesText ), g.IO.DeltaTime * 8.f );

		DrawSwitch( Window->DrawList, Rect, RowHeight, *Checked, Hovered,
			CheckBoxAnim->second.Knob, CheckBoxAnim->second.Glow, CheckBoxAnim->second.TrackColor, CheckBoxAnim->second.KnobColor );

		ImVec2 TextPos = ImVec2( Rect.Min.x + g_MenuInfo.CheckBoxSize + 9.f,
			Rect.Min.y + ( RowHeight - TextSize.y ) / 2.f );
		Window->DrawList->AddText( TextPos, ImGui::GetColorU32( CheckBoxAnim->second.LabelColor ), Label );

		DescTooltip( id, Desc, Hovered && !IconHovered && g_MenuInfo.IsOpen );

		////////////////////////////
		// Link Obj
		////////////////////////////
		// Dois destinos na mesma linha, dois textos: a engrenagem diz que abre a
		// pagina, o resto da linha diz o que a funcao faz. O Desc e suprimido
		// enquanto o mouse esta sobre o icone, senao os dois abriam juntos.
		if ( bToolTip ) {
			ToolTip( Label, ToolTipMsg, ICON_FA_GEAR, IconHovered && g_MenuInfo.IsOpen );
		}

		CheckBoxAnim->second.SlideUp = ImLerp( CheckBoxAnim->second.SlideUp, IconHovered ? 2.f : 0.f,
			g.IO.DeltaTime * 10.f );
		CheckBoxAnim->second.IconColor = ImLerp( CheckBoxAnim->second.IconColor, IconHovered
			? ImColor( g_Col.Base ) : ImColor( 60, 60, 60 ), g.IO.DeltaTime * 10.f );

		Window->DrawList->AddText( g_Variables.FontAwesomeSolid, g_Variables.FontAwesomeSolid->FontSize - 4
			, ImVec2( IconPos.x, IconPos.y - CheckBoxAnim->second.SlideUp ),
			ImGui::GetColorU32( CheckBoxAnim->second.IconColor ), ICON_FA_GEAR );

		if ( IconHovered && g.IO.MouseClicked[ 0 ] ) {
			Code( );
		}

		IMGUI_TEST_ENGINE_ITEM_INFO( id, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | ( *Checked ? ImGuiItemStatusFlags_Checked : 0 ) );
		return Pressed;
	}

	inline void TextCentered( const char * text, int m ) {

		ImVec2 textSize = ImGui::CalcTextSize( text );
		float posX = ( g_MenuInfo.MenuSize.x  -textSize.x ) * 0.5f;
		float posY = ( g_MenuInfo.MenuSize.y - textSize.y ) * 0.5f;

		switch ( m ) {

		case 0:
			ImGui::SetCursorPos( { posX, posY } );
			ImGui::Text( text );
			break;
		case 1:
			ImGui::SetCursorPosX( posX );
			ImGui::Text( text );
			break;
		case 2:
			ImGui::SetCursorPosY( posY );
			ImGui::Text( text );
			break;
		default:
			ImGui::SetCursorPos( { posX, posY } );
			ImGui::Text( text );
			break;

		}
	}


	inline bool ButtonWithIcon( const char * icon, const char * label, const ImVec2 & size_arg, ImGuiButtonFlags flags ) {
		struct button_struct {
			ImVec4 BorderCol;
			ImVec4 background;
			ImVec4 LabelColor;
			float UpBackground;
		};

		ImGuiWindow * window = ImGui::GetCurrentWindow( );
		if ( window->SkipItems )
			return false;

		ImGuiContext & g = *GImGui;
		const ImGuiStyle & style = g.Style;
		const ImGuiID id = window->GetID( label );
		const ImVec2 label_size = g_Variables.m_FontNormal->CalcTextSizeA( g_Variables.m_FontNormal->FontSize, FLT_MAX, 0, label );
		const ImVec2 IconTextSize = g_Variables.FontAwesomeSolid->CalcTextSizeA( g_Variables.FontAwesomeSolid->FontSize - 4, FLT_MAX, 0, icon );
		const ImVec2 pos = window->DC.CursorPos;

		static std::map<ImGuiID, button_struct> anim;
		auto it_anim = anim.find( id );

		if ( it_anim == anim.end( ) ) {
			anim.insert( { id, button_struct( ) } );
			it_anim = anim.find( id );
		}

		ImVec2 size = ImGui::CalcItemSize( size_arg, label_size.x + IconTextSize.x + style.FramePadding.x * 3.0f, label_size.y + style.FramePadding.y * 2.0f );

		const ImRect bb( pos, ImVec2( pos.x + size.x, pos.y + size.y ) );
		ImGui::ItemSize( size, 0.f );

		if ( !ImGui::ItemAdd( bb, id ) )
			return false;

		bool Hovered, Held, Pressed = ImGui::ButtonBehavior( bb, id, &Hovered, &Held, flags );

		it_anim->second.BorderCol = ImLerp( it_anim->second.BorderCol, Hovered ? ImVec4( g_Col.Base ) : ImVec4( g_Col.InputBorder ), g.IO.DeltaTime * 6.f );
		it_anim->second.background = ImLerp( it_anim->second.background, Hovered ? ImVec4( g_Col.HoverCol ) : ImVec4( g_Col.InputBackground ), g.IO.DeltaTime * 6.f );
		it_anim->second.LabelColor = ImLerp( it_anim->second.LabelColor, Hovered ? ImVec4( g_Col.PrimaryText ) : ImVec4( g_Col.FeaturesText ), g.IO.DeltaTime * 6.f );
		it_anim->second.UpBackground = ImLerp( it_anim->second.UpBackground, Hovered ? ( bb.Min.y - pos.y ) - ( bb.Max.y - pos.y ) : 0.f, g.IO.DeltaTime * 6.f );

		float totalWidth = label_size.x + IconTextSize.x + style.FramePadding.x;
		ImVec2 TextPos = { pos.x + ( size.x - totalWidth ) * 0.5f + IconTextSize.x + style.FramePadding.x, pos.y + ( size.y - label_size.y ) * 0.5f - 0.5f };
		ImVec2 TextWithoutIconPos = { pos.x + ( size.x - label_size.x ) * 0.5f + style.FramePadding.x, pos.y + ( size.y - label_size.y ) * 0.5f - 0.5f };
		ImVec2 IconPos = { pos.x + ( size.x - totalWidth ) * 0.5f, pos.y + ( size.y - IconTextSize.y ) * 0.5f - 0.5f };

		window->DrawList->AddRectFilled( bb.Min, bb.Max, ImGui::GetColorU32( it_anim->second.background ), 4 );
		window->DrawList->AddRect( bb.Min, bb.Max, ImGui::GetColorU32( it_anim->second.BorderCol ), 4 );

		window->DrawList->AddRectFilled( ImVec2( bb.Min.x, bb.Max.y ), ImVec2( bb.Max.x, bb.Max.y + it_anim->second.UpBackground ), ImGui::GetColorU32( g_Col.Base ), 4 );

		//window->DrawList->AddText( g_Variables.FontAwesomeSolid, g_Variables.FontAwesomeSolid->FontSize - 4, IconPos, ImGui::GetColorU32( it_anim->second.LabelColor ), icon );
		window->DrawList->AddText( g_Variables.m_FontNormal, g_Variables.m_FontNormal->FontSize, TextWithoutIconPos, ImGui::GetColorU32( it_anim->second.LabelColor ), label );

		return Pressed;
	}

	inline bool Button( const char * label, const ImVec2 & size_arg, ImGuiButtonFlags flags, bool bToolTip = false, const char * ToolTipMsg = xorstr( "" ), const char * ToolTipIcon = xorstr( "" ) )
	{
		struct button13Anims {
			float closing_anim;
			float closing_alpha;
			float label_alpha;
			bool animation_complete;
		};

		ImGuiWindow * window = GetCurrentWindow( );
		if ( window->SkipItems )
			return false;

		ImGuiContext & g = *GImGui;
		const ImGuiStyle & style = g.Style;
		const ImGuiID id = window->GetID( label );
		const ImVec2 label_size = CalcTextSize( label, NULL, true );
		ImDrawList * draw = GetWindowDrawList( );

		static std::map<ImGuiID, button13Anims> anim; // #include <map> on top of this file, where is "imgui.h" located.
		auto it_anim = anim.find( id );
		if ( it_anim == anim.end( ) )
		{
			anim.insert( { id, button13Anims( ) } );
			it_anim = anim.find( id );
		}

		ImVec2 pos = window->DC.CursorPos;
		if ( ( flags & ImGuiButtonFlags_AlignTextBaseLine ) && style.FramePadding.y < window->DC.CurrLineTextBaseOffset ) // Try to vertically align buttons that are smaller/have no padding so that text baseline matches (bit hacky, since it shouldn't be a flag)
			pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;

		// Largura: -1 = fill real (usa a largura disponivel inteira do
		// card). Antes aplicava um clamp em 168px e empurrava pra borda
		// direita, o que em cards largos como "Configuration Actions"
		// deixava os botoes com um canto vazio enorme a esquerda — nao
		// e isso que o resto da UI espera quando passa -1.
		ImVec2 requested = size_arg;
		if ( requested.x < 0.f )
			requested.x = ImGui::GetContentRegionAvail( ).x;
		ImVec2 size = CalcItemSize( requested, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f );

		const ImRect bb( pos, pos + size );
		ItemSize( size, 0.f );
		if ( !ItemAdd( bb, id ) )
			return false;

		if ( g.LastItemData.InFlags & ImGuiItemFlags_ButtonRepeat )
			flags |= ImGuiButtonFlags_Repeat;

		bool hovered, held;
		bool pressed = ButtonBehavior( bb, id, &hovered, &held, flags );

		if ( bToolTip ) {
			ToolTip( label, ToolTipMsg, ToolTipIcon, hovered && g_MenuInfo.IsOpen );
		}

		it_anim->second.closing_anim = ImLerp( it_anim->second.closing_anim, ( hovered ? size.y : 0 ), g.IO.DeltaTime * 8.f );

		if ( hovered || pressed ) {
			if ( it_anim->second.label_alpha < 255.f )
				it_anim->second.label_alpha += 5.f / GetIO( ).Framerate * 160.f;

			if ( it_anim->second.closing_alpha < 255.f )
				it_anim->second.closing_alpha += 15.f / GetIO( ).Framerate * 160.f;
		}
		else {
			if ( it_anim->second.label_alpha > 0.f )
				it_anim->second.label_alpha -= 5.f / GetIO( ).Framerate * 160.f;

			if ( it_anim->second.closing_alpha > 0.f )
				it_anim->second.closing_alpha -= 10.f / GetIO( ).Framerate * 160.f;
		}

		// Render
		const ImU32 inside_solid_col = GetColorU32( g_Col.InputBackground );
		const ImU32 outside_solid_col = GetColorU32( g_Col.InputBorder );
		const ImU32 inside_hover_col = GetColorU32( ImVec4( g_Col.BaseFill.x, g_Col.BaseFill.y, g_Col.BaseFill.z, it_anim->second.closing_alpha / 255.f ) );

		draw->AddRectFilled( ImVec2( bb.Min.x, bb.Min.y ), ImVec2( bb.Max.x, bb.Max.y ), inside_solid_col, g_MenuInfo.ControlRounding );
		draw->AddRect( bb.Min, bb.Max, outside_solid_col, g_MenuInfo.ControlRounding );
		draw->AddRectFilled( ImVec2( bb.Min.x, bb.Max.y - it_anim->second.closing_anim ), ImVec2( bb.Max.x, bb.Max.y ), inside_hover_col, g_MenuInfo.ControlRounding );

		PushStyleColor( ImGuiCol_Text, ColorConvertFloat4ToU32( ImColor( 138, 138, 148, 255 - ( int ) it_anim->second.label_alpha ) ) );
		RenderTextClipped( bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size, style.ButtonTextAlign, &bb );
		PopStyleColor( );

		PushStyleColor( ImGuiCol_Text, ColorConvertFloat4ToU32( ImColor( 240, 253, 244, ( int ) it_anim->second.label_alpha ) ) );
		RenderTextClipped( bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size, style.ButtonTextAlign, &bb );
		PopStyleColor( );

		IMGUI_TEST_ENGINE_ITEM_INFO( id, label, g.LastItemData.StatusFlags );

		return pressed;
	}

	inline bool ButtonHeld( const char * label, const ImVec2 & size_arg, ImGuiButtonFlags flags )
	{
		struct button13Anims {
			float closing_anim;
			float closing_alpha;
			float label_alpha;
			bool animation_complete;
		};

		ImGuiWindow * window = GetCurrentWindow( );
		if ( window->SkipItems )
			return false;

		ImGuiContext & g = *GImGui;
		const ImGuiStyle & style = g.Style;
		const ImGuiID id = window->GetID( label );
		const ImVec2 label_size = CalcTextSize( label, NULL, true );
		ImDrawList * draw = GetWindowDrawList( );

		static std::map<ImGuiID, button13Anims> anim; // #include <map> on top of this file, where is "imgui.h" located.
		auto it_anim = anim.find( id );
		if ( it_anim == anim.end( ) )
		{
			anim.insert( { id, button13Anims( ) } );
			it_anim = anim.find( id );
		}

		ImVec2 pos = window->DC.CursorPos;
		if ( ( flags & ImGuiButtonFlags_AlignTextBaseLine ) && style.FramePadding.y < window->DC.CurrLineTextBaseOffset ) // Try to vertically align buttons that are smaller/have no padding so that text baseline matches (bit hacky, since it shouldn't be a flag)
			pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;
		ImVec2 size = CalcItemSize( size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f );

		const ImRect bb( pos, pos + size );
		ItemSize( size, 0.f );
		if ( !ItemAdd( bb, id ) )
			return false;

		if ( g.LastItemData.InFlags & ImGuiItemFlags_ButtonRepeat )
			flags |= ImGuiButtonFlags_Repeat;

		bool hovered, held;
		bool pressed = ButtonBehavior( bb, id, &hovered, &held, flags );

		it_anim->second.closing_anim = ImLerp( it_anim->second.closing_anim, ( held ? size.y : 0 ), g.IO.DeltaTime * 8.f );

		if ( held || pressed ) {
			if ( it_anim->second.label_alpha < 255.f )
				it_anim->second.label_alpha += 5.f / GetIO( ).Framerate * 160.f;

			if ( it_anim->second.closing_alpha < 255.f )
				it_anim->second.closing_alpha += 15.f / GetIO( ).Framerate * 160.f;
		}
		else {
			if ( it_anim->second.label_alpha > 0.f )
				it_anim->second.label_alpha -= 5.f / GetIO( ).Framerate * 160.f;

			if ( it_anim->second.closing_alpha > 0.f )
				it_anim->second.closing_alpha -= 10.f / GetIO( ).Framerate * 160.f;
		}

		// Render
		const ImU32 inside_solid_col = GetColorU32( g_Col.InputBackground );
		const ImU32 outside_solid_col = GetColorU32( g_Col.InputBorder );
		const ImU32 inside_hover_col = GetColorU32( ImVec4( g_Col.BaseFill.x, g_Col.BaseFill.y, g_Col.BaseFill.z, it_anim->second.closing_alpha / 255.f ) );

		draw->AddRectFilled( ImVec2( bb.Min.x, bb.Min.y ), ImVec2( bb.Max.x, bb.Max.y ), inside_solid_col, g_MenuInfo.ControlRounding );
		draw->AddRect( bb.Min, bb.Max, outside_solid_col, g_MenuInfo.ControlRounding );
		draw->AddRectFilled( ImVec2( bb.Min.x, bb.Max.y - it_anim->second.closing_anim ), ImVec2( bb.Max.x, bb.Max.y ), inside_hover_col, g_MenuInfo.ControlRounding );

		PushStyleColor( ImGuiCol_Text, ColorConvertFloat4ToU32( ImColor( 138, 138, 148, 255 - ( int ) it_anim->second.label_alpha ) ) );
		RenderTextClipped( bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size, style.ButtonTextAlign, &bb );
		PopStyleColor( );

		PushStyleColor( ImGuiCol_Text, ColorConvertFloat4ToU32( ImColor( 240, 253, 244, ( int ) it_anim->second.label_alpha ) ) );
		RenderTextClipped( bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size, style.ButtonTextAlign, &bb );
		PopStyleColor( );

		IMGUI_TEST_ENGINE_ITEM_INFO( id, label, g.LastItemData.StatusFlags );

		bool completed = it_anim->second.closing_anim >= ( size.y - 1.f );
		if ( completed && !it_anim->second.animation_complete ) {
			it_anim->second.animation_complete = true;
			return true;
		}

		if ( pressed ) {
			it_anim->second.animation_complete = false;
		}

		return false;
	}

	inline bool WeaponButtonHeld( ImTextureID Icon, const char * label, ImGuiButtonFlags flags )
	{
		ImGuiWindow * window = GetCurrentWindow( );
		if ( window->SkipItems )
			return false;

		ImGuiContext & g = *GImGui;
		const ImGuiStyle & style = g.Style;
		const ImVec2 LabelSize = Utils::CalcTextSize( g_Variables.m_FontSecundary, g_Variables.m_FontSecundary->FontSize, label );
		const ImGuiID id = window->GetID( label );
		ImDrawList * Draw = window->DrawList;

		struct WeaponButtonHeld_t {
			ImVec4 ProgressCol = ImColor( );
			ImVec4 ShadowProgressCol = ImColor( );
			ImVec4 SlideCol = ImColor( );
			float Alpha = 0.f;
			float SlideX = 0.f;
			float SlideXShadow = 0.f;
			float IconSize = 0.f;
			bool Completed = false;
		};

		static std::map<ImGuiID, WeaponButtonHeld_t> anim;
		auto WeaponButtonAnim = anim.find( id );
		if ( WeaponButtonAnim == anim.end( ) )
		{
			anim.insert( { id, WeaponButtonHeld_t( ) } );
			WeaponButtonAnim = anim.find( id );
		}

		const float Width = 72;
		const float Height = Width;
		ImVec2 Pos = window->DC.CursorPos;
		const ImRect Rect( Pos, Pos + ImVec2( Width, Height ) );
		ItemSize( ImVec2( Width, Height ), 0.f );
		if ( !ItemAdd( Rect, id ) )
			return false;

		if ( g.LastItemData.InFlags & ImGuiItemFlags_ButtonRepeat )
			flags |= ImGuiButtonFlags_Repeat;

		bool Hovered, Held;
		bool Pressed = ButtonBehavior( Rect, id, &Hovered, &Held, flags );

		bool Condition = WeaponButtonAnim->second.Completed ? false : Held;

		WeaponButtonAnim->second.Alpha = ImLerp( WeaponButtonAnim->second.Alpha, Condition ? 0.8f : Hovered ? 1.f : 0.6f, g.IO.DeltaTime * 8.f );
		WeaponButtonAnim->second.IconSize = ImLerp( WeaponButtonAnim->second.IconSize, Condition ? 4.f : 0.f, g.IO.DeltaTime * 8.f );
		WeaponButtonAnim->second.SlideX = ImLerp( WeaponButtonAnim->second.SlideX, Condition ? Width : 0.f, g.IO.DeltaTime * 2.2f );
		WeaponButtonAnim->second.SlideXShadow = ImLerp( WeaponButtonAnim->second.SlideXShadow, WeaponButtonAnim->second.SlideX >= 1.f ? WeaponButtonAnim->second.SlideX + 1.f : WeaponButtonAnim->second.SlideX, g.IO.DeltaTime * 3.f );
		WeaponButtonAnim->second.ProgressCol = ImLerp( WeaponButtonAnim->second.ProgressCol, WeaponButtonAnim->second.SlideX >= 0.6f ? ImVec4( g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, 255.f / 255.f ) : ImVec4( g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, 0.f ), g.IO.DeltaTime * 18.f );
		WeaponButtonAnim->second.ShadowProgressCol = ImLerp( WeaponButtonAnim->second.ShadowProgressCol, WeaponButtonAnim->second.SlideXShadow >= 0.6f ? ImVec4( g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, 0.4f ) : ImVec4( g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, 0.f ), g.IO.DeltaTime * 18.f );

		float Size = ( 7 + WeaponButtonAnim->second.IconSize );
		ImGui::PushStyleVar( ImGuiStyleVar_Alpha, g.Style.Alpha * WeaponButtonAnim->second.Alpha );
		{
			Draw->AddRectFilled( Rect.Min, Rect.Max, GetColorU32( g_Col.InputBackground ), 5 );
			//Draw->AddRectFilledMultiColor( Rect.Min, Rect.Max, GetColorU32( ImVec4( ImColor( g_Col.Base ) ), WeaponButtonAnim->second.Alpha ), GetColorU32( ImVec4( ImColor( g_Col.Base ) ), WeaponButtonAnim->second.Alpha ), GetColorU32( ImVec4( ImColor( 20, 20, 22, 0 ) ) ), GetColorU32( ImVec4( ImColor( 20, 20, 22, 0 ) ) ), 8 );
			Draw->AddRectFilled( Pos + ImVec2( 0, Height - 3 ), Pos + ImVec2( WeaponButtonAnim->second.SlideXShadow, Height ), GetColorU32( WeaponButtonAnim->second.ShadowProgressCol ), 5 );
			Draw->AddRectFilled( Pos + ImVec2( 0, Height - 3 ), Pos + ImVec2( WeaponButtonAnim->second.SlideX, Height ), GetColorU32( WeaponButtonAnim->second.ProgressCol ), 5 );
			Draw->AddImage( Icon, ImVec2( Rect.Min.x + Size, Rect.Min.y + Size ), ImVec2( Rect.Max.x - Size, Rect.Max.y - Size ), { 0,0 }, { 1,1 }, GetColorU32( ImVec4( ImColor( 255, 255, 255 ) ) ) );
		}
		ImGui::PopStyleVar( );

		Draw->AddRect( Rect.Min - ImVec2( 3, 3 ), Rect.Max + ImVec2( 3, 3 ), GetColorU32( g_Col.BackgroundCol ), 5, 0, 6.f );

		IMGUI_TEST_ENGINE_ITEM_INFO( id, label, g.LastItemData.StatusFlags );

		bool Completed = WeaponButtonAnim->second.SlideX >= ( Width - 1.f );

		if ( WeaponButtonAnim->second.Completed && Held ) {
			return false;
		}


		if ( Completed ) {
			WeaponButtonAnim->second.Completed = true;
			return true;
		}

		if ( !Held ) {
			WeaponButtonAnim->second.Completed = false;
		}

		return false;
	}

	inline void ResourceListItem( const char * Label, uint32_t ResourceState,
		const char * StateLabel, ImVec2 SizeArg )
	{
		ImGuiWindow * Window = ImGui::GetCurrentWindow( );
		if ( Window->SkipItems )
			return;

		SizeArg.x = ImGui::GetContentRegionAvail( ).x;
		const ImVec2 Pos = ImGui::GetCursorScreenPos( );
		ImGui::Dummy( SizeArg );
		const ImVec2 End = Pos + SizeArg;
		const ImVec4 StateTint = ResourceState == 3 ? ImVec4( 0.27f, 0.88f, 0.48f, 1.f )
			: ResourceState == 1 ? ImVec4( 0.88f, 0.36f, 0.36f, 1.f )
			: ResourceState == 2 || ResourceState == 4 ? ImVec4( 0.95f, 0.72f, 0.26f, 1.f )
			: g_Col.SecundaryText;
		const ImU32 StateColor = ImGui::GetColorU32( StateTint );
		const float FontSize = g_Variables.m_FontSmaller->FontSize;
		const ImVec2 StateSize = g_Variables.m_FontSmaller->CalcTextSizeA(
			FontSize, FLT_MAX, 0, StateLabel );

		Window->DrawList->AddRectFilled( Pos, End,
			ImGui::GetColorU32( g_Col.InputBackground ), g_MenuInfo.ControlRounding );
		Window->DrawList->PushClipRect( Pos + ImVec2( 8.f, 0.f ),
			ImVec2( End.x - StateSize.x - 24.f, End.y ), true );
		Window->DrawList->AddText( g_Variables.m_FontSmaller, FontSize,
			ImVec2( Pos.x + 8.f, Pos.y + ( SizeArg.y - FontSize ) * 0.5f ),
			ImGui::GetColorU32( g_Col.FeaturesText ), Label );
		Window->DrawList->PopClipRect( );
		Window->DrawList->AddText( g_Variables.m_FontSmaller, FontSize,
			ImVec2( End.x - StateSize.x - 18.f, Pos.y + ( SizeArg.y - StateSize.y ) * 0.5f ),
			StateColor, StateLabel );
		Window->DrawList->AddCircleFilled( ImVec2( End.x - 8.f, Pos.y + SizeArg.y * 0.5f ),
			3.f, StateColor, 16 );
	}

	inline bool ListSelectableCustom( const char * Label, bool * Selected, int Type )
	{
		if ( Type == 0 ) //Players
		{
			//if ( SelectableCustomResources( Label, *Selected, 3 ) ) {
				//*Selected = !*Selected;
				//return true;
			//}
		} 
		else if ( Type == 1 ) //Vehicles
		{
			//if ( SelectableCustomResources( Label, *Selected, 3 ) ) {
				//*Selected = !*Selected;
				//return true;
			//}
		}

		return false;
	}

	struct PlayerSearchBar_t {
		char SearchBuffer[256] = {};
		float IconAlpha = 0.6f;
		float BorderAlpha = 0.35f;
		float PlaceholderAlpha = 1.f;
		ImVector<int> FilteredIndices;
		int SelectedIndex = -1;
	};

	inline static PlayerSearchBar_t g_PlayerSearch = {};

	inline void PlayerSearchBar(const char* Label, const ImVector<const char*>& PlayerNames, int& SelectedPlayerIndex, ImGuiID SearchID = 0,
		PlayerSearchBar_t* SearchState = nullptr, const char* Placeholder = nullptr)
	{
		ImGuiWindow* Window = ImGui::GetCurrentWindow();
		if (Window->SkipItems)
			return;

		auto& Search = SearchState ? *SearchState : g_PlayerSearch;
		if (!Placeholder) Placeholder = Utils::Translate(xorstr("Search player..."));

		ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;

		if (SearchID == 0) SearchID = Window->GetID(Label);

		const float Width = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x - (Window->ScrollbarY ? 2.f : 0.f);
		const float Height = g_MenuInfo.RowHeight;
		const float IconPadding = 8.f;
		const float TextPadding = 6.f;

		const ImVec2 Pos = Window->DC.CursorPos;
		const ImRect Rect(Pos, Pos + ImVec2(Width, Height));

		ImGui::ItemSize(Rect, 0.f);
		if (!ImGui::ItemAdd(Rect, SearchID))
			return;

		bool Hovered = ImGui::IsMouseHoveringRect(Rect.Min, Rect.Max);
		bool Focused = ImGui::IsItemActive();

		if (Hovered && ImGui::IsMouseClicked(0))
		{
			ImGui::SetActiveID(SearchID, Window);
		}

		if (ImGui::GetActiveID() == SearchID)
		{
			Focused = true;

			ImGuiIO& io = ImGui::GetIO();
			for (int n = 0; n < io.InputQueueCharacters.Size; n++)
			{
				unsigned int c = (unsigned int)io.InputQueueCharacters[n];
				if (c < 32 || c >= 127)
					continue;

				size_t CurrentLen = strlen(Search.SearchBuffer);
				if (CurrentLen < sizeof(Search.SearchBuffer) - 1)
				{
					Search.SearchBuffer[CurrentLen] = (char)c;
					Search.SearchBuffer[CurrentLen + 1] = '\0';
				}
			}

			if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
			{
				size_t CurrentLen = strlen(Search.SearchBuffer);
				if (CurrentLen > 0)
				{
					Search.SearchBuffer[CurrentLen - 1] = '\0';
				}
			}

			if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
			{
				memset(Search.SearchBuffer, 0, sizeof(Search.SearchBuffer));
			}

			if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
			{
				ImGui::SetActiveID(0, nullptr);
			}

			io.WantCaptureKeyboard = true;
		}

		// So podemos soltar o ActiveID se ele for nosso. Sem esta checagem, um clique
		// em qualquer outro lugar apagava o ActiveID que o widget clicado (aba, botao)
		// tinha acabado de adquirir no mesmo frame, e o clique era perdido.
		if (!Hovered && ImGui::IsMouseClicked(0) && ImGui::GetActiveID() == SearchID)
		{
			ImGui::ClearActiveID();
		}

		Search.IconAlpha = ImLerp(Search.IconAlpha, Hovered || Focused ? 1.f : 0.6f, g.IO.DeltaTime * 8.f);
		Search.BorderAlpha = ImLerp(Search.BorderAlpha, Hovered || Focused ? 1.f : 0.35f, g.IO.DeltaTime * 8.f);
		Search.PlaceholderAlpha = ImLerp(Search.PlaceholderAlpha, strlen(Search.SearchBuffer) > 0 ? 0.f : 0.5f, g.IO.DeltaTime * 6.f);

		Window->DrawList->AddRectFilled(Rect.Min, Rect.Max, ImGui::GetColorU32(g_Col.InputBackground), g_MenuInfo.ControlRounding);
		Window->DrawList->AddRect(Rect.Min, Rect.Max, ImGui::GetColorU32(ImVec4(g_Col.InputBorder.x, g_Col.InputBorder.y, g_Col.InputBorder.z, Search.BorderAlpha)), g_MenuInfo.ControlRounding);

		ImVec2 IconPos(Rect.Min.x + IconPadding, Rect.Min.y + (Height - 14.f) / 2.f);
		Window->DrawList->AddText(g_Variables.FontAwesomeSolidSmall, g_Variables.FontAwesomeSolidSmall->FontSize,
			IconPos, ImGui::GetColorU32(ImVec4(g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, Search.IconAlpha)), ICON_FA_MAGNIFYING_GLASS);

		ImVec2 TextPos(Rect.Min.x + IconPadding + 18.f + TextPadding, Rect.Min.y + (Height - 10.f) / 2.f);

		if (strlen(Search.SearchBuffer) == 0)
		{
			ImVec4 PlaceholderCol = g_Col.SecundaryText;
			PlaceholderCol.w = Search.PlaceholderAlpha;
			Window->DrawList->AddText(g_Variables.m_FontSmaller, g_Variables.m_FontSmaller->FontSize,
				TextPos, ImGui::GetColorU32(PlaceholderCol), Placeholder);
		}
		else
		{
			Window->DrawList->AddText(g_Variables.m_FontSmaller, g_Variables.m_FontSmaller->FontSize,
				TextPos, ImGui::GetColorU32(g_Col.FeaturesText), Search.SearchBuffer);
		}

		Search.FilteredIndices.clear();
		for (int i = 0; i < PlayerNames.size(); i++)
		{
			const char* PlayerName = PlayerNames[i];
			bool Match = false;

			if (strlen(Search.SearchBuffer) == 0)
			{
				Match = true;
			}
			else
			{
				const char* Ptr = PlayerName;
				const char* SearchPtr = Search.SearchBuffer;

				while (*Ptr && *SearchPtr)
				{
					if (tolower((unsigned char)*Ptr) == tolower((unsigned char)*SearchPtr))
					{
						SearchPtr++;
					}
					Ptr++;
				}

				if (*SearchPtr == '\0')
				{
					Match = true;
				}
			}

			if (Match)
			{
				Search.FilteredIndices.push_back(i);
			}
		}

		SelectedPlayerIndex = Search.SelectedIndex;
	}

	inline void PlayerList(const ImVector<const char*>& PlayerNames, int& SelectedPlayerIndex, float ListHeight = 150.f)
	{
		ImGuiWindow* Window = ImGui::GetCurrentWindow();
		if (Window->SkipItems)
			return;

		ImGuiContext& g = *GImGui;

		const float Width = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x - (Window->ScrollbarY ? 2.f : 0.f);
		const float ItemHeight = g_MenuInfo.RowHeight;

		const ImVec2 ListMin = Window->DC.CursorPos;
		const ImVec2 ListMax = ListMin + ImVec2(Width, ListHeight);

		Window->DrawList->AddRectFilled(ListMin, ListMax, ImGui::GetColorU32(g_Col.ChildCol), g_MenuInfo.ControlRounding);
		Window->DrawList->AddRect(ListMin, ListMax, ImGui::GetColorU32(g_Col.InputBorder), g_MenuInfo.ControlRounding);

		ImGui::BeginChild(xorstr("##PlayerListChild"), ImVec2(Width, ListHeight), false);

		for (int i = 0; i < g_PlayerSearch.FilteredIndices.size(); i++)
		{
			int PlayerIdx = g_PlayerSearch.FilteredIndices[i];
			const char* PlayerName = PlayerNames[PlayerIdx];

			ImRect ItemRect(ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + ImVec2(Width, ItemHeight));

			bool ItemHovered = ImGui::IsMouseHoveringRect(ItemRect.Min, ItemRect.Max);
			bool ItemSelected = (PlayerIdx == g_PlayerSearch.SelectedIndex);

			if (ItemHovered && ImGui::IsMouseClicked(0))
			{
				g_PlayerSearch.SelectedIndex = PlayerIdx;
				SelectedPlayerIndex = PlayerIdx;
			}

			ImVec4 BgColor = ItemSelected ? g_Col.ActiveItemCol : (ItemHovered ? ImVec4(g_Col.HoverCol) : ImVec4(0.f, 0.f, 0.f, 0.f));
			Window->DrawList->AddRectFilled(ItemRect.Min, ItemRect.Max, ImGui::GetColorU32(BgColor), 2.f);

			ImVec4 TextColor = ItemSelected ? g_Col.BaseLight : g_Col.FeaturesText;
			Window->DrawList->AddText(g_Variables.m_FontSmaller, g_Variables.m_FontSmaller->FontSize,
				ImVec2(ItemRect.Min.x + 8.f, ItemRect.Min.y + (ItemHeight - 10.f) / 2.f),
				ImGui::GetColorU32(TextColor), PlayerName);

			ImGui::Dummy(ImVec2(0.f, ItemHeight));
		}

		ImGui::EndChild();
	}

	//////////////////////////////////////////////////////////////////////////
	// Country flags: every flag now shares one circular badge (same diameter,
	// border, drop shadow, padding and clip), matching the reference look of
	// the United Kingdom badge. Only the pattern drawn inside the badge
	// changes per language; the shape/scale/finish is always identical.
	//////////////////////////////////////////////////////////////////////////

	// Keep the small flag fully opaque so its colors remain legible on the dark sidebar.
	inline ImU32 MuteFlagColor( int R, int G, int B, int A = 255 )
	{
		return IM_COL32( ImClamp( R, 0, 255 ), ImClamp( G, 0, 255 ),
			ImClamp( B, 0, 255 ), ImClamp( A, 0, 255 ) );
	}

	// Soft shadow + neutral circular backdrop shared by every flag. The
	// backdrop color also doubles as the corner-mask color below, so the
	// square pattern drawn on top always ends up looking like a clean circle.
	inline void DrawFlagBadgeBase( ImDrawList * DrawList, const ImVec2 & Center, float Radius )
	{
		DrawList->AddShadowCircle( Center, Radius, IM_COL32( 0, 0, 0, 90 ), 8.f, ImVec2( 0.f, 1.f ), 0, 24 );
		DrawList->AddCircleFilled( Center, Radius, IM_COL32( 22, 22, 26, 255 ), 32 );
	}

	// Fakes a circular crop for the flag pattern (drawn as a filled square)
	// by painting the four corners of that square back to the badge backdrop
	// color. Each corner mask is a fan built from the corner vertex plus arc
	// samples between the two tangent points, which is star-shaped from that
	// vertex and therefore rasterizes correctly with a convex/fan fill, no
	// texture or stencil buffer required. Anti-aliasing comes from the same
	// arc segment count used everywhere else in this file for circles.
	inline void DrawFlagBadgeCornerMasks( ImDrawList * DrawList, const ImVec2 & Center, float Radius )
	{
		constexpr int SegmentsPerCorner = 10;
		constexpr float HalfPi = IM_PI * 0.5f;

		for ( int Corner = 0; Corner < 4; Corner++ )
		{
			const float StartAngle = Corner * HalfPi;
			const float EndAngle = StartAngle + HalfPi;

			const float SignX = ( Corner == 0 || Corner == 3 ) ? 1.f : -1.f;
			const float SignY = ( Corner == 0 || Corner == 1 ) ? 1.f : -1.f;

			ImVec2 Points[ SegmentsPerCorner + 2 ];
			Points[ 0 ] = ImVec2( Center.x + SignX * Radius, Center.y + SignY * Radius );

			for ( int i = 0; i <= SegmentsPerCorner; i++ )
			{
				const float t = StartAngle + ( EndAngle - StartAngle ) * ( i / ( float ) SegmentsPerCorner );
				Points[ i + 1 ] = ImVec2( Center.x + cosf( t ) * Radius, Center.y + sinf( t ) * Radius );
			}

			DrawList->AddConvexPolyFilled( Points, SegmentsPerCorner + 2, IM_COL32( 22, 22, 26, 255 ) );
		}
	}

	// Same subtle ring for every flag, drawn last so it always sits crisp on
	// top of the pattern and corner masks.
	inline void DrawFlagBadgeBorder( ImDrawList * DrawList, const ImVec2 & Center, float Radius )
	{
		DrawList->AddCircle( Center, Radius, IM_COL32( 255, 255, 255, 110 ), 32, 1.f );
	}

	inline void DrawBrazilFlagPattern( ImDrawList * DrawList, const ImVec2 & Min, const ImVec2 & Max )
	{
		DrawList->AddRectFilled( Min, Max, MuteFlagColor( 0, 178, 66 ) );

		const ImVec2 Center( ( Min.x + Max.x ) * 0.5f, ( Min.y + Max.y ) * 0.5f );
		const float W = Max.x - Min.x;
		const float H = Max.y - Min.y;

		ImVec2 Rhombus[ 4 ] = {
			ImVec2( Center.x, Min.y + H * 0.20f ),
			ImVec2( Max.x - W * 0.16f, Center.y ),
			ImVec2( Center.x, Max.y - H * 0.20f ),
			ImVec2( Min.x + W * 0.16f, Center.y ),
		};
		DrawList->AddConvexPolyFilled( Rhombus, 4, MuteFlagColor( 254, 223, 0 ) );

		DrawList->AddCircleFilled( Center, H * 0.15f, MuteFlagColor( 12, 61, 158 ), 24 );

		const float StarRadius = H * 0.055f;
		ImVec2 StarPoints[ 5 ];
		for ( int i = 0; i < 5; i++ )
		{
			const float Angle = IM_PI * 2.f * i / 5.f - IM_PI * 0.5f;
			StarPoints[ i ] = ImVec2( Center.x + cosf( Angle ) * StarRadius, Center.y + sinf( Angle ) * StarRadius );
		}
		DrawList->AddConvexPolyFilled( StarPoints, 5, MuteFlagColor( 255, 255, 255 ) );
	}

	inline void DrawUKFlagPattern( ImDrawList * DrawList, const ImVec2 & Min, const ImVec2 & Max )
	{
		DrawList->AddRectFilled( Min, Max, MuteFlagColor( 0, 36, 125 ) );

		const float W = Max.x - Min.x;
		const float H = Max.y - Min.y;
		const ImU32 White = MuteFlagColor( 255, 255, 255 );
		const ImU32 Red = MuteFlagColor( 200, 16, 46 );

		const float DiagThickness = W * 0.16f;
		DrawList->AddLine( Min, Max, White, DiagThickness );
		DrawList->AddLine( ImVec2( Max.x, Min.y ), ImVec2( Min.x, Max.y ), White, DiagThickness );
		DrawList->AddLine( Min, Max, Red, DiagThickness * 0.5f );
		DrawList->AddLine( ImVec2( Max.x, Min.y ), ImVec2( Min.x, Max.y ), Red, DiagThickness * 0.5f );

		const float CrossW = W * 0.16f;
		const float CrossH = H * 0.16f;
		DrawList->AddRectFilled( ImVec2( Min.x + ( W - CrossW ) * 0.5f, Min.y ), ImVec2( Min.x + ( W + CrossW ) * 0.5f, Max.y ), White );
		DrawList->AddRectFilled( ImVec2( Min.x, Min.y + ( H - CrossH ) * 0.5f ), ImVec2( Max.x, Min.y + ( H + CrossH ) * 0.5f ), White );

		const float RedCrossW = W * 0.09f;
		const float RedCrossH = H * 0.09f;
		DrawList->AddRectFilled( ImVec2( Min.x + ( W - RedCrossW ) * 0.5f, Min.y ), ImVec2( Min.x + ( W + RedCrossW ) * 0.5f, Max.y ), Red );
		DrawList->AddRectFilled( ImVec2( Min.x, Min.y + ( H - RedCrossH ) * 0.5f ), ImVec2( Max.x, Min.y + ( H + RedCrossH ) * 0.5f ), Red );
	}

	inline void DrawUSFlagPattern( ImDrawList * DrawList, const ImVec2 & Min, const ImVec2 & Max )
	{
		const float W = Max.x - Min.x;
		const float H = Max.y - Min.y;

		const ImU32 Red = MuteFlagColor( 220, 28, 62 );
		const ImU32 White = MuteFlagColor( 255, 255, 255 );
		const ImU32 Blue = MuteFlagColor( 14, 58, 151 );

		constexpr int StripeCount = 7;
		const float StripeH = H / StripeCount;
		for ( int i = 0; i < StripeCount; i++ )
		{
			const ImVec2 SMin( Min.x, Min.y + i * StripeH );
			const ImVec2 SMax( Max.x, SMin.y + StripeH );
			DrawList->AddRectFilled( SMin, SMax, ( i % 2 == 0 ) ? Red : White );
		}

		const float CantonW = W * 0.46f;
		const float CantonH = StripeH * 4.f;
		DrawList->AddRectFilled( Min, ImVec2( Min.x + CantonW, Min.y + CantonH ), Blue );
	}

	inline void DrawRussiaFlagPattern( ImDrawList * DrawList, const ImVec2 & Min, const ImVec2 & Max )
	{
		const float H = Max.y - Min.y;
		const float StripeH = H / 3.f;

		DrawList->AddRectFilled( Min, ImVec2( Max.x, Min.y + StripeH ), MuteFlagColor( 255, 255, 255 ) );
		DrawList->AddRectFilled( ImVec2( Min.x, Min.y + StripeH ), ImVec2( Max.x, Min.y + StripeH * 2.f ), MuteFlagColor( 0, 57, 166 ) );
		DrawList->AddRectFilled( ImVec2( Min.x, Min.y + StripeH * 2.f ), Max, MuteFlagColor( 213, 43, 30 ) );
	}

	inline void DrawSpainFlagPattern( ImDrawList * DrawList, const ImVec2 & Min, const ImVec2 & Max )
	{
		const float W = Max.x - Min.x;
		const float H = Max.y - Min.y;

		const ImU32 Red = MuteFlagColor( 255, 64, 64 );
		const ImU32 Yellow = MuteFlagColor( 255, 196, 0 );

		DrawList->AddRectFilled( Min, ImVec2( Max.x, Min.y + H * 0.25f ), Red );
		DrawList->AddRectFilled( ImVec2( Min.x, Min.y + H * 0.25f ), ImVec2( Max.x, Min.y + H * 0.75f ), Yellow );
		DrawList->AddRectFilled( ImVec2( Min.x, Min.y + H * 0.75f ), Max, Red );
	}

	// pos = top-left of the badge's bounding box, size = badge diameter.
	// Lang defaults to the globally active language (used for the profile-bar badge), but the
	// selector popup below passes each row's own option so every entry shows ITS flag instead of
	// all of them rendering whichever language happens to be selected right now.
	// Every language renders through the exact same badge chrome above, so
	// adding a new flag only means adding one more *Pattern function and a
	// switch case here, never touching shape/border/shadow/clip code.
	inline void DrawFlagIcon( ImDrawList* DrawList, const ImVec2& pos, float size, Utils::Language Lang )
	{
		const float Radius = size * 0.5f;
		const ImVec2 Center(pos.x + Radius, pos.y + Radius);

		DrawFlagBadgeBase(DrawList, Center, Radius);

		const ImVec2 SquareMin(Center.x - Radius, Center.y - Radius);
		const ImVec2 SquareMax(Center.x + Radius, Center.y + Radius);

		DrawList->PushClipRect(SquareMin, SquareMax, true);

		switch (Lang)
		{
		case Utils::Language::PortugueseBR:
			DrawBrazilFlagPattern(DrawList, SquareMin, SquareMax);
			break;
		case Utils::Language::EnglishUK:
			DrawUKFlagPattern(DrawList, SquareMin, SquareMax);
			break;
		case Utils::Language::EnglishUS:
			DrawUSFlagPattern(DrawList, SquareMin, SquareMax);
			break;
		case Utils::Language::Russian:
			DrawRussiaFlagPattern(DrawList, SquareMin, SquareMax);
			break;
		case Utils::Language::Spanish:
			DrawSpainFlagPattern(DrawList, SquareMin, SquareMax);
			break;
		}

		DrawList->PopClipRect();

		DrawFlagBadgeCornerMasks(DrawList, Center, Radius);
		DrawFlagBadgeBorder(DrawList, Center, Radius);
	}

	// Small flag button docked to the RIGHT edge of the profile panel, far enough from the
	// avatar/username block on the left that it never overlaps it. Clicking it opens a compact
	// popup listing every supported language; picking one updates every visible string immediately
	// since Utils::Translate() re-reads g_CurrentLanguage on every call, no restart involved.
	inline void LanguageSelector( ImDrawList * DrawList, const ImVec2 & PanelMin, const ImVec2 & PanelMax )
	{
		if ( !g_MenuInfo.IsOpen )
			return;

		struct LangSelector_t {
			float HoverAlpha = 0.f;
			float PopupAlpha = 0.f;
			bool PopupActive = false;
		};

		static LangSelector_t Anim;

		const float FlagSize = 18.f; // diameter of the circular badge
		const float Padding = 10.f;

		ImVec2 FlagMin( PanelMax.x - Padding - FlagSize, PanelMin.y + ( ( PanelMax.y - PanelMin.y ) - FlagSize ) / 2.f );
		ImVec2 FlagMax( FlagMin.x + FlagSize, FlagMin.y + FlagSize );

		ImVec2 HitMin = FlagMin - ImVec2( 6.f, 6.f );
		ImVec2 HitMax = FlagMax + ImVec2( 6.f, 6.f );

		ImVec2 MousePos = ImGui::GetMousePos( );
		const bool Hovered = ( MousePos.x > HitMin.x && MousePos.x < HitMax.x && MousePos.y > HitMin.y && MousePos.y < HitMax.y );

		Anim.HoverAlpha = ImLerp( Anim.HoverAlpha, Hovered ? 1.f : 0.f, ImGui::GetIO( ).DeltaTime * 10.f );

		static const ImVec2 PopupSize( 150.f, 132.f );
		ImVec2 PopupMin( HitMax.x - PopupSize.x, HitMax.y + 6.f );

		if ( Hovered && ImGui::GetIO( ).MouseClicked[ 0 ] )
		{
			Anim.PopupActive = !Anim.PopupActive;
		}
		else if ( !Hovered && ImGui::GetIO( ).MouseClicked[ 0 ] && Anim.PopupActive )
		{
			if ( !( MousePos.x > PopupMin.x && MousePos.x < PopupMin.x + PopupSize.x && MousePos.y > PopupMin.y && MousePos.y < PopupMin.y + PopupSize.y ) )
			{
				Anim.PopupActive = false;
			}
		}

		if ( Anim.HoverAlpha > 0.01f )
		{
			DrawList->AddRectFilled( HitMin, HitMax, ImGui::GetColorU32( ImVec4( g_Col.HoverCol.x, g_Col.HoverCol.y, g_Col.HoverCol.z, Anim.HoverAlpha * g_Col.HoverCol.w ) ), 4.f );
		}

		// The badge itself already draws its own circular border/shadow, so no extra rectangular
		// outline is added here (that would look inconsistent against the round shape).
		DrawFlagIcon( DrawList, FlagMin, FlagSize );

		Anim.PopupAlpha = ImClamp( Anim.PopupAlpha + ( 6.f * ImGui::GetIO( ).DeltaTime * ( Anim.PopupActive && g_MenuInfo.IsOpen ? 1.f : -1.f ) ), 0.f, 1.f );

		if ( Anim.PopupAlpha >= 0.001f )
		{
			struct LangOption_t {
				Utils::Language Lang;
				std::string Label;
			};

			static const LangOption_t Options[ 5 ] = {
				{ Utils::Language::PortugueseBR, std::string( xorstr( "Português (Brasil)" ) ) },
				{ Utils::Language::EnglishUK,     std::string( xorstr( "English (UK)" ) ) },
				{ Utils::Language::EnglishUS,     std::string( xorstr( "English (US)" ) ) },
				{ Utils::Language::Russian,       std::string( xorstr( "Русский" ) ) },
				{ Utils::Language::Spanish,       std::string( xorstr( "Español" ) ) },
			};

			ImGui::PushStyleVar( ImGuiStyleVar_Alpha, Anim.PopupAlpha );
			ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 6, 6 ) );
			ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 6, 4 ) );
			ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, g_MenuInfo.CardRounding );
			ImGui::PushStyleVar( ImGuiStyleVar_PopupBorderSize, 1 );
			ImGui::PushStyleColor( ImGuiCol_Border, ImGui::GetColorU32( g_Col.ChildBorderCol ) );
			ImGui::PushStyleColor( ImGuiCol_PopupBg, ImGui::GetColorU32( g_Col.ChildCol ) );

			ImGui::SetNextWindowSize( ImVec2( PopupSize.x, 0 ) );
			ImGui::SetNextWindowPos( PopupMin );

			ImGui::Begin( xorstr( "##LanguageSelectorPopup" ), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_WithoutScrollClamp | ImGuiWindowFlags_Tooltip );
			{
				for ( const LangOption_t & Option : Options )
				{
					const bool Active = Utils::g_CurrentLanguage == Option.Lang;

					ImGuiWindow * Window = ImGui::GetCurrentWindow( );
					const ImVec2 RowPos = Window->DC.CursorPos;
					const float RowWidth = ImGui::GetWindowContentRegionMax( ).x - ImGui::GetWindowContentRegionMin( ).x;
					const float RowHeight = 20.f;

					ImGui::InvisibleButton( Option.Label.c_str( ), ImVec2( RowWidth, RowHeight ) );
					const bool RowHovered = ImGui::IsItemHovered( );

					if ( RowHovered || Active )
					{
						Window->DrawList->AddRectFilled( RowPos, RowPos + ImVec2( RowWidth, RowHeight ),
							ImGui::GetColorU32( Active ? ImVec4( g_Col.Base.x, g_Col.Base.y, g_Col.Base.z, 0.22f ) : g_Col.HoverCol ), 3.f );
					}

					ImVec2 FlagPos( RowPos.x + 4.f, RowPos.y + ( RowHeight - 12.f ) / 2.f );
					DrawFlagIcon( Window->DrawList, FlagPos, 12.f, Option.Lang );

					ImVec2 LabelSize = Utils::CalcTextSize( g_Variables.m_FontSmaller, g_Variables.m_FontSmaller->FontSize, Option.Label.c_str( ) );
					ImVec2 LabelPos( FlagPos.x + 12.f + 6.f, RowPos.y + ( RowHeight - LabelSize.y ) / 2.f );

					Window->DrawList->AddText( g_Variables.m_FontSmaller, g_Variables.m_FontSmaller->FontSize, LabelPos,
						ImGui::GetColorU32( Active ? ImVec4( g_Col.BaseLight ) : ImVec4( g_Col.FeaturesText ) ), Option.Label.c_str( ) );

					if ( RowHovered && ImGui::GetIO( ).MouseClicked[ 0 ] )
					{
						Utils::g_CurrentLanguage = Option.Lang;
						Anim.PopupActive = false;
					}
				}
			}
			ImGui::End( );

			ImGui::PopStyleColor( 2 );
			ImGui::PopStyleVar( 5 );
		}
	}

	// One fixed-position flag shows the current language and toggles between
	// Portuguese and English when clicked.
	inline void CompactLanguageSelector( ImDrawList * DrawList, const ImVec2 & PanelMin, const ImVec2 & PanelMax )
	{
		if ( !g_MenuInfo.IsOpen )
			return;

		static float HoverAlpha = 0.f;
		static float PressAlpha = 0.f;
		const float FlagSize = 20.f;
		const ImVec2 BaseMin( PanelMax.x - FlagSize - 10.f,
			( PanelMin.y + PanelMax.y - FlagSize ) * 0.5f );
		const float CenterY = ( PanelMin.y + PanelMax.y ) * 0.5f;
		const ImVec2 Mouse = ImGui::GetMousePos( );
		const float Dt = ImGui::GetIO( ).DeltaTime;
		const ImRect Hit( BaseMin - ImVec2( 5.f, 5.f ),
			BaseMin + ImVec2( FlagSize + 5.f, FlagSize + 5.f ) );
		const bool Hovered = Hit.Contains( Mouse );
		HoverAlpha = ImLerp( HoverAlpha, Hovered ? 1.f : 0.f, Dt * 12.f );
		PressAlpha = ImLerp( PressAlpha,
			Hovered && ImGui::GetIO( ).MouseDown[ 0 ] ? 1.f : 0.f, Dt * 18.f );
		if ( Hovered && ImGui::GetIO( ).MouseClicked[ 0 ] )
			Utils::g_CurrentLanguage = Utils::g_CurrentLanguage == Utils::Language::PortugueseBR
				? Utils::Language::EnglishUS : Utils::Language::PortugueseBR;

		const float Scale = 1.f + HoverAlpha * 0.07f - PressAlpha * 0.05f;
		const float Size = FlagSize * Scale;
		const ImVec2 FlagMin( BaseMin.x + ( FlagSize - Size ) * 0.5f,
			CenterY - Size * 0.5f );
		DrawFlagIcon( DrawList, FlagMin, Size,
			Utils::g_CurrentLanguage == Utils::Language::PortugueseBR
				? Utils::Language::PortugueseBR : Utils::Language::EnglishUS );
	}

	inline void ProfileBar() {
		ImVec2 WindowPos = ImGui::GetWindowPos();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();

		std::string Username = g_Variables.UserName;
		std::string Role = g_Variables.Role;
		std::string FirstLetter = Username.empty() ? std::string() : Username.substr(0, 1);
		std::transform(FirstLetter.begin(), FirstLetter.end(), FirstLetter.begin(), [](unsigned char c) { return std::toupper(c); });

		if (Username.length() > 12) {
			Username = Username.substr(0, 10) + Utils::Translate( xorstr("...") );
		}

		const float Radius = g_MenuInfo.AvatarSize / 2.f;

		// Anchored to the bottom edge, so it follows the window height instead of floating
		const float BlockTop = WindowPos.y + g_MenuInfo.MenuSize.y - g_MenuInfo.ProfileY - g_MenuInfo.ProfileHeight;
		const float BlockLeft = WindowPos.x + g_MenuInfo.SideBarPadding + 6.f;

		ImVec2 LetterSize = Utils::CalcTextSize(g_Variables.m_FontSecundary, g_Variables.m_FontSecundary->FontSize, FirstLetter.c_str());
		ImVec2 NameSize = Utils::CalcTextSize(g_Variables.m_FontSecundary, g_Variables.m_FontSecundary->FontSize, Username.c_str());
		ImVec2 RoleSize = Utils::CalcTextSize(g_Variables.m_FontSmaller, g_Variables.m_FontSmaller->FontSize, Role.c_str());

		// Panel that hosts the profile, same body/border/glow language as the cards
		const ImVec2 PanelMin(WindowPos.x + 8.f, BlockTop);
		const ImVec2 PanelMax(WindowPos.x + g_MenuInfo.SideBarWidth - 8.f, BlockTop + g_MenuInfo.ProfileHeight);

		DrawList->AddRectFilled(PanelMin, PanelMax, ImGui::GetColorU32(g_Col.ChildCol), g_MenuInfo.CardRounding);
		DrawList->AddRect(PanelMin, PanelMax, ImGui::GetColorU32(g_Col.ChildBorderCol), g_MenuInfo.CardRounding);
		GlowBorder(DrawList, PanelMin, PanelMax, g_MenuInfo.CardRounding, g_Col.Base);

		ImVec2 AvatarCenter = { BlockLeft + Radius, BlockTop + g_MenuInfo.ProfileHeight / 2.f };
		DrawList->AddCircleFilled(AvatarCenter, Radius, ImGui::GetColorU32((ImVec4)ImColor(38, 38, 43)), 64);
		DrawList->AddCircle(AvatarCenter, Radius, ImGui::GetColorU32((ImVec4)ImColor(54, 54, 60)), 64);

		DrawList->AddText(g_Variables.m_FontSecundary, g_Variables.m_FontSecundary->FontSize,
			ImVec2(AvatarCenter.x - LetterSize.x / 2.f, AvatarCenter.y - LetterSize.y / 2.f - 0.5f),
			ImGui::GetColorU32((ImVec4)ImColor(200, 200, 208)), FirstLetter.c_str());

		CompactLanguageSelector( DrawList, PanelMin, PanelMax );

		const float InfoX = BlockLeft + g_MenuInfo.AvatarSize + 8.f;
		const float TextTop = Role.empty()
			? BlockTop + (g_MenuInfo.ProfileHeight - NameSize.y) / 2.f
			: BlockTop + (g_MenuInfo.ProfileHeight - (NameSize.y + RoleSize.y + 7.f)) / 2.f;

		DrawList->AddText(g_Variables.m_FontSecundary, g_Variables.m_FontSecundary->FontSize,
			ImVec2(InfoX, TextTop),
			ImGui::GetColorU32(g_Col.PrimaryText), Username.c_str());

		// Role badge, hidden while the role is not populated so no empty pill is drawn
		if (!Role.empty())
		{
			ImVec2 BadgeMin(InfoX, TextTop + NameSize.y + 3.f);
			ImVec2 BadgeMax(BadgeMin.x + RoleSize.x + 10.f, BadgeMin.y + RoleSize.y + 4.f);

			DrawList->AddRectFilled(BadgeMin, BadgeMax, ImGui::GetColorU32(g_Col.BadgeCol), 4.f);
			DrawList->AddRect(BadgeMin, BadgeMax, ImGui::GetColorU32(g_Col.BadgeBorderCol), 4.f);
			DrawList->AddText(g_Variables.m_FontSmaller, g_Variables.m_FontSmaller->FontSize,
				ImVec2(BadgeMin.x + 5.f, BadgeMin.y + 2.f),
				ImGui::GetColorU32(g_Col.BadgeTextCol), Role.c_str());
		}
	}

}
