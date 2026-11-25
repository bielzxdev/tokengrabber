
#include "../../../../pch.h"
#include "pch.h"
#include "CameraBypass.hpp"
#include <Core/SDK/Memory.hpp>
#include <Core/Offsets.hpp>
#include <cmath>
#include <cstring>

namespace Core::Features
{
	static CameraBypass g_CameraBypass_instance;
	CameraBypass& g_CameraBypass = g_CameraBypass_instance;

	namespace
	{
		constexpr float kRad2Deg = 57.29577951308232f;

		// As natives Get...CamCoord/Rot em FiveM b3258 devolvem o resultado em
		// layout scrVector (padding de 4 bytes entre componentes):
		//     scrVector* Get...(scrVector* out /*RCX*/, int rotOrder /*EDX, so nas Rot*/)
		//     [rcx+0x00] = X  (float)
		//     [rcx+0x04] = padding (nao usado)
		//     [rcx+0x08] = Y  (float)
		//     [rcx+0x0C] = padding
		//     [rcx+0x10] = Z  (float)
		// (Antes o shellcode escrevia em layout Vector3 contiguo [+0,+4,+8],
		//  o que crashava o jogo porque o script engine le em [+0,+8,+0x10].)
		// O contrato obriga tambem a devolver RCX em RAX.
		//
		// mov  rax, vec3Addr        ; nosso buffer em layout Vector3 contiguo
		// mov  r8d, [rax]      → mov [rcx+0x00], r8d    ; X
		// mov  r8d, [rax+4]    → mov [rcx+0x08], r8d    ; Y
		// mov  r8d, [rax+8]    → mov [rcx+0x10], r8d    ; Z
		// mov  rax, rcx
		// ret
		std::vector<uint8_t> BuildShellcode(uintptr_t vec3Addr)
		{
			uint8_t sc[] = {
				0x48, 0xB8, 0,0,0,0,0,0,0,0,          // mov rax, imm64
				0x44, 0x8B, 0x00,                      // mov r8d, [rax]
				0x44, 0x89, 0x01,                      // mov [rcx], r8d       (X at +0)
				0x44, 0x8B, 0x40, 0x04,                // mov r8d, [rax+4]
				0x44, 0x89, 0x41, 0x08,                // mov [rcx+8], r8d     (Y at +8)
				0x44, 0x8B, 0x40, 0x08,                // mov r8d, [rax+8]
				0x44, 0x89, 0x41, 0x10,                // mov [rcx+0x10], r8d  (Z at +0x10)
				0x48, 0x8B, 0xC1,                      // mov rax, rcx
				0xC3                                   // ret
			};
			std::memcpy(sc + 2, &vec3Addr, sizeof(uintptr_t));
			return std::vector<uint8_t>(sc, sc + sizeof(sc));
		}

		// Prologues aceitos:
		//  - "48 89 5C 24 ??"       (mov [rsp+X],rbx)   -- GetGameplayCamRot, GetFinalRenderedCamRot
		//  - "40 53"                (push rbx)          -- GetGameplayCamCoord, GetFinalRenderedCamCoord
		// Ambas as ABIs recebem RCX = scrVector* out, e nosso shellcode so
		// escreve 12 bytes de dados em layout scrVector + retorna; nao
		// precisa reproduzir o prologue stolen.
		bool LooksLikePrologue(const std::vector<uint8_t>& b)
		{
			if (b.size() < 2) return false;
			if (b[0] == 0x40 && b[1] == 0x53) return true;
			return b.size() >= 5
				&& b[0] == 0x48 && b[1] == 0x89 && b[2] == 0x5C && b[3] == 0x24;
		}

	}

	bool CameraBypass::AlreadyTaken(uintptr_t addr, HookId upTo) const
	{
		for (int i = 0; i < upTo; i++)
			if (m_hooks[i].prepared && m_hooks[i].addr == addr)
				return true;
		return false;
	}

	bool CameraBypass::IsCurrentAttachment() const
	{
		return m_boundPid && m_boundModuleBase && Mem.ProcHandle &&
			Mem.ProcId == m_boundPid && Mem.ModBase == m_boundModuleBase &&
			GetProcessId(Mem.ProcHandle) == m_boundPid;
	}

