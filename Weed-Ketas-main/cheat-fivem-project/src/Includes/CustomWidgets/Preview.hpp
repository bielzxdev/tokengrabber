#pragma once
#include <Includes/Includes.hpp>

#include <cmath>
#include <cstring>

namespace Custom::VehicleModel
{
	struct Vertex { D3DXVECTOR3 P, N; D3DXVECTOR4 Color; };
	inline GLuint Vao = 0, Vbo = 0, Program = 0;
	inline GLuint ColorTexture = 0, DepthBuffer = 0, Framebuffer = 0;
	inline size_t VertexCount = 0;
	inline int UniformVP = -1;
	inline bool Attempted = false;
	inline std::string Error;

	inline void Reset( )
	{
		if ( Framebuffer ) GLX::DeleteFramebuffers( 1, &Framebuffer );
		if ( DepthBuffer ) GLX::DeleteRenderbuffers( 1, &DepthBuffer );
		if ( Vao ) GLX::DeleteVertexArrays( 1, &Vao );
		if ( Vbo ) GLX::DeleteBuffers( 1, &Vbo );
		if ( Program ) GLX::DeleteProgram( Program );
		if ( ColorTexture ) GLX::DeleteTextures( 1, &ColorTexture );
		Vao = Vbo = Program = ColorTexture = DepthBuffer = Framebuffer = 0;
		VertexCount = 0; UniformVP = -1; Attempted = false; Error.clear( );
	}

	inline GLuint CompileShader( GLenum Type, const char * Source )
	{
		GLuint Shader = GLX::CreateShader( Type );
		if ( !Shader ) return 0;
		GLX::ShaderSource( Shader, 1, &Source, nullptr );
		GLX::CompileShader( Shader );

		GLint Ok = 0;
		GLX::GetShaderiv( Shader, GL_COMPILE_STATUS, &Ok );
		if ( !Ok )
		{
			GLint Length = 0;
			GLX::GetShaderiv( Shader, GL_INFO_LOG_LENGTH, &Length );
			Error.assign( Length > 0 ? static_cast<size_t>( Length ) : 1, '\0' );
			if ( Length > 0 )
				GLX::GetShaderInfoLog( Shader, Length, nullptr, &Error[ 0 ] );
			Error.insert( 0, "Shader: " );
			GLX::DeleteShader( Shader );
			return 0;
		}
		return Shader;
	}

