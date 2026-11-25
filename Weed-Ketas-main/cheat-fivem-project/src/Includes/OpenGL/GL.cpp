#include "pch.h"
#include <Includes/OpenGL/GL.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include <Includes/ImGui/Files/stb_image.h>

namespace GLX
{
	PFN_GenVertexArrays GenVertexArrays = nullptr;
	PFN_BindVertexArray BindVertexArray = nullptr;
	PFN_DeleteVertexArrays DeleteVertexArrays = nullptr;
	PFN_GenBuffers GenBuffers = nullptr;
	PFN_BindBuffer BindBuffer = nullptr;
	PFN_BufferData BufferData = nullptr;
	PFN_DeleteBuffers DeleteBuffers = nullptr;
	PFN_VertexAttribPointer VertexAttribPointer = nullptr;
	PFN_EnableVertexAttribArray EnableVertexAttribArray = nullptr;
	PFN_DisableVertexAttribArray DisableVertexAttribArray = nullptr;
	PFN_CreateShader CreateShader = nullptr;
	PFN_ShaderSource ShaderSource = nullptr;
	PFN_CompileShader CompileShader = nullptr;
	PFN_GetShaderiv GetShaderiv = nullptr;
	PFN_GetShaderInfoLog GetShaderInfoLog = nullptr;
	PFN_DeleteShader DeleteShader = nullptr;
	PFN_CreateProgram CreateProgram = nullptr;
	PFN_AttachShader AttachShader = nullptr;
	PFN_LinkProgram LinkProgram = nullptr;
	PFN_GetProgramiv GetProgramiv = nullptr;
	PFN_GetProgramInfoLog GetProgramInfoLog = nullptr;
	PFN_UseProgram UseProgram = nullptr;
	PFN_DeleteProgram DeleteProgram = nullptr;
	PFN_GetUniformLocation GetUniformLocation = nullptr;
	PFN_UniformMatrix4fv UniformMatrix4fv = nullptr;
	PFN_GenTextures GenTextures = nullptr;
	PFN_BindTexture BindTexture = nullptr;
	PFN_TexImage2D TexImage2D = nullptr;
	PFN_TexParameteri TexParameteri = nullptr;
	PFN_DeleteTextures DeleteTextures = nullptr;
	PFN_ActiveTexture ActiveTexture = nullptr;
	PFN_GenFramebuffers GenFramebuffers = nullptr;
	PFN_BindFramebuffer BindFramebuffer = nullptr;
	PFN_FramebufferTexture2D FramebufferTexture2D = nullptr;
	PFN_CheckFramebufferStatus CheckFramebufferStatus = nullptr;
	PFN_DeleteFramebuffers DeleteFramebuffers = nullptr;
	PFN_GenRenderbuffers GenRenderbuffers = nullptr;
	PFN_BindRenderbuffer BindRenderbuffer = nullptr;
	PFN_RenderbufferStorage RenderbufferStorage = nullptr;
	PFN_FramebufferRenderbuffer FramebufferRenderbuffer = nullptr;
	PFN_DeleteRenderbuffers DeleteRenderbuffers = nullptr;
	PFN_SwapInterval SwapInterval = nullptr;

	template < typename T >
	static T Load( const char * Name )
	{
		void * Proc = GetProc( Name );
		return reinterpret_cast<T>( Proc );
	}

	void * GetProc( const char * Name )
	{
		if ( !Name )
			return nullptr;

		void * Proc = reinterpret_cast<void *>( wglGetProcAddress( Name ) );
		if ( !Proc || Proc == reinterpret_cast<void *>( 0x1 ) ||
			Proc == reinterpret_cast<void *>( 0x2 ) || Proc == reinterpret_cast<void *>( 0x3 ) ||
			Proc == reinterpret_cast<void *>( -1 ) )
		{
			Proc = reinterpret_cast<void *>( GetProcAddress( GetModuleHandleA( "opengl32.dll" ), Name ) );
		}
		return Proc;
	}