	bool CameraBypass::Initialize()
	{
		std::lock_guard<std::recursive_mutex> lock(m_stateMutex);

		if (m_initialized) return true;
		if (!Mem.ProcHandle || !Mem.ProcId || !Mem.ModBase)
			return false;

		const auto Now = std::chrono::steady_clock::now();

		// HookCamera chama isto a cada tick do aimbot (~1ms) enquanto falhar, e
		// aqui dentro tem alocacao de code cave e varredura de assinatura. Sem
		// freio, uma falha vira milhares de tentativas por segundo. O estado e
		// por attachment: um FiveM novo nunca herda o cooldown do anterior.
		if (Now < m_nextInitializeTry)
			return false;

		m_nextInitializeTry = Now +
			std::chrono::milliseconds(m_initializeFailures < 5 ? 500 : 30000);
		++m_initializeFailures;

		// Buffer com os valores spoofados, dentro do processo do JOGO.
		m_gameDataAddr = Mem.CreateDataCave(sizeof(CameraSpoof));
		if (!m_gameDataAddr) {
			return false;
		}

		CameraSpoof zeroed = {};
		if (!Mem.Write<CameraSpoof>(m_gameDataAddr, zeroed))
		{
			Mem.FreeCave(m_gameDataAddr);
			m_gameDataAddr = 0;
			return false;
		}

		const uintptr_t PosAddr = m_gameDataAddr + offsetof(CameraSpoof, position);
		const uintptr_t RotAddr = m_gameDataAddr + offsetof(CameraSpoof, rotation);

		struct { HookId Id; uintptr_t Addr; uintptr_t Vec; } Targets[] =
		{
			{ HookGameplayCoord, g_Offsets.m_GetGameplayCamCoord,      PosAddr },
			{ HookGameplayRot,   g_Offsets.m_GetGameplayCamRot,        RotAddr },
			{ HookFinalRot,      g_Offsets.m_GetFinalRenderedCamRot,   RotAddr },
		};

		for (const auto& T : Targets)
			PrepareHook(T.Id, T.Addr, T.Vec);

		// Hook de GetPedLastWeaponImpactCoord foi removido: o usuario
		// patcha essa funcao externamente, entao nao precisamos spoofar
		// a resposta dela aqui.

		if (ActiveHooks() == 0)
		{
			Mem.FreeCave(m_gameDataAddr);
			m_gameDataAddr = 0;
			return false;
		}

		m_boundPid = Mem.ProcId;
		m_boundModuleBase = Mem.ModBase;
		m_initialized = true;
		m_initializeFailures = 0;
		m_nextInitializeTry = {};
		return true;
	}

	// Aloca o cave e escreve o shellcode, mas NAO instala o JMP ainda.
	// Devolve false (sem derrubar os outros) quando o alvo nao e confiavel.
	bool CameraBypass::PrepareHook(HookId id, uintptr_t addr, uintptr_t vec3Addr)
	{
		Hook& hook = m_hooks[id];
		if (hook.prepared) return true;
		if (!addr) {
			return false;
		}

		// A assinatura de GetGameplayCamRot e prefixo da de
		//     GetFinalRenderedCamRot, entao FindSignature pode devolver o mesmo
		//     endereco para as duas. Hookar duas vezes faria o segundo
		//     PrepareHook ler o JMP ja instalado como "bytes originais" e
		//     corromper a funcao para sempre.
		if (AlreadyTaken(addr, id)) {
			return false;
		}

		std::vector<uint8_t> Original = Mem.ReadBytes(addr, kPatchSize);
		if (Original.size() < kPatchSize) {
			return false;
		}

		// (3) Se os bytes nao parecem um prologo, o offset esta errado.
		if (!LooksLikePrologue(Original)) {
			return false;
		}

		// HookJMP usa "FF 25 00000000" + endereco absoluto de 8 bytes,
		// que e um salto RIP-relative indireto: nao precisa de cave perto.
		hook.cave = Mem.CreateCodeCave(0x100);
		if (!hook.cave) {
			return false;
		}

		if (!Mem.WriteBytes(hook.cave, BuildShellcode(vec3Addr)))
		{
			Mem.FreeCave(hook.cave);
			hook.cave = 0;
			return false;
		}
		if (!Mem.SealCave(hook.cave, 0x100))
		{
			Mem.FreeCave(hook.cave);
			hook.cave = 0;
			return false;
		}
		FlushInstructionCache(Mem.ProcHandle,
			reinterpret_cast<const void*>(hook.cave), 0x100);

		hook.addr = addr;
		hook.original = std::move(Original);
		hook.prepared = true;
		return true;
	}

	bool CameraBypass::InstallJmp(Hook& hook)
	{
		if (!hook.prepared || hook.active) return hook.active;

		const std::vector<uint8_t> patch = {
			0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
			static_cast<uint8_t>(hook.cave),
			static_cast<uint8_t>(hook.cave >> 8),
			static_cast<uint8_t>(hook.cave >> 16),
			static_cast<uint8_t>(hook.cave >> 24),
			static_cast<uint8_t>(hook.cave >> 32),
			static_cast<uint8_t>(hook.cave >> 40),
			static_cast<uint8_t>(hook.cave >> 48),
			static_cast<uint8_t>(hook.cave >> 56)
		};

		hook.active = Mem.PatchFunc(hook.addr, patch) != FALSE;
		return hook.active;
	}

