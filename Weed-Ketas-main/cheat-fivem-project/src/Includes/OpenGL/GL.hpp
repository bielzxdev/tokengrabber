#pragma once

// Minimal OpenGL layer for the overlay. Everything is loaded at runtime with
// wglGetProcAddress, so the only link dependency is opengl32.lib from the
// Windows SDK. No GLAD/GLEW install and no DirectX SDK are needed.

#include <Windows.h>
#include <gl/GL.h>
#include <cstddef>
#include <cstdint>

#pragma comment( lib, "opengl32.lib" )

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER             0x8894
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW              0x88E4
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER            0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER          0x8B30
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS           0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS              0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH          0x8B84
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER              0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0        0x8CE0
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT         0x8D00
#endif
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER             0x8D41
#endif
#ifndef GL_DEPTH_COMPONENT16
#define GL_DEPTH_COMPONENT16        0x81A5
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE     0x8CD5
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE            0x812F
#endif
#ifndef GL_UNPACK_ALIGNMENT
#define GL_UNPACK_ALIGNMENT         0x0CF5
#endif
#ifndef GL_LEQUAL
#define GL_LEQUAL                   0x0203
#endif

#define WGL_CONTEXT_MAJOR_VERSION_ARB       0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB       0x2092
#define WGL_CONTEXT_FLAGS_ARB               0x2093
#define WGL_CONTEXT_PROFILE_MASK_ARB        0x9126
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002