	bool Init( )
	{
		GenVertexArrays = Load<PFN_GenVertexArrays>( "glGenVertexArrays" );
		BindVertexArray = Load<PFN_BindVertexArray>( "glBindVertexArray" );
		DeleteVertexArrays = Load<PFN_DeleteVertexArrays>( "glDeleteVertexArrays" );
		GenBuffers = Load<PFN_GenBuffers>( "glGenBuffers" );
		BindBuffer = Load<PFN_BindBuffer>( "glBindBuffer" );
		BufferData = Load<PFN_BufferData>( "glBufferData" );
		DeleteBuffers = Load<PFN_DeleteBuffers>( "glDeleteBuffers" );
		VertexAttribPointer = Load<PFN_VertexAttribPointer>( "glVertexAttribPointer" );
		EnableVertexAttribArray = Load<PFN_EnableVertexAttribArray>( "glEnableVertexAttribArray" );
		DisableVertexAttribArray = Load<PFN_DisableVertexAttribArray>( "glDisableVertexAttribArray" );
		CreateShader = Load<PFN_CreateShader>( "glCreateShader" );
		ShaderSource = Load<PFN_ShaderSource>( "glShaderSource" );
		CompileShader = Load<PFN_CompileShader>( "glCompileShader" );
		GetShaderiv = Load<PFN_GetShaderiv>( "glGetShaderiv" );
		GetShaderInfoLog = Load<PFN_GetShaderInfoLog>( "glGetShaderInfoLog" );
		DeleteShader = Load<PFN_DeleteShader>( "glDeleteShader" );
		CreateProgram = Load<PFN_CreateProgram>( "glCreateProgram" );
		AttachShader = Load<PFN_AttachShader>( "glAttachShader" );
		LinkProgram = Load<PFN_LinkProgram>( "glLinkProgram" );
		GetProgramiv = Load<PFN_GetProgramiv>( "glGetProgramiv" );
		GetProgramInfoLog = Load<PFN_GetProgramInfoLog>( "glGetProgramInfoLog" );
		UseProgram = Load<PFN_UseProgram>( "glUseProgram" );
		DeleteProgram = Load<PFN_DeleteProgram>( "glDeleteProgram" );
		GetUniformLocation = Load<PFN_GetUniformLocation>( "glGetUniformLocation" );
		UniformMatrix4fv = Load<PFN_UniformMatrix4fv>( "glUniformMatrix4fv" );
		GenTextures = Load<PFN_GenTextures>( "glGenTextures" );
		BindTexture = Load<PFN_BindTexture>( "glBindTexture" );
		TexImage2D = Load<PFN_TexImage2D>( "glTexImage2D" );
		TexParameteri = Load<PFN_TexParameteri>( "glTexParameteri" );
		DeleteTextures = Load<PFN_DeleteTextures>( "glDeleteTextures" );
		ActiveTexture = Load<PFN_ActiveTexture>( "glActiveTexture" );
		GenFramebuffers = Load<PFN_GenFramebuffers>( "glGenFramebuffers" );
		BindFramebuffer = Load<PFN_BindFramebuffer>( "glBindFramebuffer" );
		FramebufferTexture2D = Load<PFN_FramebufferTexture2D>( "glFramebufferTexture2D" );
		CheckFramebufferStatus = Load<PFN_CheckFramebufferStatus>( "glCheckFramebufferStatus" );
		DeleteFramebuffers = Load<PFN_DeleteFramebuffers>( "glDeleteFramebuffers" );
		GenRenderbuffers = Load<PFN_GenRenderbuffers>( "glGenRenderbuffers" );
		BindRenderbuffer = Load<PFN_BindRenderbuffer>( "glBindRenderbuffer" );
		RenderbufferStorage = Load<PFN_RenderbufferStorage>( "glRenderbufferStorage" );
		FramebufferRenderbuffer = Load<PFN_FramebufferRenderbuffer>( "glFramebufferRenderbuffer" );
		DeleteRenderbuffers = Load<PFN_DeleteRenderbuffers>( "glDeleteRenderbuffers" );
		SwapInterval = Load<PFN_SwapInterval>( "wglSwapIntervalEXT" );

		// A working 1.1 entry point means the desktop driver is present.
		return glGetString( GL_VERSION ) != nullptr;
	}