	// Fixed scene: built once, rendered once into an offscreen FBO.
	inline bool Prepare( )
	{
		if ( ColorTexture ) return true;
		if ( Attempted ) return false;
		Attempted = true;
		auto Fail = []( const char * Stage ) { Error = Stage; return false; };

		std::vector<Vertex> V;
		const D3DXVECTOR4 Paint( .055f, .27f, .55f, 1 ), Glass( .025f, .055f, .085f, 1 ),
			Rubber( .018f, .022f, .028f, 1 ), Silver( .57f, .62f, .68f, 1 ), Dark( .025f, .035f, .045f, 1 ),
			Light( .85f, .94f, 1, 1 ), Red( .65f, .025f, .035f, 1 );
		auto Tri = [&]( D3DXVECTOR3 A, D3DXVECTOR3 B, D3DXVECTOR3 C, D3DXVECTOR4 Col ) {
			D3DXVECTOR3 U = B - A, W = C - A, N;
			D3DXVec3Cross( &N, &U, &W ); D3DXVec3Normalize( &N, &N );
			V.push_back( { A, N, Col } ); V.push_back( { B, N, Col } ); V.push_back( { C, N, Col } );
		};
		auto Quad = [&]( D3DXVECTOR3 A, D3DXVECTOR3 B, D3DXVECTOR3 C, D3DXVECTOR3 D, D3DXVECTOR4 Col ) {
			Tri( A, B, C, Col ); Tri( A, C, D, Col );
		};
		auto Box = [&]( D3DXVECTOR3 A, D3DXVECTOR3 B, D3DXVECTOR4 Col ) {
			D3DXVECTOR3 P[ 8 ] = { { A.x,A.y,A.z },{ B.x,A.y,A.z },{ B.x,B.y,A.z },{ A.x,B.y,A.z },
				{ A.x,A.y,B.z },{ B.x,A.y,B.z },{ B.x,B.y,B.z },{ A.x,B.y,B.z } };
			const int Faces[ 6 ][ 4 ] = { { 0,3,2,1 },{ 4,5,6,7 },{ 0,4,7,3 },{ 1,2,6,5 },{ 3,7,6,2 },{ 0,1,5,4 } };
			for ( auto & F : Faces ) Quad( P[ F[ 0 ] ], P[ F[ 1 ] ], P[ F[ 2 ] ], P[ F[ 3 ] ], Col );
		};
		struct Section { float Z, Width, Top; };
		const Section S[ ] = { { -2.35f,.77f,.85f },{ -2.15f,.91f,1.00f },{ -1.5f,.96f,1.05f },
			{ -.8f,.96f,1.08f },{ .7f,.95f,1.06f },{ 1.45f,.93f,1.02f },{ 2.12f,.86f,.92f },{ 2.35f,.75f,.82f } };
		D3DXVECTOR3 Rings[ 8 ][ 8 ];
		for ( int j = 0; j < 8; ++j )
		{
			float w = S[ j ].Width, t = S[ j ].Top, z = S[ j ].Z;
			D3DXVECTOR3 R[ 8 ] = { { -w*.91f,.42f,z },{ -w,.57f,z },{ -w,t-.13f,z },{ -w*.83f,t,z },
				{ w*.83f,t,z },{ w,t-.13f,z },{ w,.57f,z },{ w*.91f,.42f,z } };
			for ( int k = 0; k < 8; ++k ) Rings[ j ][ k ] = R[ k ];
		}
		for ( int j = 0; j < 7; ++j ) for ( int k = 0; k < 8; ++k )
			Quad( Rings[ j ][ k ], Rings[ j + 1 ][ k ], Rings[ j + 1 ][ ( k + 1 ) % 8 ], Rings[ j ][ ( k + 1 ) % 8 ], Paint );
		for ( int k = 1; k < 7; ++k )
		{
			Tri( Rings[ 0 ][ 0 ], Rings[ 0 ][ k + 1 ], Rings[ 0 ][ k ], Paint );
			Tri( Rings[ 7 ][ 0 ], Rings[ 7 ][ k ], Rings[ 7 ][ k + 1 ], Paint );
		}
		D3DXVECTOR3 LB( -.79f, 1.055f, -1.28f ), RB( .79f, 1.055f, -1.28f ),
			LF( -.79f, 1.055f, 1.03f ), RF( .79f, 1.055f, 1.03f ),
			TLB( -.64f, 1.62f, -.73f ), TRB( .64f, 1.62f, -.73f ),
			TLF( -.64f, 1.62f, .38f ), TRF( .64f, 1.62f, .38f );
		Quad( LB, RB, TRB, TLB, Glass ); Quad( LF, TLF, TRF, RF, Glass );
		Quad( LB, TLB, TLF, LF, Paint ); Quad( RF, TRF, TRB, RB, Paint );
		Quad( TLB, TRB, D3DXVECTOR3( .57f, 1.66f, .28f ), D3DXVECTOR3( -.57f, 1.66f, .28f ), Paint );
		Quad( D3DXVECTOR3( -.57f, 1.66f, .28f ), D3DXVECTOR3( .57f, 1.66f, .28f ), TRF, TLF, Paint );
		for ( float side : { -1.f, 1.f } )
		{
			auto P = [&]( float y, float z ) { return D3DXVECTOR3( side * ( .79f - ( y - 1.055f ) * ( .15f / .565f ) + .008f ), y, z ); };
			Quad( P( 1.12f, -1.13f ), P( 1.55f, -.70f ), P( 1.55f, -.23f ), P( 1.12f, -.23f ), Glass );
			Quad( P( 1.12f, -.15f ), P( 1.55f, -.15f ), P( 1.55f, .34f ), P( 1.12f, .88f ), Glass );
			Box( { side*.963f,.54f,-.20f }, { side*.965f,1.0f,-.19f }, Dark );
			for ( float z : { -.90f, .25f } ) Box( { side*.956f,.92f,z }, { side*.975f,.945f,z+.16f }, Silver );
			Box( { side*.77f,1.08f,.70f }, { side*1.12f,1.22f,.92f }, Paint );
		}
		Box( { -.70f,.46f,2.33f }, { .70f,.61f,2.39f }, Dark );
		Box( { -.44f,.63f,2.35f }, { .44f,.80f,2.395f }, Dark );
		for ( float y : { .66f,.71f,.76f } ) Box( { -.42f,y,2.397f }, { .42f,y+.014f,2.405f }, Silver );
		for ( float side : { -1.f,1.f } )
		{
			Box( { side*.47f,.76f,2.27f }, { side*.76f,.85f,2.36f }, Light );
			Box( { side*.48f,.75f,-2.365f }, { side*.77f,.86f,-2.30f }, Red );
		}
		Box( { -.72f,.43f,-2.37f }, { .72f,.55f,-2.31f }, Dark );
		for ( float side : { -1.f,1.f } ) for ( float z : { -1.48f,1.45f } )
		{
			const float xs[ ] = { .84f,.88f,1.04f,1.08f };
			const float rs[ ] = { .32f,.365f,.365f,.32f };
			for ( int j = 0; j < 3; ++j ) for ( int k = 0; k < 32; ++k )
			{
				float a = k * D3DX_PI / 16, b = ( k + 1 ) * D3DX_PI / 16;
				auto P = [&]( int r, float t ) { return D3DXVECTOR3( side*xs[ r ], .365f + rs[ r ] * cosf( t ), z + rs[ r ] * sinf( t ) ); };
				Quad( P( j, a ), P( j, b ), P( j + 1, b ), P( j + 1, a ), Rubber );
			}
			for ( int k = 0; k < 32; ++k )
			{
				float a = k * D3DX_PI / 16, b = ( k + 1 ) * D3DX_PI / 16;
				auto P = [&]( float r, float t ) { return D3DXVECTOR3( side*1.083f, .365f + r * cosf( t ), z + r * sinf( t ) ); };
				Quad( P( .24f, a ), P( .32f, a ), P( .32f, b ), P( .24f, b ), Rubber );
				Tri( { side*1.086f,.365f,z }, P( .24f, a ), P( .24f, b ), k % 4 < 2 ? Silver : Dark );
			}
		}

		if ( !GLX::GenVertexArrays || !GLX::GenBuffers || !GLX::CreateProgram )
			return Fail( "OpenGL loader" );

		const char * VertexSource = R"GLSL(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;
uniform mat4 uVP;
out vec3 vWorld;
out vec3 vNormal;
out vec4 vColor;
void main()
{
    vWorld = aPos;
    vNormal = aNormal;
    vColor = aColor;
    gl_Position = uVP * vec4(aPos, 1.0);
})GLSL";
		const char * FragmentSource = R"GLSL(#version 330 core
in vec3 vWorld;
in vec3 vNormal;
in vec4 vColor;
out vec4 FragColor;
void main()
{
    vec3 n = normalize(vNormal);
    if (!gl_FrontFacing) n = -n;
    vec3 l = normalize(vec3(-3.0, 6.0, 4.0));
    vec3 eye = normalize(vec3(5.0, 3.1, 6.0) - vWorld);
    float diffuse = max(dot(n, l), 0.0);
    float rim = pow(1.0 - max(dot(n, eye), 0.0), 3.0);
    float spec = pow(max(dot(n, normalize(l + eye)), 0.0), 48.0);
    vec3 col = vColor.rgb * (0.42 + 0.68 * diffuse) + spec * 0.32 + rim * vec3(0.05, 0.08, 0.12);
    FragColor = vec4(min(col, vec3(1.0)), 1.0);
})GLSL";

		GLuint VS = CompileShader( GL_VERTEX_SHADER, VertexSource );
		GLuint FS = CompileShader( GL_FRAGMENT_SHADER, FragmentSource );
		if ( !VS || !FS ) return Fail( "Shader compilation" );

		Program = GLX::CreateProgram( );
		GLX::AttachShader( Program, VS );
		GLX::AttachShader( Program, FS );
		GLX::LinkProgram( Program );
		GLX::DeleteShader( VS ); GLX::DeleteShader( FS );

		GLint Linked = 0;
		GLX::GetProgramiv( Program, GL_LINK_STATUS, &Linked );
		if ( !Linked )
		{
			GLint Length = 0;
			GLX::GetProgramiv( Program, GL_INFO_LOG_LENGTH, &Length );
			Error.assign( Length > 0 ? static_cast<size_t>( Length ) : 1, '\0' );
			if ( Length > 0 ) GLX::GetProgramInfoLog( Program, Length, nullptr, &Error[ 0 ] );
			Error.insert( 0, "Link: " );
			return Fail( Error.c_str( ) );
		}
		UniformVP = GLX::GetUniformLocation( Program, "uVP" );

		GLX::GenVertexArrays( 1, &Vao );
		GLX::GenBuffers( 1, &Vbo );
		GLX::BindVertexArray( Vao );
		GLX::BindBuffer( GL_ARRAY_BUFFER, Vbo );
		GLX::BufferData( GL_ARRAY_BUFFER, static_cast<std::ptrdiff_t>( V.size( ) * sizeof( Vertex ) ), V.data( ), GL_STATIC_DRAW );
		const GLsizei Stride = static_cast<GLsizei>( sizeof( Vertex ) );
		GLX::VertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, Stride, reinterpret_cast<void *>( 0 ) );
		GLX::VertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, Stride, reinterpret_cast<void *>( 12 ) );
		GLX::VertexAttribPointer( 2, 4, GL_FLOAT, GL_FALSE, Stride, reinterpret_cast<void *>( 24 ) );
		GLX::EnableVertexAttribArray( 0 );
		GLX::EnableVertexAttribArray( 1 );
		GLX::EnableVertexAttribArray( 2 );
		GLX::BindVertexArray( 0 );
		VertexCount = V.size( );

		constexpr int Width = 1024, Height = 512;
		GLX::GenTextures( 1, &ColorTexture );
		GLX::BindTexture( GL_TEXTURE_2D, ColorTexture );
		GLX::TexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
		GLX::TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		GLX::TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		GLX::TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		GLX::TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

		GLX::GenRenderbuffers( 1, &DepthBuffer );
		GLX::BindRenderbuffer( GL_RENDERBUFFER, DepthBuffer );
		GLX::RenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, Width, Height );

		GLX::GenFramebuffers( 1, &Framebuffer );
		GLX::BindFramebuffer( GL_FRAMEBUFFER, Framebuffer );
		GLX::FramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ColorTexture, 0 );
		GLX::FramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, DepthBuffer );
		if ( GLX::CheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
		{
			GLX::BindFramebuffer( GL_FRAMEBUFFER, 0 );
			return Fail( "Preview framebuffer" );
		}

		D3DXMATRIX View, Proj, VP;
		D3DXVECTOR3 Eye( 5, 3.1f, 6 ), Target( 0, .75f, 0 ), Up( 0, 1, 0 );
		D3DXMatrixLookAtLH( &View, &Eye, &Target, &Up );
		D3DXMatrixPerspectiveFovLH( &Proj, D3DX_PI / 5, 2.f, .1f, 30.f );
		VP = View * Proj;

		glViewport( 0, 0, Width, Height );
		glClearColor( 0.f, 0.f, 0.f, 0.f );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDisable( GL_CULL_FACE );

		GLX::UseProgram( Program );
		if ( UniformVP >= 0 )
			GLX::UniformMatrix4fv( UniformVP, 1, GL_TRUE, &VP._11 );
		GLX::BindVertexArray( Vao );
		glDrawArrays( GL_TRIANGLES, 0, static_cast<GLsizei>( VertexCount ) );
		GLX::BindVertexArray( 0 );
		GLX::UseProgram( 0 );
		glDisable( GL_DEPTH_TEST );

		// Restore the default framebuffer so ImGui keeps drawing to the window.
		GLX::BindFramebuffer( GL_FRAMEBUFFER, 0 );
		GLX::BindTexture( GL_TEXTURE_2D, 0 );
		return true;
	}
}