namespace GLX
{
	using PFN_GenVertexArrays = void( APIENTRY * )( GLsizei, GLuint * );
	using PFN_BindVertexArray = void( APIENTRY * )( GLuint );
	using PFN_DeleteVertexArrays = void( APIENTRY * )( GLsizei, const GLuint * );
	using PFN_GenBuffers = void( APIENTRY * )( GLsizei, GLuint * );
	using PFN_BindBuffer = void( APIENTRY * )( GLenum, GLuint );
	using PFN_BufferData = void( APIENTRY * )( GLenum, std::ptrdiff_t, const void *, GLenum );
	using PFN_DeleteBuffers = void( APIENTRY * )( GLsizei, const GLuint * );
	using PFN_VertexAttribPointer = void( APIENTRY * )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * );
	using PFN_EnableVertexAttribArray = void( APIENTRY * )( GLuint );
	using PFN_DisableVertexAttribArray = void( APIENTRY * )( GLuint );
	using PFN_CreateShader = GLuint( APIENTRY * )( GLenum );
	using PFN_ShaderSource = void( APIENTRY * )( GLuint, GLsizei, const char * const *, const GLint * );
	using PFN_CompileShader = void( APIENTRY * )( GLuint );
	using PFN_GetShaderiv = void( APIENTRY * )( GLuint, GLenum, GLint * );
	using PFN_GetShaderInfoLog = void( APIENTRY * )( GLuint, GLsizei, GLsizei *, char * );
	using PFN_DeleteShader = void( APIENTRY * )( GLuint );
	using PFN_CreateProgram = GLuint( APIENTRY * )( void );
	using PFN_AttachShader = void( APIENTRY * )( GLuint, GLuint );
	using PFN_LinkProgram = void( APIENTRY * )( GLuint );
	using PFN_GetProgramiv = void( APIENTRY * )( GLuint, GLenum, GLint * );
	using PFN_GetProgramInfoLog = void( APIENTRY * )( GLuint, GLsizei, GLsizei *, char * );
	using PFN_UseProgram = void( APIENTRY * )( GLuint );
	using PFN_DeleteProgram = void( APIENTRY * )( GLuint );
	using PFN_GetUniformLocation = GLint( APIENTRY * )( GLuint, const char * );
	using PFN_UniformMatrix4fv = void( APIENTRY * )( GLint, GLsizei, GLboolean, const GLfloat * );
	using PFN_GenTextures = void( APIENTRY * )( GLsizei, GLuint * );
	using PFN_BindTexture = void( APIENTRY * )( GLenum, GLuint );
	using PFN_TexImage2D = void( APIENTRY * )( GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void * );
	using PFN_TexParameteri = void( APIENTRY * )( GLenum, GLenum, GLint );
	using PFN_DeleteTextures = void( APIENTRY * )( GLsizei, const GLuint * );
	using PFN_ActiveTexture = void( APIENTRY * )( GLenum );
	using PFN_GenFramebuffers = void( APIENTRY * )( GLsizei, GLuint * );
	using PFN_BindFramebuffer = void( APIENTRY * )( GLenum, GLuint );
	using PFN_FramebufferTexture2D = void( APIENTRY * )( GLenum, GLenum, GLenum, GLuint, GLint );
	using PFN_CheckFramebufferStatus = GLenum( APIENTRY * )( GLenum );
	using PFN_DeleteFramebuffers = void( APIENTRY * )( GLsizei, const GLuint * );
	using PFN_GenRenderbuffers = void( APIENTRY * )( GLsizei, GLuint * );
	using PFN_BindRenderbuffer = void( APIENTRY * )( GLenum, GLuint );
	using PFN_RenderbufferStorage = void( APIENTRY * )( GLenum, GLenum, GLsizei, GLsizei );
	using PFN_FramebufferRenderbuffer = void( APIENTRY * )( GLenum, GLenum, GLenum, GLuint );
	using PFN_DeleteRenderbuffers = void( APIENTRY * )( GLsizei, const GLuint * );
	using PFN_CreateContextAttribs = HGLRC( APIENTRY * )( HDC, HGLRC, const int * );
	using PFN_SwapInterval = int( APIENTRY * )( int );

	extern PFN_GenVertexArrays GenVertexArrays;
	extern PFN_BindVertexArray BindVertexArray;
	extern PFN_DeleteVertexArrays DeleteVertexArrays;
	extern PFN_GenBuffers GenBuffers;
	extern PFN_BindBuffer BindBuffer;
	extern PFN_BufferData BufferData;
	extern PFN_DeleteBuffers DeleteBuffers;
	extern PFN_VertexAttribPointer VertexAttribPointer;
	extern PFN_EnableVertexAttribArray EnableVertexAttribArray;
	extern PFN_DisableVertexAttribArray DisableVertexAttribArray;
	extern PFN_CreateShader CreateShader;
	extern PFN_ShaderSource ShaderSource;
	extern PFN_CompileShader CompileShader;
	extern PFN_GetShaderiv GetShaderiv;
	extern PFN_GetShaderInfoLog GetShaderInfoLog;
	extern PFN_DeleteShader DeleteShader;
	extern PFN_CreateProgram CreateProgram;
	extern PFN_AttachShader AttachShader;
	extern PFN_LinkProgram LinkProgram;
	extern PFN_GetProgramiv GetProgramiv;
	extern PFN_GetProgramInfoLog GetProgramInfoLog;
	extern PFN_UseProgram UseProgram;
	extern PFN_DeleteProgram DeleteProgram;
	extern PFN_GetUniformLocation GetUniformLocation;
	extern PFN_UniformMatrix4fv UniformMatrix4fv;
	extern PFN_GenTextures GenTextures;
	extern PFN_BindTexture BindTexture;
	extern PFN_TexImage2D TexImage2D;
	extern PFN_TexParameteri TexParameteri;
	extern PFN_DeleteTextures DeleteTextures;
	extern PFN_ActiveTexture ActiveTexture;
	extern PFN_GenFramebuffers GenFramebuffers;
	extern PFN_BindFramebuffer BindFramebuffer;
	extern PFN_FramebufferTexture2D FramebufferTexture2D;
	extern PFN_CheckFramebufferStatus CheckFramebufferStatus;
	extern PFN_DeleteFramebuffers DeleteFramebuffers;
	extern PFN_GenRenderbuffers GenRenderbuffers;
	extern PFN_BindRenderbuffer BindRenderbuffer;
	extern PFN_RenderbufferStorage RenderbufferStorage;
	extern PFN_FramebufferRenderbuffer FramebufferRenderbuffer;
	extern PFN_DeleteRenderbuffers DeleteRenderbuffers;
	extern PFN_SwapInterval SwapInterval;

	bool Init( );
	void * GetProc( const char * Name );
	void SetVSync( bool Enabled );

	struct Surface
	{
		HWND Window = nullptr;
		HDC Device = nullptr;
		HGLRC Context = nullptr;

		bool Create( HWND hWnd );
		void Present( );
		void Destroy( );
	};

	// ImTextureID is void* in this ImGui version. A GLuint is an integer, so it
	// is carried through intptr_t and hidden behind this tiny wrapper.
	struct Texture
	{
		GLuint Id = 0;

		Texture( ) = default;
		Texture( GLuint Id ) : Id( Id ) { }
		operator void *( ) const { return reinterpret_cast<void *>( static_cast<intptr_t>( Id ) ); }
		operator GLuint( ) const { return Id; }
		GLuint Get( ) const { return Id; }
		explicit operator bool( ) const { return Id != 0; }
	};

	bool CreateTextureFromPNG( const void * Data, size_t Size, Texture & Out );
	void DestroyTexture( Texture & Texture );
}

using GLTexture = GLX::Texture;
