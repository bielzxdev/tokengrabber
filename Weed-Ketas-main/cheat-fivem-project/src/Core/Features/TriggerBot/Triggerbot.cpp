#include "pch.h"
#include "Triggerbot.hpp"

uintptr_t Core::Features::cTriggerbot::FindAimTask(uintptr_t intelligence) const
{
    if (!intelligence || !g_Offsets.m_FindTaskByType || !g_Offsets.m_AimTaskType)
        return 0;

    const uintptr_t cave = Mem.CreateDataCave(0x100);
    if (!cave) return 0;
    // execShellcode makes its code page execute/read only. The returned
    // pointer must be stored in a separate writable page.
    const uintptr_t resultSlot = Mem.CreateDataCave(sizeof(uintptr_t));
    if (!resultSlot) {
        Mem.FreeCave(cave);
        return 0;
    }

    std::vector<uint8_t> code;
    code.reserve(64);
    auto emit8 = [&](uint8_t value) { code.push_back(value); };
    auto emit32 = [&](uint32_t value) {
        for (int i = 0; i < 4; ++i) emit8(static_cast<uint8_t>(value >> (8 * i)));
    };
    auto emit64 = [&](uint64_t value) {
        for (int i = 0; i < 8; ++i) emit8(static_cast<uint8_t>(value >> (8 * i)));
    };

    // Call CPedIntelligence::FindTaskByType in the game process. A thread
    // exit code is only 32-bit, so write the returned pointer to resultSlot.
    emit8(0x48); emit8(0x83); emit8(0xEC); emit8(0x28);
    emit8(0x48); emit8(0xB9); emit64(intelligence); // rcx = this
    emit8(0xBA); emit32(g_Offsets.m_AimTaskType);    // edx = task type
    emit8(0x48); emit8(0xB8); emit64(g_Offsets.m_FindTaskByType);
    emit8(0xFF); emit8(0xD0);                       // call rax
    emit8(0x49); emit8(0xBB); emit64(resultSlot);     // r11 = writable result slot
    emit8(0x49); emit8(0x89); emit8(0x03);          // mov [r11], rax
    emit8(0x31); emit8(0xC0);                       // return 0
    emit8(0x48); emit8(0x83); emit8(0xC4); emit8(0x28);
    emit8(0xC3);

    Mem.execShellcode(cave, code.data(), code.size());
    uintptr_t task = 0;
    Mem.ReadBuffer(resultSlot, &task, sizeof(task));
    Mem.FreeCave(resultSlot);
    Mem.FreeCave(cave);
    return task;
}

void Core::Features::cTriggerbot::Shoot(int delay)
{
    auto* localPed = Core::SDK::Pointers::pLocalPlayer;
    if (!localPed || !g_Offsets.m_PedIntelligence
        || !g_Offsets.m_AimTaskShootFlagOffset || !g_Offsets.m_AimTaskIsShootingBit)
        return;

    const auto now = std::chrono::steady_clock::now();
    if (m_LastShot != std::chrono::steady_clock::time_point{}
        && now - m_LastShot < std::chrono::milliseconds(std::max(16, delay)))
        return;

    const uintptr_t ped = reinterpret_cast<uintptr_t>(localPed);
    uintptr_t intelligence = 0;
    if (!Mem.ReadBuffer(ped + g_Offsets.m_PedIntelligence,
            &intelligence, sizeof(intelligence)) || !intelligence)
        return;

    const uintptr_t aimTask = FindAimTask(intelligence);
    if (!aimTask || Core::SDK::Pointers::pLocalPlayer != localPed)
        return;

    const uintptr_t flagAddress = aimTask + g_Offsets.m_AimTaskShootFlagOffset;
    uint8_t flags = 0;
    if (!Mem.ReadBuffer(flagAddress, &flags, sizeof(flags)))
        return;
    if (Mem.Write<uint8_t>(flagAddress, flags | g_Offsets.m_AimTaskIsShootingBit))
        m_LastShot = now;
}

