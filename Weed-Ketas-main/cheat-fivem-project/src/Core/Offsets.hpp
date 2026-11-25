#pragma once

namespace Core {

    class OffsetsClass {
    public:

        //Class Addrs
        uintptr_t m_World, //48 8B 05 ?? ?? ?? ?? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B
            m_ReplayInterFace, //48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? 8A D8 E8 ? ? ? ? 84 DB 75 13 48 8D 0D ? ? ? ? 48 8B D7 E8 ? ? ? ? 84 C0 74 BC 8B 8F
            m_ViewPort, //48 8B 15 ?? ?? ?? ?? 48 8D 2D ?? ?? ?? ?? 48 8B CD
            m_BlipList, //4C 8D 05 ?? ?? ?? ?? 0F B7 C1 / 4C 8D 35 ?? ?? ?? ?? 3B 35 ?? ?? ?? ?? 74 ?? 49 8B 3E
            m_CamGameplayDirector, //4C 8B 35 ?? ?? ?? ?? 33 FF 32 DB
            m_LocalPlayer;

        uintptr_t m_FindTaskByType = 0;
        uintptr_t m_PedIntelligence = 0;

        uintptr_t m_AimTaskShootFlagOffset = 0;
        uint32_t m_AimTaskType = 0;
        uint8_t m_AimTaskIsShootingBit = 0;
        uintptr_t m_InvincibilityFlags = 0;
        uintptr_t m_Godmod = 0; // CPlayerInfo weapon-defense multiplier offset
        uintptr_t m_WaypointData = 0;
        uintptr_t m_NetworkGameInProgress = 0;

        uintptr_t HasEntityDamaged;
        uintptr_t GetShapeTest;
        uintptr_t GetPedLastImpact;

        //Function Addrs
        uintptr_t m_SilentAim, //48 8D 45 ?? F3 0F 10 00 F3 0F 10 48 ?? F3 0F 11 45
            m_InfiniteAmmo0, //41 2B C9 3B C8 0F 4D C8
            m_InfiniteAmmo1, //41 2B D1 E8
            m_HandleBullet,
            m_MagicBulletsPatch, //0F 29 4F ?? 83 8F ?? ?? ?? ?? ?? 48 8B 4F
            m_ArmsKinematics, //E8 ?? ?? ?? ?? 48 83 C3 60 48 FF CF 75 E6 48 8B 5C 24
            m_LegsKinematics, //E8 ?? ?? ?? ?? 48 83 C3 60 48 FF CF 75 DF 48 8B 5C 24 ?? 48 8B 6C 24 ?? 48 8B 74 24
            m_InfiniteCombatRoll, //89 81 ?? ?? ?? ?? 8B 87 ?? ?? ?? ?? F7 D0
            m_SetVariation, //48 8B F9 E8 ? ? ? ? 8B D6
            m_ThermalVision, //48 8d 05 ? ? ? ? 48 c1 e1 ? 48 03 c8 0f 29 4c 24 rip7
            m_SetPedSeat, //48 8B C4 44 89 48 ? 44 89 40 ? 48 89 50 ? 48 89 48 ? 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 ? ? ? ? 48 81 EC ? ? ? ? 83 BA
            m_GiveWeapon, //48 89 5c 24 ?? 48 89 6c 24 ?? 48 89 74 24 ? 57 48 83 ec ?? 41 8b f0 8b fa 48 8b d9 e8
            m_GiveWeaponInHand, //48 8B C4 48 89 58 ? 48 89 68 ? 48 89 70 ? 48 89 78 ? 41 54 41 55 41 56 48 83 EC ? 41 8A F9
            m_FreecanPatch,//f3 0f 11 47 ? f3 0f 10 44 24 ? f3 0f 11 4f ? f3 0f 10 4c 24 ? f3 0f 11 47 ? f3 0f 11 4f ? e8
            m_Vehicle01, // 0x48, 0x83, 0xEC, 0x00, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x49
            m_RequestModel, // 48 89 5C 24 ? 48 89 74 24 ? 55 57 41 56 48 8B EC 48 83 EC ? 83 61
            m_getIdByHash,
            m_RapidFire,
            m_RapidFireBlind,
            m_BreakOffWheel,
            m_SetPedInVehicleFunc,
            m_GameplayCamHolder,// 4C 89 2D ? ? ? ? E8 ? ? ? ? E8
            m_GameplayCamTarget, // 48 83 EC 38 0F 29 74 24 ? 0F 28 F0 0F 2F 35 ? ? ? ? 73
            m_IsEntityVisible, // 48 89 5C 24 ? 57 48 83 EC ? 48 8B 41 ? 48 8B F9 33 DB 8B 08 E8 ? ? ? ? 48 85 C0 74 ? 8A 58 ? 80 E3 ? 48 8B 07 0F B6 CB 48 8B 5C 24 ? 89 08 48 83 C4 ? 5F C3 40 53 48 83 EC ? 48 8B 41 ? 48 8B D9 8B 08 E8
            m_NetSync, //48 8B 1B 44 38 7B 19 0F 85 ? ? ? ? 66 66 0F 1F 84 00 ? ? ? ?
            m_SetPhysicalVelocity,
            m_SetEntityVelocityNative,
            m_GetScriptGuidFromBase,
            m_IsPedInParachuteFreeFallNative,
            m_GetEntitySpeedNative,
            m_TaskLeaveAnyVehicleNative, // TASK_LEAVE_ANY_VEHICLE native (0x504D54DF3F6F2247)
            m_SetRocketBoostFillNative,   // CommandSetRocketBoostFill(int guid, float fill)
            m_SetRocketBoostActiveNative, // CommandSetRocketBoostActive(int guid, bool active)
            m_HasRocketBoost,             // CVehicle::HasRocketBoost(this)
            m_ProcessRocketBoostGate,     // chamada de HasRocketBoost antes de ProcessRocketBoost
            m_SetRocketBoosting,          // CVehicle::SetRocketBoosting(this, bool) — sem GUID, direto no veiculo
            m_RocketBoostFuelOffset,      // disp32 dentro de CommandSetRocketBoostFill (movss [rbx+disp32],xmm0) — offset do fuel atual em CVehicle
            m_RocketBoostKersAllowedOffset, // CTransmission::m_bKERSAllowed dentro de CVehicle
            m_RocketBoostModelFlagOffset, // FLAG_HAS_ROCKET_BOOST dentro de CVehicleModelInfo
            m_RocketBoostModelFlagMask,   // bit da flag 126 (0x40 no byte +0x58B)
            m_RequestControlEventTrigger; // CRequestControlEvent::Trigger(netObject*) — real OneSync-friendly ownership request

