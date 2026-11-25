#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

// Antes vinha de curl/curl.h; mantenho o alias para o codigo existente.
using u_char = unsigned char;

#define M_PI 3.14159265358979323846

#include <Includes/ImGui/Files/imgui.h>
#include <Includes/ImGui/Files/imgui_internal.h>
#include <Includes/ImGui/Files/imgui_impl_win32.h>
#include <Includes/ImGui/Files/imgui_impl_opengl3.h>
#include <Includes/OpenGL/GL.hpp>

#include <Includes/ImGui/FontAwesome/FontAwesome.hpp>
#include <Includes/ImGui/FontAwesome/FontAwesomeBrands.hpp>
#include <Includes/ImGui/FontAwesome/RawAwesome6.hpp>

#include <Security/xorstr.hpp>
#include <Security/Api/json.hpp>

#include <Globals.hpp>

#include <Core/Config.hpp>

#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>

#include <D3dx9math.h>

#pragma comment( lib, "ws2_32.lib" )
#pragma comment( lib, "Normaliz.lib" )
#pragma comment( lib, "Crypt32.lib" )
#pragma comment( lib, "Wldap32.lib" )
#pragma comment( lib, "winhttp.lib" )

#pragma warning( disable : 4996 )

using namespace Core;