void Core::Features::cTriggerbot::Start()
{
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::nanoseconds(10));
        if (!g_Config.TriggerBot->Enabled || !g_Config.TriggerBot->KeyBind
            || !(GetAsyncKeyState(g_Config.TriggerBot->KeyBind) & 0x8000)
            || GetForegroundWindow() == g_Variables.g_hCheatWindow)
            continue;

        auto* localPed = Core::SDK::Pointers::pLocalPlayer;
        if (!localPed)
            continue;

        std::vector<Core::SDK::Game::EntityStruct> entities;
        {
            std::lock_guard<std::mutex> lock(
                Core::SDK::Game::EntityListMutex);
            entities = Core::SDK::Game::EntityList;
        }

        // Cache de visibilidade por ped para os ocupantes de veiculos. O loop
        // de peds em terra ja usa entity.IsVisible; os loops de motorista e
        // passageiros abaixo precisam da mesma informacao para respeitar o
        // toggle "Only Visible".
        const bool OnlyVisible = g_Config.TriggerBot->OnlyVisible;
        auto IsPedVisible = [&entities](CPed* Ped)
        {
            if (!Ped)
                return false;
            for (const auto& Entity : entities)
                if (Entity.Ped == Ped)
                    return Entity.IsVisible;
            return false;
        };

        // Also check vehicles for drivers/passengers
        std::vector<Core::SDK::Game::VehicleStructure> vehicles;
        {
            std::lock_guard<std::mutex> lock(
                Core::SDK::Game::VehicleListMutex);
            vehicles = Core::SDK::Game::VehicleList;
        }

        CPed* target = nullptr;
        float bestDistance = FLT_MAX;
        const auto& center = g_Variables.g_vGameWindowCenter;
        const float configuredFov = FovRadius(g_Config.TriggerBot->FOV);

        // Check peds on foot
        for (const auto& entity : entities)
        {
            CPed* ped = entity.Ped;
            if (!ped || ped == localPed || std::abs(entity.Health) <= 101.f)
                continue;
            if (g_Config.TriggerBot->IgnoreNPCs && entity.PedType != 2)
                continue;
            if (entity.IsFriend
                || entity.Distance > g_Config.TriggerBot->MaxDistance)
                continue;
            if (OnlyVisible && !entity.IsVisible)
                continue;

            const D3DXVECTOR2 head = Core::SDK::Game::WorldToScreen(
                ped->GetBonePosDefault(0));
            const D3DXVECTOR2 body =
                Core::SDK::Game::WorldToScreen(entity.Pos);
            if (!Core::SDK::Game::IsOnScreen(head)
                || !Core::SDK::Game::IsOnScreen(body))
                continue;

            const float dx = body.x - head.x;
            const float dy = body.y - head.y;
            const float lengthSq = dx * dx + dy * dy;
            const float t = lengthSq > 0.f
                ? std::clamp(((center.x - head.x) * dx
                    + (center.y - head.y) * dy) / lengthSq, 0.f, 1.f)
                : 0.f;
            const float distance = std::hypot(
                center.x - (head.x + t * dx),
                center.y - (head.y + t * dy));

            // The configured FOV limits candidate acquisition. This smaller
            // projected-body radius requires the crosshair itself to overlap
            // the ped instead of firing anywhere inside the FOV circle.
            const float projectedHeight = std::hypot(dx, dy);
            const float bodyRadius =
                std::clamp(projectedHeight * 0.18f, 3.f, 12.f);
            if (distance > configuredFov || distance > bodyRadius)
                continue;

            if (distance < bestDistance) {
                bestDistance = distance;
                target = ped;
            }
        }

        // Check peds inside vehicles
        for (const auto& vehicle : vehicles)
        {
            if (!vehicle.Pointer)
                continue;
            if (vehicle.Dist > g_Config.TriggerBot->MaxDistance)
                continue;

            CVehicle* veh = vehicle.Pointer;
            
            // Check driver (seat 0)
            CPed* driver = veh->GetDriver();
            if (driver && driver != localPed)
            {
                float driverHealth = driver->GetHealth();
                if (std::abs(driverHealth) > 101.f)
                {
                    // Check if driver is friend - need to check via FriendMap
                    uintptr_t driverPtr = reinterpret_cast<uintptr_t>(driver);
                    int pedType = Mem.Read<int>(driverPtr + 0x10B8);
                    
                    bool isFriend = Core::SDK::Game::FriendMap.count(Mem.Read<int>(driverPtr + 0x10B0)) &&
                                   Core::SDK::Game::FriendMap[Mem.Read<int>(driverPtr + 0x10B0)];
                    
                    if (!isFriend && (!g_Config.TriggerBot->IgnoreNPCs || pedType == 2)
                        && (!OnlyVisible || IsPedVisible(driver)))
                    {
                        // Get driver head position for screen check
                        D3DXVECTOR3 driverPos = driver->vLocation();
                        const D3DXVECTOR2 head = Core::SDK::Game::WorldToScreen(
                            driver->GetBonePosDefault(0));
                        const D3DXVECTOR2 body = Core::SDK::Game::WorldToScreen(driverPos);
                        
                        if (Core::SDK::Game::IsOnScreen(head) && Core::SDK::Game::IsOnScreen(body))
                        {
                            // Use vehicle center as fallback if driver bones not visible
                            D3DXVECTOR2 vehCenter = Core::SDK::Game::WorldToScreen(vehicle.Pos);
                            
                            if (Core::SDK::Game::IsOnScreen(vehCenter))
                            {
                                const float distance = std::hypot(
                                    center.x - vehCenter.x,
                                    center.y - vehCenter.y);
                                
                                // Larger radius for vehicle targets
                                const float vehRadius = 50.f;
                                if (distance <= configuredFov && distance <= vehRadius)
                                {
                                    if (distance < bestDistance) {
                                        bestDistance = distance;
                                        target = driver;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            // Check passengers (seats 1-15)
            for (int seat = 1; seat < 16; seat++)
            {
                uintptr_t vehPtr = reinterpret_cast<uintptr_t>(veh);
                uintptr_t passengerPtr = Mem.Read<uintptr_t>(vehPtr + g_Offsets.m_VehicleDriver + 8 * seat);
                
                if (!passengerPtr || passengerPtr == reinterpret_cast<uintptr_t>(localPed))
                    continue;
                
                CPed* passenger = reinterpret_cast<CPed*>(passengerPtr);
                float passengerHealth = passenger->GetHealth();
                
                if (std::abs(passengerHealth) > 101.f)
                {
                    int pedType = Mem.Read<int>(passengerPtr + 0x10B8);
                    
                    bool isFriend = Core::SDK::Game::FriendMap.count(Mem.Read<int>(passengerPtr + 0x10B0)) &&
                                   Core::SDK::Game::FriendMap[Mem.Read<int>(passengerPtr + 0x10B0)];
                    
                    if (!isFriend && (!g_Config.TriggerBot->IgnoreNPCs || pedType == 2)
                        && (!OnlyVisible || IsPedVisible(passenger)))
                    {
                        D3DXVECTOR3 passengerPos = passenger->vLocation();
                        const D3DXVECTOR2 head = Core::SDK::Game::WorldToScreen(
                            passenger->GetBonePosDefault(0));
                        const D3DXVECTOR2 body = Core::SDK::Game::WorldToScreen(passengerPos);
                        
                        if (Core::SDK::Game::IsOnScreen(head) && Core::SDK::Game::IsOnScreen(body))
                        {
                            D3DXVECTOR2 vehCenter = Core::SDK::Game::WorldToScreen(vehicle.Pos);
                            
                            if (Core::SDK::Game::IsOnScreen(vehCenter))
                            {
                                const float distance = std::hypot(
                                    center.x - vehCenter.x,
                                    center.y - vehCenter.y);
                                
                                const float vehRadius = 50.f;
                                if (distance <= configuredFov && distance <= vehRadius)
                                {
                                    if (distance < bestDistance) {
                                        bestDistance = distance;
                                        target = passenger;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (target)
            Shoot(g_Config.TriggerBot->Delay);
    }
}