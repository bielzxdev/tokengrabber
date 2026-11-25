#pragma once

// ============================================================
//  Precompiled Header
//  Inclui todos os headers pesados e est�veis do projeto.
//  Compilado uma vez e reutilizado por todos os .cpp.
// ============================================================

#define IMGUI_DEFINE_MATH_OPERATORS

// Antes vinha de curl/curl.h; mantenho o alias para o codigo existente.
using u_char = unsigned char;

#define M_PI 3.14159265358979323846

#ifndef NOMINMAX
#define NOMINMAX
#endif

// --- ImGui ---
#include <Includes/ImGui/Files/imgui.h>
#include <Includes/ImGui/Files/imgui_internal.h>
#include <Includes/ImGui/Files/imgui_impl_win32.h>
#include <Includes/ImGui/Files/imgui_impl_opengl3.h>
#include <Includes/OpenGL/GL.hpp>

// --- FontAwesome (arrays de fonte grandes) ---
#include <Includes/ImGui/FontAwesome/FontAwesome.hpp>
#include <Includes/ImGui/FontAwesome/FontAwesomeBrands.hpp>
#include <Includes/ImGui/FontAwesome/RawAwesome6.hpp>

// --- Security / API ---
#include <Security/xorstr.hpp>
#include <Security/Api/json.hpp>

// --- Windows / DirectX ---
#include <Windows.h>
#include <TlHelp32.h>
// D3DX matematica e header-only, fornecido pelo pacote NuGet vendorado.
// Nao ha mais dependencia do DirectX SDK June 2010 no renderizador.
#include <D3dx9math.h>

// --- STL ---
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <string>
#include <vector>
#include <atomic>
#include <functional>

// --- Linker ---
#pragma comment( lib, "ws2_32.lib" )
#pragma comment( lib, "Normaliz.lib" )
#pragma comment( lib, "Crypt32.lib" )
#pragma comment( lib, "Wldap32.lib" )
#pragma comment( lib, "winhttp.lib" )

#pragma warning( disable : 4996 )