namespace Custom
{
	class EspPreview {
	public:
		void SetVehicleData(bool Available, const std::string& Name, float Health, float Distance, bool Locked)
		{
			m_HasVehicle = Available; m_Locked = Locked;
			if (!m_VehicleMode) return;
			mWidgets[0].Text = Available ? Name : "Sem veiculo";
			mWidgets[1].Text = Available ? (Locked ? "Trancado" : "Destrancado") : "Status indisponivel";
			mWidgets[2].Text = Available ? "Vida: " + std::to_string(static_cast<int>(Health)) : "Vida: --";
			mWidgets[3].Text = Available ? std::to_string(static_cast<int>(Distance)) + "m" : "Distancia: --";
		}
		explicit EspPreview( bool VehicleMode = false ) : m_VehicleMode( VehicleMode )
		{
			if ( m_VehicleMode )
			{
				mWidgets = {
					cDragWidget{ 0, 0, xorstr( "Obey Tailgater" ), ImColor( 245, 245, 245 ) },
					cDragWidget{ 0, 4, xorstr( "Unlocked" ), ImColor( 255, 205, 80 ) },
					cDragWidget{ 0, 2, xorstr( "Health: 1000" ), ImColor( 95, 235, 135 ) },
					cDragWidget{ 0, 2, xorstr( "28m" ), ImColor( 120, 190, 255 ) },
				};
			}
		}

		void DragDropHandler( )
		{
			Position Positions[ 5 ] =
			{
				{ImVec2( iBox_X, iBox_Y - 3.5f )}, // Top
				{ImVec2( iBox_X + iBox_Width + 3.5f, iBox_Y )}, // Right
				{ImVec2( iBox_X, iBox_Y + iBox_Height + 3.5f )}, // Bottom
				{ImVec2( iBox_X - 3.5f, iBox_Y )}, // Left
				{ImVec2( iBox_X + iBox_Width * 0.5f, iBox_Y + iBox_Height * 0.5f )}, // Center
			};
			if ( m_VehicleMode )
			{
				Positions[ 0 ].Pos.x += iBox_Width * 0.5f;
				Positions[ 1 ].Pos.y += iBox_Height * 0.5f;
				Positions[ 2 ].Pos.x += iBox_Width * 0.5f;
				Positions[ 3 ].Pos.y += iBox_Height * 0.5f;
			}
			const int PositionCount = m_VehicleMode ? 5 : 4;
			const int MovingPosition = m_VehicleMode ? 5 : 4;

			for ( size_t i = 0; i < mWidgets.size( ); i++ )
			{
				auto & Widget = mWidgets.at( i );
				if ( !m_VehicleMode && ( i == 0 || i == 1 ) )
				{
					const bool IsHealth = i == 0;
					const int DesiredType = ( IsHealth
						? g_Config.ESP->HealthStyle
						: g_Config.ESP->ArmorStyle ) == 1 ? 0 : 1;
					if ( Widget.Type != DesiredType )
					{
						Widget.Type = DesiredType;
						Widget.Initialized = false;
					}
					Widget.Text = DesiredType == 0
						? xorstr( "100/100" )
						: ( IsHealth ? xorstr( "HealthBar" ) : xorstr( "ArmorBar" ) );
				}

				if ( !Widget.Initialized )
					continue;

				if ( Widget.AlphaAnimation == 0 )
					continue;

				ImGui::PushID( i );
				{
					ImGui::SetCursorScreenPos( Widget.Position );
					ImGui::InvisibleButton( ( xorstr( "Drag&Drop##" ) + Widget.Text ).c_str( ), Widget.Size + ImVec2( 10, 10 ) );
					bool Hovered = ImGui::IsItemHovered( );
					int Pos = FindClosestPosition( ImGui::GetMousePos( ), Positions, PositionCount, Widget.Type );
					Widget.Helding = false;
					if ( ImGui::BeginDragDropSource( ImGuiDragDropFlags_SourceNoPreviewTooltip ) )
					{
						ImGui::SetDragDropPayload( xorstr( "Drag&Drop" ), &i, sizeof( int ), 0 );
						Widget.Pos = MovingPosition;
						Widget.PossiblePos = Pos;
						Widget.Helding = true;
						ImGui::EndDragDropSource( );
					}
					else if ( Widget.Pos == MovingPosition )
					{
						Widget.Pos = Widget.PossiblePos;
						Widget.PossiblePos = -1;
					}

					Widget.HeldingAnimation = ImClamp( Widget.HeldingAnimation + ( 6 * ImGui::GetIO( ).DeltaTime * ( Widget.Helding ? 1.f : -1.f ) ), 0.f, 1.f );
					//ImGui::GetWindowDrawList( )->AddShadowRect( Widget.Position, Widget.Position + Widget.Size, ImColor( 1.f, 1.f, 1.f, ImGui::GetStyle( ).Alpha * Widget.HeldingAnimation ), 15, ImVec2( 0, 0 ), ImDrawFlags_ShadowCutOutShapeBackground );
				}
				ImGui::PopID( );
			}
		}

