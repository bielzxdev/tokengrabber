#pragma once
#include <Includes/Includes.hpp>
#include <Core/SDK/SDK.hpp>

namespace Core
{
	namespace Features
	{

		// Ped em que o aimbot esta puxando AGORA, ou nullptr. Publicado aqui
		// porque o ESP roda noutra thread e precisa saber em quem o aimbot
		// travou para pintar so o esqueleto dele.
		//
		// atomic e nao um ponteiro solto: a thread do aimbot escreve a cada
		// ~1ms enquanto a de render le a cada frame.
		inline std::atomic<CPed*> g_AimTarget{ nullptr };

		class cAimbot
		{
		private:
			void SetAngles(D3DXVECTOR3 WorldPosition, int Smooth);

		public:
			void Start();
		};

		inline cAimbot g_Aimbot;
	}
}
