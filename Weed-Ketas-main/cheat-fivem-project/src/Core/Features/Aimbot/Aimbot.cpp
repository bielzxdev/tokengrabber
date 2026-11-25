#include "pch.h"
#include "Aimbot.hpp"
#include "Prediction.hpp"
#include "AimEngine.hpp"
#include <Core/Features/VehicleWheelVisibility.hpp>

// ============================================================================
// A escrita na camera fica isolada aqui: e a unica fronteira do motor com o
// jogo. Tudo antes disso e matematica pura sobre structs.
// ============================================================================
namespace
{
    using namespace Core::Features;

    // Cada loop precisa do proprio estado cinetico. Quando ambos usavam
    // g_Engine, o loop de ped chamava Release() a cada 1 ms e apagava a
    // velocidade do modo de pneus, produzindo somente pequenos flicks.
    AimEngine g_PedEngine;
    AimEngine g_VehicleEngine;

    // Le a orientacao atual como (yaw, pitch).
    bool ReadView(CamFollowPedCamera* Cam, AimMath::Angles& Out)
    {
        if (!Cam) return false;
        Out = AimMath::FromDirection(Cam->GetViewAngles());
        return true;
    }

    // Escreve a orientacao. Converte de volta para vetor unitario -- o campo
    // em camera+0x40 e um vetor direcao, nao angulos de Euler.
    void WriteView(CamFollowPedCamera* Cam, const AimMath::Angles& A)
    {
        if (!Cam) return;

        const D3DXVECTOR3 Dir = AimMath::ToDirection(A);

        // O terceiro componente do bloco de terceira pessoa guarda um offset
        // proprio; preservamos a diferenca como o codigo original fazia, se
        // nao a camera de ombro salta ao engajar.
        const D3DXVECTOR3 View = Cam->GetViewAngles();
        const D3DXVECTOR3 Third = Cam->GetThirdpersonViewAngles();
        const float FixZ = View.z - Third.z;

        D3DXVECTOR3 ThirdDir = Dir;
        ThirdDir.z -= FixZ;

        Cam->SetThirdpersonViewAngles(ThirdDir);
        Cam->SetViewAngles(Dir);
    }
}
void Core::Features::cAimbot::SetAngles(D3DXVECTOR3 WorldPosition, int Smooth)
{
    // Mantida para quem ja chamava. Agora delega ao motor com o profile
    // legado, para nao existirem dois caminhos de escrita na camera.
    if (!Core::SDK::Pointers::pCamGamePlayDirector)
        return;

    auto Cam = Core::SDK::Pointers::pLocalPlayer->CamGamePlayDirector()->GetFollowPedCamera();
    if (!Cam) return;

    AimMath::Angles View;
    if (!ReadView(Cam, View)) return;

    const D3DXVECTOR3 Delta = WorldPosition - Cam->GetCrosshairPosition();
    if (D3DXVec3Length(&const_cast<D3DXVECTOR3&>(Delta)) <= 0.0001f) return;

    const AimParams P = ParamsFromSmooth(Smooth, Smooth);

    g_PedEngine.Engage(View);
    WriteView(Cam, g_PedEngine.Tick(View, AimMath::FromDirection(Delta), P, g_PedEngine.Delta()));
}