		void Draw( )
		{
			if ( m_VehicleMode )
			{
				DrawVehicleMode( );
				return;
			}

			// Box is sized from the card so the stacked labels (name on top, weapon and distance
			// underneath) and the side bars always have room and never get clipped.
			const ImVec2 Avail = ImGui::GetWindowSize( );
			const float LabelRoom = 58.f;

			iBox_Height = ( int ) ImClamp( Avail.y - LabelRoom * 2.f, 80.f, 160.f );
			iBox_Width = iBox_Height / 2.f;

			iBox_X = ImGui::GetWindowPos( ).x + Avail.x / 2 - iBox_Width / 2;
			iBox_Y = ImGui::GetWindowPos( ).y + Avail.y / 2 - iBox_Height / 2;

			Position Positions[ ] =
			{
				{ImVec2( iBox_X, iBox_Y - 3.5f )}, // Top
				{ImVec2( iBox_X + iBox_Width + 3.5f, iBox_Y )}, // Right
				{ImVec2( iBox_X, iBox_Y + iBox_Height + 3.5f )}, // Bottom
				{ImVec2( iBox_X - 3.5f, iBox_Y )}, // Left
			};

			static float BoxAnimation = 0.f;
			BoxAnimation = ImClamp( BoxAnimation + ( 6 * ImGui::GetIO( ).DeltaTime * ( g_Config.ESP->Box ? 1.f : -1.f ) ), 0.f, g_Config.ESP->BoxCol.Value.w );

			static float BoxFillAnimation = 0.f;
			BoxFillAnimation = ImClamp( BoxFillAnimation + ( 6 * ImGui::GetIO( ).DeltaTime * ( g_Config.ESP->FilledBox ? 1.f : -1.f ) ), 0.f, g_Config.ESP->FilledBoxCol.Value.w );

			if ( BoxFillAnimation != 0 )
			{
				ImGui::GetWindowDrawList( )->AddRectFilled( ImVec2( iBox_X, iBox_Y ), ImVec2( iBox_X + iBox_Width, iBox_Y + iBox_Height ), ImColor( ( int ) g_Config.ESP->FilledBoxCol.Value.x * 255, ( int ) g_Config.ESP->FilledBoxCol.Value.y * 255, ( int ) g_Config.ESP->FilledBoxCol.Value.z * 255, ( int ) ( ( int ) ( ImGui::GetStyle( ).Alpha * 255 ) * BoxFillAnimation ) ), 0, ImDrawFlags_None );
			}

			if ( BoxAnimation != 0 )
			{
				ImGui::GetWindowDrawList( )->AddRect( ImVec2( iBox_X, iBox_Y ), ImVec2( iBox_X + iBox_Width, iBox_Y + iBox_Height ), ImColor( 0, 0, 0, ( int ) ( ( int ) ( ImGui::GetStyle( ).Alpha * 255 ) * BoxAnimation ) ), 0, ImDrawFlags_None, 2 );
				ImGui::GetWindowDrawList( )->AddRect( ImVec2( iBox_X, iBox_Y ), ImVec2( iBox_X + iBox_Width, iBox_Y + iBox_Height ), ImColor( ( int ) g_Config.ESP->BoxCol.Value.x * 255, ( int ) g_Config.ESP->BoxCol.Value.y * 255, ( int ) g_Config.ESP->BoxCol.Value.z * 255, ( int ) ( ( int ) ( ImGui::GetStyle( ).Alpha * 255 ) * BoxAnimation ) ), 0, ImDrawFlags_None );
			}

			// Skeleton ESP Preview
			if ( g_Config.ESP->Skeleton )
			{
				const float A = ImGui::GetStyle( ).Alpha;

				// A previa usa a COR CONFIGURADA do esqueleto, nao um verde
				// fixo: assim ela mostra de verdade como vai sair no jogo.
				const ImVec4 Base = g_Config.ESP->SkeletonCol.Value;

				const ImU32 ColGlow = ImGui::GetColorU32( ImVec4( Base.x, Base.y, Base.z, 0.20f * A ) );
				const ImU32 ColCore = ImGui::GetColorU32( ImVec4( Base.x, Base.y, Base.z, Base.w * A ) );
				const ImU32 ColSoft = ImGui::GetColorU32( ImVec4( Base.x, Base.y, Base.z, 0.45f * A ) );

				// Corpo por tras do esqueleto. Um esqueleto sozinho no vazio nao
				// diz nada: com a silhueta atras a previa vira a mesma cena que
				// o jogador ve em partida, o esqueleto grudado num boneco.
				const ImU32 ColBody = ImGui::GetColorU32( ImVec4( 0.24f, 0.245f, 0.27f, 0.95f * A ) );
				const ImU32 ColBodyDark = ImGui::GetColorU32( ImVec4( 0.16f, 0.165f, 0.19f, 0.95f * A ) );

				// ---------------------------------------------------------------
				// Esqueleto: MESMOS 14 ossos do Esp.cpp, na mesma ordem. Alturas
				// em fracoes do corpo, nas proporcoes de uma figura em pe.
				// ---------------------------------------------------------------
				const float CenterX = iBox_X + iBox_Width / 2.f;
				const float TopY = iBox_Y + iBox_Height * 0.06f;
				const float BottomY = iBox_Y + iBox_Height * 0.94f;
				const float H = BottomY - TopY;

				const float HalfW = ImMin( iBox_Width * 0.5f - 10.f, H * 0.22f );
				const float T = ImMax( 1.3f, H * 0.011f );
				const float HeadR = H * 0.070f;

				// Idle: respiracao, troca de peso de uma perna para a outra e um
				// balanco leve dos bracos, em frequencias diferentes para o ciclo
				// nao repetir em bloco. Amplitudes em fracao da altura, entao o
				// movimento escala junto com a previa.
				const float Time = ( float ) ImGui::GetTime( );
				const float Breath = ImSin( Time * 1.7f );
				const float Sway = ImSin( Time * 0.8f );
				const float ArmL = ImSin( Time * 1.05f + 0.7f );
				const float ArmR = ImSin( Time * 1.05f + 2.5f );

				const float Drift = Sway * H * 0.007f;     // corpo inteiro
				const float Lift = Breath * H * 0.006f;    // peito subindo

				const ImVec2 Head( CenterX + Drift * 1.5f, TopY + HeadR - Lift * 1.4f );
				const ImVec2 Neck( CenterX + Drift * 1.2f, TopY + HeadR * 2.05f - Lift );
				const ImVec2 Pelvis( CenterX + Drift * 0.5f, TopY + H * 0.500f );

				const ImVec2 LUpper( CenterX - HalfW * 0.50f + Drift, Neck.y + H * 0.055f );
				const ImVec2 RUpper( CenterX + HalfW * 0.50f + Drift, LUpper.y );
				const ImVec2 LFore( CenterX - HalfW * 0.60f + Drift + ArmL * H * 0.008f, TopY + H * 0.335f );
				const ImVec2 RFore( CenterX + HalfW * 0.60f + Drift + ArmR * H * 0.008f, LFore.y );
				const ImVec2 LHand( CenterX - HalfW * 0.64f + Drift + ArmL * H * 0.014f, TopY + H * 0.455f + ArmL * H * 0.006f );
				const ImVec2 RHand( CenterX + HalfW * 0.64f + Drift + ArmR * H * 0.014f, TopY + H * 0.455f + ArmR * H * 0.006f );

				const ImVec2 LThigh( CenterX - HalfW * 0.26f + Drift * 0.5f, Pelvis.y + H * 0.035f );
				const ImVec2 RThigh( CenterX + HalfW * 0.26f + Drift * 0.5f, LThigh.y );
				const ImVec2 LCalf( CenterX - HalfW * 0.30f + Drift * 0.25f, TopY + H * 0.735f );
				const ImVec2 RCalf( CenterX + HalfW * 0.30f + Drift * 0.25f, LCalf.y );

				// Os pes ficam plantados: so o corpo acima deles se mexe, que e o
				// que faz o balanco ler como peso trocando de perna.
				const ImVec2 LFoot( CenterX - HalfW * 0.30f, BottomY );
				const ImVec2 RFoot( CenterX + HalfW * 0.30f, BottomY );

				// Mesmo encurtamento do ESP: o osso da cabeca fica na altura
				// dos olhos, a linha para na altura da boca.
				const ImVec2 HeadTip(
					Neck.x + ( Head.x - Neck.x ) * 0.70f,
					Neck.y + ( Head.y - Neck.y ) * 0.70f );

				const ImVec2 Bones[ ][ 2 ] = {
					{ HeadTip, Neck  }, { Neck,   Pelvis },
					{ Neck,   LUpper }, { Neck,   RUpper },
					{ LUpper, LFore  }, { RUpper, RFore  },
					{ LFore,  LHand  }, { RFore,  RHand  },
					{ Pelvis, LThigh }, { Pelvis, RThigh },
					{ LThigh, LCalf  }, { RThigh, RCalf  },
					{ LCalf,  LFoot  }, { RCalf,  RFoot  },
				};

				const ImVec2 Joints[ ] = {
					Neck, Pelvis, LUpper, RUpper, LFore, RFore,
					LHand, RHand, LThigh, RThigh, LCalf, RCalf, LFoot, RFoot,
				};

				ImDrawList * Draw = ImGui::GetWindowDrawList( );

				// ---------------- corpo ----------------
				const float LimbW = H * 0.052f;
				const float ArmW = H * 0.040f;

				// Membros como capsulas: linha grossa mais um circulo em cada
				// ponta, porque AddLine nao tem ponta arredondada e a junta
				// ficaria com um degrau quadrado.
				const auto Limb = [ & ] ( const ImVec2 & P, const ImVec2 & Q, float W ) {
					Draw->AddLine( P, Q, ColBody, W );
					Draw->AddCircleFilled( P, W * 0.5f, ColBody, 16 );
					Draw->AddCircleFilled( Q, W * 0.5f, ColBody, 16 );
					};

				Limb( LUpper, LFore, ArmW ); Limb( LFore, LHand, ArmW * 0.85f );
				Limb( RUpper, RFore, ArmW ); Limb( RFore, RHand, ArmW * 0.85f );
				Limb( LThigh, LCalf, LimbW ); Limb( LCalf, LFoot, LimbW * 0.8f );
				Limb( RThigh, RCalf, LimbW ); Limb( RCalf, RFoot, LimbW * 0.8f );

				// Tronco: quadrilatero ombro-ombro-quadril-quadril, alargado nos
				// ombros para nao ficar um retangulo reto.
				const ImVec2 Torso[ ] = {
					ImVec2( LUpper.x - ArmW * 0.25f, LUpper.y - LimbW * 0.30f ),
					ImVec2( RUpper.x + ArmW * 0.25f, RUpper.y - LimbW * 0.30f ),
					ImVec2( RThigh.x + LimbW * 0.35f, RThigh.y ),
					ImVec2( LThigh.x - LimbW * 0.35f, LThigh.y ),
				};
				Draw->AddConvexPolyFilled( Torso, 4, ColBody );

				Draw->AddLine( Neck, Pelvis, ColBodyDark, LimbW * 0.30f );
				Draw->AddCircleFilled( Head, HeadR * 1.12f, ColBody, 32 );

				// ---------------- esqueleto por cima ----------------
				// Passadas separadas, e nao osso a osso: assim o halo de um osso
				// nunca cobre o traco cheio do osso vizinho nos cruzamentos.
				for ( const auto & B : Bones )
					Draw->AddLine( B[ 0 ], B[ 1 ], ColGlow, T + 3.f );

				for ( const auto & B : Bones )
					Draw->AddLine( B[ 0 ], B[ 1 ], ColCore, T );

				const float NodeR = ImMax( 1.6f, T * 0.95f );

				for ( const auto & J : Joints )
				{
					Draw->AddCircleFilled( J, NodeR + 1.2f, ColSoft, 12 );
					Draw->AddCircleFilled( J, NodeR, ColCore, 12 );
				}

				Draw->AddCircleFilled( Head, NodeR + 1.2f, ColSoft, 12 );
				Draw->AddCircleFilled( Head, NodeR, ColCore, 12 );

				// O circulo da cabeca e uma opcao SEPARADA no ESP. A previa
				// desenhava ele sempre que o esqueleto estava ligado, entao
				// mostrava na tela uma coisa que o jogo nao ia desenhar.
				if ( g_Config.ESP->HeadCircle )
				{
					Draw->AddCircle( Head, HeadR, ColGlow, 32, T + 3.f );
					Draw->AddCircle( Head, HeadR, ColCore, 32, T );
				}
			}

			for ( size_t i = 0; i < mWidgets.size( ); i++ )
			{
				auto & Widget = mWidgets.at( i );

				if ( Widget.IsIcon )
					ImGui::PushFont( g_Variables.FontAwesomeSolidSmall );
				else
					ImGui::PushFont( Widget.SmallFont ? g_Variables.m_FontSmaller : g_Variables.m_FontSecundary );

				if ( !Widget.Initialized )
				{
					Widget.Position = Positions[ Widget.InitialPos ].Pos;
					Widget.Pos = Widget.InitialPos;

					if ( Widget.Type == 0 )
					{
						Widget.Size = ImGui::CalcTextSize( Widget.Text.c_str( ) );
					}
					else if ( Widget.Type == 1 )
					{
						if ( Widget.InitialPos == 0 || Widget.InitialPos == 2 )
						{
							Widget.Size = ImVec2( iBox_Width, 2 );
						}
						else
						{
							Widget.Size = ImVec2( 2, iBox_Height );
						}
					}

					Widget.Initialized = true;
				}

				switch ( i )
				{
				case 0:
					Widget.Enabled = g_Config.ESP->HealthBar;
					g_Config.ESP->HealthBarPos = Widget.RawPos;
					break;
				case 1:
					Widget.Enabled = g_Config.ESP->ArmorBar;
					g_Config.ESP->ArmorBarPos = Widget.RawPos;
					break;
				case 2:
					Widget.Enabled = g_Config.ESP->UserNames;
					g_Config.ESP->UserNamesPos = Widget.RawPos;
					break;
				case 3:
					Widget.Enabled = g_Config.ESP->WeaponName;
					g_Config.ESP->WeaponNamePos = Widget.RawPos;
					break;
				case 4:
					Widget.Enabled = g_Config.ESP->DistanceFromMe;
					g_Config.ESP->DistanceFromMePos = Widget.RawPos;
					break;
				}

				static int seloco = 0;

				switch ( i )
				{
				case 0:
					if ( Widget.Pos != 4 )
					{
						if ( g_Config.ESP->UpdateCfgESP )
						{
							Widget.Pos = g_Config.ESP->HealthBarState;
							seloco++;
						}
						else
							g_Config.ESP->HealthBarState = Widget.Pos;
					}
					break;
				case 1:
					if ( Widget.Pos != 4 )
					{
						if ( g_Config.ESP->UpdateCfgESP )
						{
							Widget.Pos = g_Config.ESP->ArmorBarState;		seloco++;
						}
						else
							g_Config.ESP->ArmorBarState = Widget.Pos;
					}
					break;
				case 2:
					if ( Widget.Pos != 4 )
					{
						if ( g_Config.ESP->UpdateCfgESP )
						{
							Widget.Pos = g_Config.ESP->UserNamesState;		seloco++;
						}
						else
							g_Config.ESP->UserNamesState = Widget.Pos;
					}
					break;
				case 3:
					if ( Widget.Pos != 4 )
					{
						if ( g_Config.ESP->UpdateCfgESP )
						{
							Widget.Pos = g_Config.ESP->WeaponNameState;		seloco++;
						}
						else
							g_Config.ESP->WeaponNameState = Widget.Pos;
					}
					break;
				case 4:
					if ( Widget.Pos != 4 )
					{
						if ( g_Config.ESP->UpdateCfgESP )
						{
							Widget.Pos = g_Config.ESP->DistanceFromMeState;		seloco++;
						}
						else {
							g_Config.ESP->DistanceFromMeState = Widget.Pos;
						}
					}
					break;
				}


				Widget.AlphaAnimation = ImClamp( Widget.AlphaAnimation + ( 6 * ImGui::GetIO( ).DeltaTime * ( Widget.Enabled ? 1.f : -1.f ) ), 0.f, 1.f );
				if ( Widget.AlphaAnimation == 0 )
				{
					ImGui::PopFont( );
					continue;
				}

				if ( Widget.Helding )
				{
					switch ( Widget.PossiblePos )
					{
					case 0:
						PaddingY[ 0 ] += -Widget.Size.y - 2;
						break;
					case 1:
						Widget.Type == 0 ? PaddingY[ 1 ] += Widget.Size.y + 2 : PaddingX[ 1 ] += Widget.Size.x + 2;
						break;
					case 2:
						PaddingY[ 2 ] += Widget.Size.y + 2;
						break;
					case 3:
						Widget.Type == 0 ? PaddingY[ 3 ] += Widget.Size.y + 2 : PaddingX[ 3 ] += -Widget.Size.x - 2;
						break;
					}
				}

				if ( Widget.Type == 0 ) //Text
				{
					if ( Widget.Pos == 0 ) // Top
					{
						Widget.Position = Positions[ Widget.Pos ].Pos + ImVec2( iBox_Width / 2 - Widget.Size.x / 2, -Widget.Size.y + PaddingY[ 0 ] );
						Widget.RawPos = ImVec2( 0, -Widget.Size.y + PaddingY[ 0 ] );
						PaddingY[ 0 ] += -Widget.Size.y - 1;
					}
					else if ( Widget.Pos == 1 ) // Right
					{
						Widget.Position = Positions[ Widget.Pos ].Pos + ImVec2( PaddingX[ 1 ], PaddingY[ 1 ] );
						Widget.RawPos = ImVec2( PaddingX[ 1 ], PaddingY[ 1 ] );
						PaddingY[ 1 ] += Widget.Size.y + 1;
					}
					else if ( Widget.Pos == 2 ) // Bottom
					{
						Widget.Position = Positions[ Widget.Pos ].Pos + ImVec2( iBox_Width / 2 - Widget.Size.x / 2, PaddingY[ 2 ] );
						Widget.RawPos = ImVec2( 0, PaddingY[ 2 ] );
						PaddingY[ 2 ] += Widget.Size.y + 1;
					}
					else if ( Widget.Pos == 3 ) // Left
					{
						Widget.Position = Positions[ Widget.Pos ].Pos + ImVec2( -Widget.Size.x + PaddingX[ 3 ], PaddingY[ 3 ] );
						Widget.RawPos = ImVec2( PaddingX[ 3 ], PaddingY[ 3 ] );
						PaddingY[ 3 ] += Widget.Size.y + 1;
					}
					else if ( Widget.Pos == 4 ) // Moving
					{
						Widget.Position = ImLerp( Widget.Position, ImGui::GetMousePos( ) + ImVec2( -Widget.Size.x / 2.f, 0 ), ImGui::GetIO( ).DeltaTime * 8 );
					}

					ImGui::GetWindowDrawList( )->AddText( Widget.Position, ImColor( 0.f, 0.f, 0.f, ImGui::GetStyle( ).Alpha * Widget.AlphaAnimation ), Widget.Text.c_str( ) );
					ImGui::GetWindowDrawList( )->AddText( Widget.Position + ImVec2( 1, 1 ), ImGui::GetColorU32( ImVec4( Widget.Color.Value.x, Widget.Color.Value.y, Widget.Color.Value.z, ImGui::GetStyle( ).Alpha * Widget.AlphaAnimation ) ), Widget.Text.c_str( ) );
				}
				else if ( Widget.Type == 1 ) //Bar
				{
					if ( Widget.Pos == 0 ) // Top
					{
						Widget.Size = ImVec2( iBox_Width, 3 );
						Widget.Position = Positions[ Widget.Pos ].Pos + ImVec2( 0, -Widget.Size.y + PaddingY[ 0 ] );
						Widget.RawPos = ImVec2( 0, -Widget.Size.y + PaddingY[ 0 ] );
						PaddingY[ 0 ] += -Widget.Size.y - 4;
					}
					else if ( Widget.Pos == 1 ) // Right
					{
						Widget.Size = ImVec2( 3, iBox_Height );
						Widget.Position = Positions[ Widget.Pos ].Pos + ImVec2( PaddingX[ 1 ], 0 );
						Widget.RawPos = ImVec2( PaddingX[ 1 ], 0 );
						PaddingX[ 1 ] += Widget.Size.x + 4;
					}
					else if ( Widget.Pos == 2 ) // Bottom
					{
						Widget.Size = ImVec2( iBox_Width, 3 );
						Widget.Position = Positions[ Widget.Pos ].Pos + ImVec2( 0, PaddingY[ 2 ] );
						Widget.RawPos = ImVec2( 0, PaddingY[ 2 ] );
						PaddingY[ 2 ] += Widget.Size.y + 4;
					}
					else if ( Widget.Pos == 3 ) // Left
					{
						Widget.Size = ImVec2( 3, iBox_Height );
						Widget.Position = Positions[ Widget.Pos ].Pos + ImVec2( -Widget.Size.x + PaddingX[ 3 ], 0 );
						Widget.RawPos = ImVec2( -Widget.Size.x + PaddingX[ 3 ], 0 );
						PaddingX[ 3 ] += -Widget.Size.x - 4;
					}
					else if ( Widget.Pos == 4 ) // Moving
					{
						if ( Widget.PossiblePos == 0 || Widget.PossiblePos == 2 )
							Widget.Size = ImVec2( iBox_Width, 3 );
						else if ( Widget.PossiblePos == 1 || Widget.PossiblePos == 3 )
							Widget.Size = ImVec2( 3, iBox_Height );

						Widget.Position = ImLerp( Widget.Position, ImGui::GetMousePos( ) + ImVec2( 0, 0 ), ImGui::GetIO( ).DeltaTime * 8 );
					}

					ImGui::GetWindowDrawList( )->AddRectFilled( Widget.Position - ImVec2( 1, 1 ), Widget.Position + Widget.Size + ImVec2( 1, 1 ), ImGui::GetColorU32( ImVec4( ImColor( 0, 0, 0 ) ), ImGui::GetStyle( ).Alpha * Widget.AlphaAnimation ) );
					ImGui::GetWindowDrawList( )->AddRectFilled( Widget.Position, Widget.Position + Widget.Size, ImGui::GetColorU32( ImVec4( ImColor( 80, 80, 80 ) ), ImGui::GetStyle( ).Alpha * ( 125.f / 255.f ) ) );
					ImGui::GetWindowDrawList( )->AddRectFilled( Widget.Position, Widget.Position + Widget.Size, ImGui::GetColorU32( ImVec4( Widget.Color.Value.x, Widget.Color.Value.y, Widget.Color.Value.z, ImGui::GetStyle( ).Alpha * Widget.AlphaAnimation ) ) );
				}

				ImGui::PopFont( );


				if ( seloco == 4 )
				{
					g_Config.ESP->UpdateCfgESP = false;
					seloco = 0;
				}
			}

			for ( size_t i = 0; i < 4; i++ )
			{
				PaddingY[ i ] = 0;
				PaddingX[ i ] = 0;
			}
		}