        //Offsets
        uintptr_t m_LastVehicle,
            m_Invisible,
            m_IsPedShooting,
            m_Handling,
            m_VehicleEngineHealth,
            m_PlayerInfo,
            m_FragInst,
            m_Armor,
            m_Recoil,
            m_PedFlag,
            m_Spread,
            m_Reload,
            m_WeaponManager,
            m_GetGameplayCamRot, //48 89 5C 24 08 57 48 83 EC 30 8B DA 48 8B F9 E8 ? ? ? ?
            m_GetGameplayCamCoord, //48 89 5C 24 F8 48 8D 64 24 F8 48 83 EC 60
            m_GetFinalRenderedCamRot, //48 89 5C 24 08 57 48 83 EC 30 8B DA 48 8B F9 E8 ? ? ? ? 48 8D 4C 24 20 44 8B C3 48 8B D0
            m_GetFinalRenderedCamCoord, //48 89 5C 24 ? 57 48 83 EC ? 33 DB 48 8B FA 48 89 5A
            m_ClonePedToTarget,
            m_CopyPedVariation,
            m_OffsetPool, //0F B7 CA 83 F9 07 7F 5E
            m_VehicleDoorsLockState,
            m_EntityType,
            m_Speed,
            m_Health,
            m_MaxHealth,
            m_CommandIsVehicleTyreBurst,
            m_VehicleDriver,
            m_GetWheelFromId,
            m_LastVisibleTime,
            m_Stamina,
            m_MaxStamina,
            m_VehicleGravity,
            m_CObject,
            m_NoRagDoll,
            m_CWeapon,
            m_PlayerId,
            m_CitizenNamesModBase,
            m_NetIdToNamesEntry,
            m_SeatBealt,
            m_VehicleState,
            m_VehicleExtras;

        uintptr_t m_ShrinkDataCave = 0;
        bool m_ShrinkPatched = false;

        //Infos
        uint32_t CurrentBuild;
        std::string ServerIp;



    };

    inline OffsetsClass g_Offsets;
}