	void SetVSync( bool Enabled )
	{
		if ( SwapInterval )
			SwapInterval( Enabled ? 1 : 0 );
	}

	bool Surface::Create( HWND hWnd )
	{
		Window = hWnd;

		Device = GetDC( hWnd );
		if ( !Device )
			return false;

		PIXELFORMATDESCRIPTOR pfd{};
		pfd.nSize = sizeof( pfd );
		pfd.nVersion = 1;
		pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 32;
		pfd.cDepthBits = 24;
		pfd.cStencilBits = 8;
		pfd.iLayerType = PFD_MAIN_PLANE;

		int Format = ChoosePixelFormat( Device, &pfd );
		if ( !Format || !SetPixelFormat( Device, Format, &pfd ) )
		{
			Destroy( );
			return false;
		}

		// A legacy context is needed before wglCreateContextAttribsARB can be used.
		HGLRC Legacy = wglCreateContext( Device );
		if ( !Legacy || !wglMakeCurrent( Device, Legacy ) )
		{
			Destroy( );
			return false;
		}

		Context = Legacy;
		auto CreateAttribs = reinterpret_cast<PFN_CreateContextAttribs>( GetProc( "wglCreateContextAttribsARB" ) );
		if ( CreateAttribs )
		{
			const int Attribs[ ] = {
				WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
				WGL_CONTEXT_MINOR_VERSION_ARB, 3,
				WGL_CONTEXT_FLAGS_ARB, 0,
				WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
				0
			};
			HGLRC Modern = CreateAttribs( Device, nullptr, Attribs );
			if ( Modern )
			{
				wglMakeCurrent( nullptr, nullptr );
				wglDeleteContext( Legacy );
				Legacy = nullptr;
				if ( wglMakeCurrent( Device, Modern ) )
					Context = Modern;
				else
				{
					wglDeleteContext( Modern );
					Context = nullptr;
				}
			}
		}

		if ( !Context )
		{
			Destroy( );
			return false;
		}

		if ( !Init( ) )
		{
			Destroy( );
			return false;
		}

		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
		return true;
	}

	void Surface::Present( )
	{
		if ( Device )
			SwapBuffers( Device );
	}

	void Surface::Destroy( )
	{
		if ( Context )
		{
			wglMakeCurrent( nullptr, nullptr );
			wglDeleteContext( Context );
			Context = nullptr;
		}
		if ( Window && Device )
			ReleaseDC( Window, Device );
		Device = nullptr;
		Window = nullptr;
	}

	bool CreateTextureFromPNG( const void * Data, size_t Size, Texture & Out )
	{
		if ( !Data || !Size || !GenTextures )
			return false;

		int Width = 0, Height = 0, Channels = 0;
		stbi_uc * Pixels = stbi_load_from_memory(
			static_cast<const stbi_uc *>( Data ), static_cast<int>( Size ),
			&Width, &Height, &Channels, 4 );
		if ( !Pixels || Width <= 0 || Height <= 0 )
			return false;

		GLuint Id = 0;
		GenTextures( 1, &Id );
		if ( !Id )
		{
			stbi_image_free( Pixels );
			return false;
		}

		BindTexture( GL_TEXTURE_2D, Id );
		TexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, Pixels );
		TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		BindTexture( GL_TEXTURE_2D, 0 );

		stbi_image_free( Pixels );
		Out = Texture( Id );
		return true;
	}

	void DestroyTexture( Texture & Texture )
	{
		if ( Texture.Id && DeleteTextures )
			DeleteTextures( 1, &Texture.Id );
		Texture.Id = 0;
	}
}