	private:
		class cDragWidget {
		public:
			int Type; // 0 = Text | 1 = Bar
			int InitialPos; // 0 = Top | 1 = Right | 2 = Bottom | 3 = Left
			std::string Text;
			ImColor Color;
			bool IsIcon;
			bool SmallFont = false;
			bool Initialized = false;
			bool Enabled;
			ImVec2 Position;
			int Pos;
			ImVec2 Size;
			int PossiblePos;
			bool Helding;
			float HeldingAnimation;
			float AlphaAnimation;
			ImVec2 RawPos;
		};

		float PaddingY[ 4 ] = { 0, 0, 0, 0 };
		float PaddingX[ 4 ] = { 0, 0, 0, 0 };

		struct Position {
			ImVec2 Pos;
		};

		std::vector<cDragWidget> mWidgets =
		{
			cDragWidget{ 1, 3, xorstr( "HealthBar" ), ImColor( 66, 245, 132 ) },
			cDragWidget{ 1, 3, xorstr( "ArmorBar" ), ImColor( 85, 128, 200 ) },
			cDragWidget{ 0, 0, xorstr( "Name" ), ImColor( g_Config.ESP->UserNamesCol ) },
			cDragWidget{ 0, 2, xorstr( "Unarmed" ), ImColor( g_Config.ESP->WeaponNameCol ), false, true },
			cDragWidget{ 0, 2, xorstr( "0m" ), ImColor( g_Config.ESP->DistanceCol ), false, true }
		};

