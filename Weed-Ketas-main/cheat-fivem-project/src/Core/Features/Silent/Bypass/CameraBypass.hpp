#pragma once
#include <Windows.h>
#include <chrono>
#include <cstdint>
#include <vector>
#include <d3dx9.h>
#include <mutex>

namespace Core::Features
{
	struct alignas(16) CameraSpoof
	{
		D3DXVECTOR3 position;        // +0x00  cam coord forjada
		float _pad1;                 // +0x0C
		D3DXVECTOR3 rotation;        // +0x10  cam rot forjada
		float _pad2;                 // +0x1C
	};

	class CameraBypass
	{
	public:
		CameraBypass() = default;
		~CameraBypass() = default;

		bool Initialize();
		// targetPos: ponto no mundo que a bala vai atingir (cabeça do alvo).
		// playerPos: posição do próprio ped (origem lógica do tiro).
		// cameraPos: posição REAL da câmera do jogo nesse frame — lida do
		// CamFollowPedCamera para que GetGameplayCamCoord forjada bata com
		// a câmera que outras natives/anticheats correlacionam.
		// localPlayerGuid: nao mais utilizado (hook de impact removido);
		// mantido no prototype pra evitar quebrar chamadores.
		//
		// Camera forjada aponta exatamente para targetPos.
		void HookCamera(const D3DXVECTOR3& targetPos,
			const D3DXVECTOR3& playerPos,
			const D3DXVECTOR3& cameraPos,
			uint32_t localPlayerGuid);
		void UnhookCamera();
		void Shutdown(bool releaseMemory = true);

		// Quantos hooks realmente entraram (diagnostico p/ a GUI)
		int  ActiveHooks() const;

	private:
		// HookJMP escreve "FF 25 00000000" + 8 bytes de endereco absoluto
		static constexpr size_t kPatchSize = 14;

		// Hook de GetPedLastWeaponImpactCoord foi removido: o usuario
		// aplica patch nessa funcao externamente.
		// HookFinalCoord tambem removido: a assinatura de
		// GetFinalRenderedCamCoord e identica a de GetPedLastWeaponImpactCoord
		// (mesmo prologue "48 89 5C 24 ? 57 48 83 EC ? 33 DB 48 8B FA 48 89 5A"),
		// entao o scanner cai na funcao errada e o cave escreve 12 bytes
		// em [RCX] que e um int pequeno (ped guid) -> crash insta.
		enum HookId
		{
			HookGameplayCoord = 0,
			HookGameplayRot,
			HookFinalRot,
			HookCount
		};

		struct Hook
		{
			uintptr_t addr = 0;
			uintptr_t cave = 0;
			std::vector<uint8_t> original;
			bool prepared = false;
			bool active = false;
		};

		bool PrepareHook(HookId id, uintptr_t addr, uintptr_t vec3Addr);
		bool InstallJmp(Hook& hook);
		void RemoveJmp(Hook& hook);

		// true se `addr` ja pertence a outro hook desta lista
		bool AlreadyTaken(uintptr_t addr, HookId upTo) const;
		bool IsCurrentAttachment() const;

		Hook m_hooks[HookCount];
		uintptr_t m_gameDataAddr = 0;
		bool m_initialized = false;
		bool m_hooked = false;
		DWORD m_boundPid = 0;
		uintptr_t m_boundModuleBase = 0;
		int m_initializeFailures = 0;
		std::chrono::steady_clock::time_point m_nextInitializeTry{};
		mutable std::recursive_mutex m_stateMutex;
	};

	extern CameraBypass& g_CameraBypass;
}
