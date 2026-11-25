#pragma once
#include <Includes/Includes.hpp>
#include <Core/Offsets.hpp>
#include <Core/SDK/SDK.hpp>



namespace Core
{
	namespace Features
	{
		class cTriggerbot
		{
		private:
			std::chrono::steady_clock::time_point m_LastShot{};
			uintptr_t FindAimTask(uintptr_t intelligence) const;
			void Shoot(int delay);
		public:
			void Start();
		};

		inline cTriggerbot g_Triggerbot;

	}
}