	private:
		int FindClosestPosition( ImVec2 CurrentPos, Position Positions[ ], int PositionCount, int WidgetType )
		{
			float Closest = FLT_MAX;
			int Best = -1;
			for ( int i = 0; i < PositionCount; i++ )
			{
				if ( !m_VehicleMode && WidgetType == 0 && ( i == 1 || i == 3 ) )
					continue; // Ignore as posi��es 1 (Direita) e 3 (Esquerda) para widgets do tipo 0 (texto)

				ImVec2 Pos = Positions[ i ].Pos;
				float dist = std::sqrt( ( CurrentPos.x - Pos.x ) * ( CurrentPos.x - Pos.x ) + ( CurrentPos.y - Pos.y ) * ( CurrentPos.y - Pos.y ) );

				if ( Closest > dist )
				{
					Closest = dist;
					Best = i;
				}
			}

			return Best;
		}

		void DrawVehicleMode( )
		{
			const ImVec2 WindowPos = ImGui::GetWindowPos( );
			const ImVec2 WindowSize = ImGui::GetWindowSize( );
			ImDrawList * DrawList = ImGui::GetWindowDrawList( );

			if ( !ImGui::IsRectVisible( WindowPos, WindowPos + WindowSize ) ) return;

			// Uma pequena cena, em vez da textura solta no card. O fundo, a borda
			// e a sombra no chao deixam o modelo legivel mesmo quando a cor do menu
			// e muito escura.
			iBox_Width = static_cast<int>( ImMin( WindowSize.x - 28.f, ( WindowSize.y - 76.f ) * 1.85f ) );
			iBox_Width = ImMax( iBox_Width, 100 );
			iBox_Height = static_cast<int>( iBox_Width * 0.52f );
			iBox_X = static_cast<int>( WindowPos.x + ( WindowSize.x - iBox_Width ) * 0.5f );
			iBox_Y = static_cast<int>( WindowPos.y + ( WindowSize.y - iBox_Height ) * 0.48f );

			const ImVec2 SceneMin( static_cast<float>( iBox_X ), static_cast<float>( iBox_Y ) );
			const ImVec2 SceneMax( SceneMin.x + iBox_Width, SceneMin.y + iBox_Height );
			const ImVec2 Center = ( SceneMin + SceneMax ) * 0.5f;
			const float Alpha = ImGui::GetStyle( ).Alpha;

			DrawList->AddShadowRect( SceneMin, SceneMax, ImGui::GetColorU32( ImVec4( 0.f, 0.f, 0.f, 0.42f * Alpha ) ),
				18.f, ImVec2( 0.f, 5.f ), ImDrawFlags_ShadowCutOutShapeBackground, 9.f );
			DrawList->AddRectFilled( SceneMin, SceneMax, ImGui::GetColorU32( ImVec4( 0.025f, 0.032f, 0.045f, 0.78f * Alpha ) ), 9.f );
			DrawList->AddRectFilledMultiColor( SceneMin, SceneMax,
				ImGui::GetColorU32( ImVec4( 0.10f, 0.22f, 0.38f, 0.20f * Alpha ) ),
				ImGui::GetColorU32( ImVec4( 0.04f, 0.05f, 0.08f, 0.05f * Alpha ) ),
				ImGui::GetColorU32( ImVec4( 0.02f, 0.025f, 0.04f, 0.02f * Alpha ) ),
				ImGui::GetColorU32( ImVec4( 0.07f, 0.12f, 0.22f, 0.14f * Alpha ) ),
				9.f, ImDrawFlags_RoundCornersAll );
			DrawList->AddRect( SceneMin, SceneMax, ImGui::GetColorU32( ImVec4( 0.24f, 0.48f, 0.78f, 0.28f * Alpha ) ), 9.f );

			DrawList->PathClear( );
			for ( int Step = 0; Step < 48; ++Step )
			{
				const float Angle = ( D3DX_PI * 2.f * Step ) / 48.f;
				DrawList->PathLineTo( ImVec2( Center.x + cosf( Angle ) * iBox_Width * 0.31f,
					SceneMax.y - iBox_Height * 0.16f + sinf( Angle ) * iBox_Height * 0.075f ) );
			}
			DrawList->PathFillConvex( ImGui::GetColorU32( ImVec4( 0.f, 0.f, 0.f, 0.34f * Alpha ) ) );

			DrawMichaelCar( DrawList, SceneMin + ImVec2( 8.f, 2.f ), SceneMax - ImVec2( 8.f, 2.f ) );

			float PanelWidth = 0.f;
			float PanelHeight = 12.f;
			for ( size_t Index = 0; Index < mWidgets.size( ); ++Index )
			{
				auto & Widget = mWidgets[ Index ];
				Widget.Enabled = VehicleEnabled( static_cast<int>( Index ) );
				Widget.Size = ImGui::CalcTextSize( Widget.Text.c_str( ) );
				if ( Index == 1 ) Widget.Size.x += 14.f;
				Widget.Pos = 4;
				Widget.Initialized = true;
				Widget.AlphaAnimation = ImClamp( Widget.AlphaAnimation +
					6.f * ImGui::GetIO( ).DeltaTime * ( Widget.Enabled ? 1.f : -1.f ), 0.f, 1.f );
				if ( Widget.AlphaAnimation > 0.f )
				{
					PanelWidth = ImMax( PanelWidth, Widget.Size.x );
					PanelHeight += Widget.Size.y + 3.f;
				}
			}

			PanelWidth = ImMin( PanelWidth + 24.f, iBox_Width - 20.f );
			const ImVec2 PanelMin( Center.x - PanelWidth * 0.5f, Center.y - PanelHeight * 0.5f );
			const ImVec2 PanelMax( Center.x + PanelWidth * 0.5f, Center.y + PanelHeight * 0.5f );
			DrawList->AddRectFilled( PanelMin, PanelMax, ImGui::GetColorU32( ImVec4( 0.015f, 0.02f, 0.03f, 0.78f * Alpha ) ), 6.f );
			DrawList->AddRect( PanelMin, PanelMax, ImGui::GetColorU32( ImVec4( 0.45f, 0.65f, 0.92f, 0.28f * Alpha ) ), 6.f );

			float TextY = PanelMin.y + 6.f;
			for ( size_t Index = 0; Index < mWidgets.size( ); ++Index )
			{
				auto & Widget = mWidgets[ Index ];
				if ( Widget.AlphaAnimation <= 0.f ) continue;

				Widget.Position = ImVec2( Center.x - Widget.Size.x * 0.5f, TextY );
				ImVec2 TextPosition = Widget.Position;
				if ( Index == 1 )
				{
					const ImVec4 StatusColor = !m_HasVehicle ? ImVec4( .5f, .5f, .5f, Alpha ) :
						( m_Locked ? ImVec4( .95f, .20f, .22f, Alpha ) : ImVec4( .20f, .85f, .40f, Alpha ) );
					DrawList->AddCircleFilled( TextPosition + ImVec2( 5.f, Widget.Size.y * .5f ), 4.f,
						ImGui::GetColorU32( StatusColor ), 16 );
					TextPosition.x += 14.f;
				}
				DrawList->AddText( TextPosition + ImVec2( 1.f, 1.f ), ImColor( 0.f, 0.f, 0.f, Alpha * Widget.AlphaAnimation ), Widget.Text.c_str( ) );
				DrawList->AddText( TextPosition, ImGui::GetColorU32( ImVec4( Widget.Color.Value.x, Widget.Color.Value.y,
					Widget.Color.Value.z, Alpha * Widget.AlphaAnimation ) ), Widget.Text.c_str( ) );
				TextY += Widget.Size.y + 3.f;
			}

			for ( size_t Index = 0; Index < 4; ++Index )
			{
				PaddingY[ Index ] = 0.f;
				PaddingX[ Index ] = 0.f;
			}

			const char * Hint = m_HasVehicle ? "Dados do veiculo mais proximo" : "Sem veiculo disponivel";
			const ImVec2 HintSize = ImGui::CalcTextSize( Hint );
			DrawList->AddText( ImVec2( WindowPos.x + ( WindowSize.x - HintSize.x ) * 0.5f,
				WindowPos.y + WindowSize.y - HintSize.y - 9.f ), ImColor( 145, 145, 155, 210 ), Hint );
		}

