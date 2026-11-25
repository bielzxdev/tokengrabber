
#include "pch.h"
#include "MagicBullets.hpp"

uintptr_t Core::Features::cMagicBullets::GetCWeaponObj()
{
	if (!Core::SDK::Pointers::pLocalPlayer)
		return 0;

	CWeaponManager* WeaponMgr = Core::SDK::Pointers::pLocalPlayer->GetWeaponManager();
	if (!WeaponMgr)
		return 0;

	auto CObject = Mem.Read<uintptr_t>((uintptr_t)WeaponMgr + g_Offsets.m_CObject);
	if (!CObject)
		return 0;

	return Mem.Read<uintptr_t>(CObject + g_Offsets.m_CWeapon);
}

void Core::Features::cMagicBullets::Start()
{
	// MagicBullets is now handled by the SilentAim hook (ApplySilent).
	// This thread is kept alive but does nothing.
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}