void Core::Features::cAimbot::Start()
{
    // Sticky Target: sobrevive entre iteracoes do loop, e so e limpo quando a
    // tecla solta. Guardar o ponteiro em vez de reeleger o alvo a cada 1ms e o
    // que impede a mira de pular pro cara que passou na frente no meio da
    // rajada -- que era o comportamento sem isso.
    CPed* StickyTarget = nullptr;

    while (true)
    {
        const bool KeyHeld =
            g_Config.Aimbot->Enabled &&
            g_Config.Aimbot->KeyBind &&
            (GetAsyncKeyState(g_Config.Aimbot->KeyBind) & 0x8000) &&
            GetForegroundWindow() != g_Variables.g_hCheatWindow;

        if (!KeyHeld)
        {
            // Soltou a tecla: o proximo engate escolhe alvo do zero.
            StickyTarget = nullptr;
            g_PedEngine.Release();
            g_VehicleEngine.Release();
            Core::Features::g_AimTarget.store(nullptr, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::nanoseconds(10));
            continue;
        }

        CPed* Local = Core::SDK::Pointers::pLocalPlayer;

        // So atua com arma na mao E mirando. Sem isso o aimbot puxava a
        // camera enquanto o jogador so andava pela rua.
        //
        // IsAimingGun (flag 78) e o proprio estado do jogo, entao vale para
        // mira livre e para lock-on -- nao depende de tecla nenhuma.
        const bool Armed =
            Local && Local->GetWeaponManager() &&
            ((uint32_t)Local->GetWeaponManager()->GetWeaponHash() != 0xA2719263u); // weapon_unarmed

        const bool Aiming = Local && Local->HasFlag(IsAimingGun);

        if (!Armed || !Aiming)
        {
            g_PedEngine.Release();
            g_VehicleEngine.Release();
            Core::Features::g_AimTarget.store(nullptr, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::nanoseconds(10));
            continue;
        }

        auto* Director = Local ? Local->CamGamePlayDirector() : nullptr;

        CamFollowPedCamera* Cam = Director ? Director->GetFollowPedCamera() : nullptr;

        AimMath::Angles View;
        if (!Cam || !ReadView(Cam, View))
        {
            g_PedEngine.Release();
            g_VehicleEngine.Release();
            Core::Features::g_AimTarget.store(nullptr, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::nanoseconds(10));
            continue;
        }

        const auto AimAtWheel = [&]()
        {
            g_PedEngine.Release();
            Core::Features::g_AimTarget.store(nullptr, std::memory_order_relaxed);
            if (!g_Config.AimbotCar->Enabled)
            {
                g_VehicleEngine.Release();
                return;
            }

            const float FovPx = FovRadius(g_Config.Aimbot->FOV);
            Core::SDK::Game::VehicleWheelTarget Wheel{};
            bool Found = Core::SDK::Game::GetClosestVehicleWheel(
                g_Config.Aimbot->MaxDistance, FovPx, Wheel, nullptr, 0,
                [](const Core::SDK::Game::VehicleWheelTarget& Candidate) {
                    return !g_VehicleWheelVisibility.Blocked(Candidate);
                });
            if (!Found)
            {
                g_VehicleEngine.Release();
                return;
            }

            g_VehicleWheelVisibility.Request(Local, Wheel);

            const D3DXVECTOR3 Direction = Wheel.World - Cam->GetCrosshairPosition();
            if (D3DXVec3Length(&const_cast<D3DXVECTOR3&>(Direction)) <= 0.0001f)
            {
                g_VehicleEngine.Release();
                return;
            }
            const AimParams Params = ParamsFromSmooth(
                g_Config.Aimbot->SmoothHorizontal, g_Config.Aimbot->SmoothVertical);
            g_VehicleEngine.Engage(View);
            WriteView(Cam, g_VehicleEngine.Tick(
                View, AimMath::FromDirection(Direction), Params, g_VehicleEngine.Delta()));
        };

        CPed* Ped = nullptr;

        // Com o sticky ligado o alvo antigo tem prioridade e so e reeleito
        // quando deixa de passar nos filtros (morreu, saiu do alcance, ficou
        // invisivel com OnlyVisible ligado).
        if (g_Config.Aimbot->StickyTarget && StickyTarget &&
            Core::SDK::Game::IsPedStillValid(
                StickyTarget,
                g_Config.Aimbot->MaxDistance,
                g_Config.Aimbot->IgnoreNPCs,
                g_Config.Aimbot->OnlyVisible))
        {
            Ped = StickyTarget;
        }
        else
        {
            Ped = Core::SDK::Game::GetClosestPed(
                g_Config.Aimbot->MaxDistance,
                g_Config.Aimbot->IgnoreNPCs,
                g_Config.Aimbot->OnlyVisible
            );
            StickyTarget = Ped;
        }

        if (!Ped)
        {
            AimAtWheel();
            std::this_thread::sleep_for(std::chrono::nanoseconds(10));
            continue;
        }

        // Head = indice fixo 0x00; Neck = BoneMask 0x9995; Torso conserva
        // SKEL_Spine3. A funcao compartilhada nao depende do cache do ESP.
        D3DXVECTOR3 AimPos{};
        if (!Core::SDK::Game::GetAimHitboxPosition(
                Ped, g_Config.Aimbot->Hitbox, AimPos))
        {
            AimAtWheel();
            std::this_thread::sleep_for(std::chrono::nanoseconds(10));
            continue;
        }

        const D3DXVECTOR2 Screen = Core::SDK::Game::WorldToScreen(AimPos);

        if (!Core::SDK::Game::IsOnScreen(Screen))
        {
            AimAtWheel();
            std::this_thread::sleep_for(std::chrono::nanoseconds(10));
            continue;
        }

        const float Fov = std::hypot(
            Screen.x - g_Variables.g_vGameWindowCenter.x,
            Screen.y - g_Variables.g_vGameWindowCenter.y);

        if (Fov >= FovRadius(g_Config.Aimbot->FOV))
        {
            AimAtWheel();
            std::this_thread::sleep_for(std::chrono::nanoseconds(10));
            continue;
        }

        // Predicao de tempo de voo -- o modulo antigo, que ja convergia bem.
        if (g_Config.Aimbot->Prediction)
        {
            const D3DXVECTOR3 CamPos = Cam->GetCameraPosition();
            const float BulletSpeed = Ped->GetWeaponManager()->GetBulletSpeed();

            if (BulletSpeed > 0.1f)
            {
                const D3DXVECTOR3 Vel = Mem.Read<D3DXVECTOR3>((uint64_t)Ped + g_Offsets.m_Speed);

                D3DXVECTOR3 Delta = Ped->GetPos() - CamPos;
                const float Dist = D3DXVec3Length(&Delta);

                D3DXVECTOR3 Predicted = AimPos + (ValidateVelocity(Vel) * (Dist / BulletSpeed));
                AimPos = Predicted;
            }
        }

        const D3DXVECTOR3 ToTarget = AimPos - Cam->GetCrosshairPosition();
        if (D3DXVec3Length(&const_cast<D3DXVECTOR3&>(ToTarget)) <= 0.0001f)
        {
            std::this_thread::sleep_for(std::chrono::nanoseconds(10));
            continue;
        }

        const AimParams P = ParamsFromSmooth(g_Config.Aimbot->SmoothHorizontal,
                                             g_Config.Aimbot->SmoothVertical);

        // Chegou aqui: esta puxando de verdade neste frame.
        g_VehicleEngine.Release();
        Core::Features::g_AimTarget.store(Ped, std::memory_order_relaxed);

        g_PedEngine.Engage(View);

        const AimMath::Angles Target = AimMath::FromDirection(ToTarget);
        WriteView(Cam, g_PedEngine.Tick(View, Target, P, g_PedEngine.Delta()));

        // ~1ms. O passo fixo interno cuida da fisica, entao a taxa do loop so
        // define a resolucao da amostragem -- nao o comportamento.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