	void CameraBypass::RemoveJmp(Hook& hook)
	{
		if (!hook.active) return;
		if (Mem.PatchFunc(hook.addr, hook.original))
			hook.active = false;
	}

	int CameraBypass::ActiveHooks() const
	{
		int n = 0;
		for (const auto& h : m_hooks)
			if (h.prepared) n++;
		return n;
	}

	void CameraBypass::HookCamera(const D3DXVECTOR3& targetPos,
		const D3DXVECTOR3& playerPos,
		const D3DXVECTOR3& cameraPos,
		uint32_t localPlayerGuid)
	{
		std::lock_guard<std::recursive_mutex> lock(m_stateMutex);

		if (m_initialized && !IsCurrentAttachment())
			Shutdown(false);

		if (!m_initialized && !Initialize())
		{
			return;
		}
		if (!IsCurrentAttachment() || !m_gameDataAddr)
			return;

		// A posição da câmera forjada tem que casar com a câmera REAL do
		// jogo, senão o anticheat vê GetGameplayCamCoord dentro do boneco
		// enquanto GetEntityCoords mostra o player no lugar certo e marca
		// como divergência. Fallback para playerPos só se a câmera real
		// não foi lida (ponteiro nulo).
		const bool HasRealCamera = std::isfinite(cameraPos.x) &&
			std::isfinite(cameraPos.y) &&
			std::isfinite(cameraPos.z) &&
			!(cameraPos.x == 0.f && cameraPos.y == 0.f && cameraPos.z == 0.f);

		const D3DXVECTOR3 CamOrigin = HasRealCamera ? cameraPos : playerPos;

		// Camera forjada aponta direto pro targetPos (hitbox exato).
		D3DXVECTOR3 direction = targetPos - CamOrigin;
		float length = D3DXVec3Length(&direction);

		if (length < 0.0001f)
			direction = D3DXVECTOR3(0.f, 0.f, 0.f);
		else
			direction /= length;

		// GET_GAMEPLAY_CAM_ROT devolve (pitch, roll, yaw) em graus.
		CameraSpoof data = {};
		data.position = CamOrigin;
		data.rotation.x = asinf(direction.z) * kRad2Deg;               // pitch
		data.rotation.y = 0.f;                                          // roll
		data.rotation.z = atan2f(-direction.x, direction.y) * kRad2Deg; // yaw
		(void)playerPos;
		(void)localPlayerGuid;

		// Valores primeiro, JMP depois: assim nenhuma das funcoes chega a
		// rodar com o buffer ainda zerado.
		Mem.Write<CameraSpoof>(m_gameDataAddr, data);

		if (!m_hooked)
		{
			bool installed = false;
			for (auto& h : m_hooks)
				installed = InstallJmp(h) || installed;
			m_hooked = installed;
		}
	}

	void CameraBypass::UnhookCamera()
	{
		std::lock_guard<std::recursive_mutex> lock(m_stateMutex);

		if (!m_hooked) return;
		if (!IsCurrentAttachment())
		{
			for (auto& h : m_hooks)
				h.active = false;
			m_hooked = false;
			return;
		}

		for (auto& h : m_hooks)
			RemoveJmp(h);

		// Os caves NAO sao liberados aqui de proposito: o jogo pode estar
		// executando dentro de um deles neste exato instante, e um
		// VirtualFreeEx(MEM_RELEASE) agora derruba o processo.
		m_hooked = false;
	}

	void CameraBypass::Shutdown(bool releaseMemory)
	{
		std::lock_guard<std::recursive_mutex> lock(m_stateMutex);

		UnhookCamera();
		const bool canTouchRemote = IsCurrentAttachment();

		for (auto& h : m_hooks)
		{
			if (releaseMemory && canTouchRemote && h.cave)
				Mem.FreeCave(h.cave);
			h.cave = 0;
			h.prepared = false;
			h.active = false;
			h.addr = 0;
			h.original.clear();
		}

		if (releaseMemory && canTouchRemote && m_gameDataAddr)
			Mem.FreeCave(m_gameDataAddr);
		m_gameDataAddr = 0;
		m_initialized = false;
		m_hooked = false;
		m_boundPid = 0;
		m_boundModuleBase = 0;
		m_initializeFailures = 0;
		m_nextInitializeTry = {};
	}
}