		static bool VehicleEnabled( int Index )
		{
			switch ( Index )
			{
			case 0: return g_Config.VehicleESP->VehName;
			case 1: return g_Config.VehicleESP->ShowLockUnlock;
			case 2: return g_Config.VehicleESP->VehHealth;
			default: return g_Config.VehicleESP->DistanceFromMe;
			}
		}

		static int & VehicleState( int Index )
		{
			switch ( Index )
			{
			case 0: return g_Config.VehicleESP->VehNameState;
			case 1: return g_Config.VehicleESP->LockState;
			case 2: return g_Config.VehicleESP->VehHealthState;
			default: return g_Config.VehicleESP->DistanceState;
			}
		}

		static void DrawMichaelCar( ImDrawList * DrawList, const ImVec2 & Min, const ImVec2 & Max )
		{
			if (VehicleModel::Prepare())
				DrawList->AddImage((ImTextureID)GLTexture( VehicleModel::ColorTexture ), Min, Max,
					ImVec2(0,0), ImVec2(1,1), ImGui::GetColorU32(ImVec4(1,1,1,1)));
			else
				DrawList->AddText(Min, ImColor(200,140,140), "Preview 3D indisponivel");
		}

	private:
		int iBox_X;
		int iBox_Y;
		int iBox_Width;
		int iBox_Height;
		bool m_VehicleMode = false;
		bool m_HasVehicle = false;
		bool m_Locked = false;

	};
	inline EspPreview g_EspPreview;

}