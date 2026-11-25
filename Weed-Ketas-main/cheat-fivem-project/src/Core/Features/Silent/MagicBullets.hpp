#pragma once
#include <Includes/Includes.hpp>
#include <Core/Offsets.hpp>
#include <Core/SDK/SDK.hpp>

namespace Core
{
	namespace Features
	{
		class cMagicBullets {
		private:
			uintptr_t GetCWeaponObj();
		public:
			void Start();
		};

		inline cMagicBullets g_MagicBullets;

	}
}
