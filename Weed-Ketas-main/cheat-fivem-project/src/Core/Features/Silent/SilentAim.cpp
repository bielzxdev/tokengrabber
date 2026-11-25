
#include "pch.h"
#include "SilentAim.hpp"
#include <array>
#include <thread>
#include <chrono>
#include <cstring>
#include <cmath>
#include "Bypass/CameraBypass.hpp"
#include <Core/Features/VehicleWheelVisibility.hpp>

namespace Core
{
    namespace Features
    {
        namespace
        {
            // Local do hook: dentro de HandleBullet, na instrucao
            //     F3 41 0F 10 19    movss xmm3, [r9]
            // A assinatura m_HandleBullet e "08 4F 38 F3 41 0F 10 19",
            // entao a instrucao comeca em m_HandleBullet + 3.
            constexpr uint8_t kStolen[5] = { 0xF3, 0x41, 0x0F, 0x10, 0x19 };
            constexpr size_t  kStolenSize = sizeof(kStolen);

            uintptr_t g_HookSite = 0;
            uintptr_t g_Cave = 0;
            uintptr_t g_DataCave = 0;              // 24 bytes: X,Y,Z,SX,SY,SZ (floats)
            size_t    g_CaveSize = 0x100;
            std::vector<uint8_t> g_OrigBytes;
            bool g_Ready = false;
            bool g_JmpInstalled = false;
            bool g_CaveWritten = false;
            bool g_LastMagic = false;
            DWORD g_BoundPid = 0;
            uintptr_t g_BoundModuleBase = 0;

            // Ultimos valores escritos no DataCave — fast-path quando nao mudou.
            uint32_t g_LastX = 0, g_LastY = 0, g_LastZ = 0;
            uint32_t g_LastSX = 0, g_LastSY = 0, g_LastSZ = 0;

            // Layout do DataCave (24 bytes):
            //   +0: X (float)   +4: Y   +8: Z     (end position)
            //  +12: SX           +16: SY  +20: SZ (start position, magic only)
            constexpr size_t kDataCaveSize = 24;

            bool IsCurrentAttachment()
            {
                // Mem is the authoritative attachment state while a scan is
                // being committed. g_LastFiveMPid is published only after all
                // signatures have been resolved, so requiring it here made
                // the first valid tick after a reattach miss the hook.
                const DWORD pid = Mem.ProcId;
                return pid && Mem.ProcHandle && Mem.ModBase &&
                    GetProcessId(Mem.ProcHandle) == pid &&
                    (!g_BoundPid ||
                        (g_BoundPid == pid && g_BoundModuleBase == Mem.ModBase));
            }

            void ClearHookState()
            {
                g_HookSite = 0;
                g_Cave = 0;
                g_DataCave = 0;
                g_OrigBytes.clear();
                g_Ready = false;
                g_JmpInstalled = false;
                g_CaveWritten = false;
                g_LastMagic = false;
                g_BoundPid = 0;
                g_BoundModuleBase = 0;
                g_LastX = g_LastY = g_LastZ = 0;
                g_LastSX = g_LastSY = g_LastSZ = 0;
            }

            // ComputeLocalPlayerGuid foi removido: era usado so pelo hook de
            // GetPedLastWeaponImpactCoord, que tambem foi removido. O
            // CreateRemoteThread/execShellcode levava ~120ms na primeira
            // chamada (dominava o max_us do SilentAim).

        }

        // Resolve o endereco do hook e aloca o cave uma unica vez.
        bool cSilentAim::EnsureCave()
        {
            if (g_Ready)
            {
                // A PID or module base change invalidates every address and
                // cave cached by this feature. Core normally calls Shutdown
                // first; this guard also prevents a stale hook from being
                // written if attachment detection races a retry.
                if (IsCurrentAttachment())
                    return true;

                ClearHookState();
            }

            if (!IsCurrentAttachment())
                return false;

            if (!g_Offsets.m_HandleBullet) {
                return false;
            }

            const uintptr_t Site = g_Offsets.m_HandleBullet + 3;

            std::vector<uint8_t> Current = Mem.ReadBytes(Site, kStolenSize);
            if (Current.size() != kStolenSize) {
                return false;
            }

            if (std::memcmp(Current.data(), kStolen, kStolenSize) != 0) {
                return false;
            }

            g_Cave = Mem.CreateCodeCaveNearby(Site, 0x100);
            if (!g_Cave) {
                return false;
            }

            // DataCave onde armazenamos os floats. O codeCave le dele via
            // mov r10, dataCave + mov eax, [r10+N]. Updates do SilentAim
            // so escrevem no DataCave (12-24 bytes), sem tocar o codeCave.
            g_DataCave = Mem.CreateDataCave(kDataCaveSize);
            if (!g_DataCave) {
                return false;
            }

            g_HookSite = Site;
            g_OrigBytes = Current;
            g_BoundPid = Mem.ProcId;
            g_BoundModuleBase = Mem.ModBase;
            g_Ready = true;
            return true;
        }

        void cSilentAim::Shutdown()
        {
            std::lock_guard<std::mutex> lock(m_StateMutex);

            // Restore while the old process handle is still valid. The
            // caves are intentionally not freed here: a game thread can be
            // returning through one while the process is being replaced.
            RestoreSilent();
            g_CameraBypass.Shutdown(false);
            ClearHookState();
        }

        void cSilentAim::RestoreSilent()
        {
            if (!g_Ready || !g_JmpInstalled) return;

            // Never send an old hook address through a newly opened process
            // handle. If teardown already advanced the attachment, the old
            // process is no longer ours to touch.
            if (!IsCurrentAttachment())
            {
                g_JmpInstalled = false;
                return;
            }

            // Restaura exatamente os bytes lidos do proprio jogo.
            // A versao antiga escrevia 17 bytes fixos em g_Offsets.m_SilentAim,
            // que e outra funcao — corrompia o alvo errado a cada tiro.
            if (Mem.PatchFunc(g_HookSite, g_OrigBytes))
                g_JmpInstalled = false;

            // O cave nao e liberado: o jogo pode estar executando dentro dele
            // neste instante. Fica alocado e e reaproveitado no proximo tiro.
        }

        // Monta o codeCave uma vez. Lê floats do DataCave e escreve em [r9]
        // ([r8], [rdi+N] no modo magic). Nao usa IMMs — opcodes nunca mudam.
        //
        //   push rax ; push r10                               (2 bytes)
        //   mov  r10, dataCave                                (10)
        //   ; End position in [r9+0..8]
        //   mov eax, [r10]     / mov [r9], eax                (7)
        //   mov eax, [r10+4]   / mov [r9+4], eax              (8)
        //   mov eax, [r10+8]   / mov [r9+8], eax              (8)
        //   (if magic:) three more blocks writing to [r8], [rdi], [rdi+0x10]
        //   pop r10 ; pop rax                                 (2)
        //   movss xmm3, [r9]                                  (5) stolen
        //   jmp back                                          (5)
        //
        // RAX e R10 sao voláteis e preservados via push/pop.
        static bool BuildCodeCave(bool magic)
        {
            auto p8 = [](std::vector<uint8_t>& v, uint32_t val) {
                v.push_back((uint8_t)val); v.push_back((uint8_t)(val>>8));
                v.push_back((uint8_t)(val>>16)); v.push_back((uint8_t)(val>>24));
            };
            auto p64 = [](std::vector<uint8_t>& v, uint64_t val) {
                for (int i = 0; i < 8; i++) v.push_back((uint8_t)(val >> (i * 8)));
            };

            std::vector<uint8_t> Cave;
            Cave.reserve(0x80);

            // push rax; push r10
            Cave.push_back(0x50); Cave.push_back(0x41); Cave.push_back(0x52);
            // mov r10, dataCave
            Cave.push_back(0x49); Cave.push_back(0xBA); p64(Cave, (uint64_t)g_DataCave);

            // End: mov eax,[r10+0] -> mov [r9+0],eax
            Cave.insert(Cave.end(), { 0x41,0x8B,0x02 });            // mov eax,[r10]
            Cave.insert(Cave.end(), { 0x41,0x89,0x01 });            // mov [r9],eax
            Cave.insert(Cave.end(), { 0x41,0x8B,0x42,0x04 });       // mov eax,[r10+4]
            Cave.insert(Cave.end(), { 0x41,0x89,0x41,0x04 });       // mov [r9+4],eax
            Cave.insert(Cave.end(), { 0x41,0x8B,0x42,0x08 });       // mov eax,[r10+8]
            Cave.insert(Cave.end(), { 0x41,0x89,0x41,0x08 });       // mov [r9+8],eax

            if (magic) {
                // Start: [r8+0..8]
                Cave.insert(Cave.end(), { 0x41,0x8B,0x42,0x0C });   // mov eax,[r10+12]
                Cave.insert(Cave.end(), { 0x41,0x89,0x00 });        // mov [r8],eax
                Cave.insert(Cave.end(), { 0x41,0x8B,0x42,0x10 });   // mov eax,[r10+16]
                Cave.insert(Cave.end(), { 0x41,0x89,0x40,0x04 });   // mov [r8+4],eax
                Cave.insert(Cave.end(), { 0x41,0x8B,0x42,0x14 });   // mov eax,[r10+20]
                Cave.insert(Cave.end(), { 0x41,0x89,0x40,0x08 });   // mov [r8+8],eax
                // [rdi+0..8]
                Cave.insert(Cave.end(), { 0x41,0x8B,0x42,0x0C });
                Cave.insert(Cave.end(), { 0x89,0x07 });             // mov [rdi],eax
                Cave.insert(Cave.end(), { 0x41,0x8B,0x42,0x10 });
                Cave.insert(Cave.end(), { 0x89,0x47,0x04 });
                Cave.insert(Cave.end(), { 0x41,0x8B,0x42,0x14 });
                Cave.insert(Cave.end(), { 0x89,0x47,0x08 });
                // [rdi+0x10..0x18]
                Cave.insert(Cave.end(), { 0x41,0x8B,0x42,0x0C });
                Cave.insert(Cave.end(), { 0x89,0x47,0x10 });
                Cave.insert(Cave.end(), { 0x41,0x8B,0x42,0x10 });
                Cave.insert(Cave.end(), { 0x89,0x47,0x14 });
                Cave.insert(Cave.end(), { 0x41,0x8B,0x42,0x14 });
                Cave.insert(Cave.end(), { 0x89,0x47,0x18 });
            }

            // pop r10; pop rax
            Cave.push_back(0x41); Cave.push_back(0x5A); Cave.push_back(0x58);
            // Stolen: movss xmm3,[r9]
            Cave.insert(Cave.end(), { 0xF3,0x41,0x0F,0x10,0x19 });

            // jmp back
            const size_t bodySize = Cave.size();
            const intptr_t BackRel = (intptr_t)(g_HookSite + kStolenSize)
                                   - (intptr_t)(g_Cave + bodySize + 5);
            Cave.push_back(0xE9); p8(Cave, (uint32_t)BackRel);

            if (!Mem.UnsealCave(g_Cave, g_CaveSize)) {
                return false;
            }
            if (!Mem.WriteBytes(g_Cave, Cave)) {
                Mem.SealCave(g_Cave, g_CaveSize);
                return false;
            }
            if (!Mem.SealCave(g_Cave, g_CaveSize)) {
                return false;
            }
            FlushInstructionCache(Mem.ProcHandle,
                reinterpret_cast<const void*>(g_Cave), Cave.size());
            return true;
        }

        bool cSilentAim::ApplySilent(const D3DXVECTOR3& EndBulletPos,
            const D3DXVECTOR3& StartBulletPos, bool MagicBullets)
        {
            if (!EnsureCave()) return false;

            const bool magic = MagicBullets;

            union { float f; uint32_t i; } X, Y, Z, SX, SY, SZ;
            X.f = EndBulletPos.x;  Y.f = EndBulletPos.y;  Z.f = EndBulletPos.z;
            SX.f = StartBulletPos.x; SY.f = StartBulletPos.y; SZ.f = StartBulletPos.z;

            // Fast-path: nada mudou → sem syscalls.
            if (g_CaveWritten && magic == g_LastMagic &&
                X.i == g_LastX && Y.i == g_LastY && Z.i == g_LastZ &&
                SX.i == g_LastSX && SY.i == g_LastSY && SZ.i == g_LastSZ)
            {
                return g_JmpInstalled;
            }

            // Primeira vez ou troca de magic/non-magic: reconstroi o codeCave.
            // Isso faz unseal/seal mas so em troca de modo (raro).
            if (!g_CaveWritten || magic != g_LastMagic) {
                if (!BuildCodeCave(magic)) return false;
                g_CaveWritten = true;
                g_LastMagic = magic;
            }

            // Hot path: so escreve floats no DataCave (sem syscall de protecao).
            // DataCave e PAGE_READWRITE desde a alocacao, nao precisa unseal.
            struct Buf { uint32_t v[6]; } buf;
            buf.v[0] = X.i;  buf.v[1] = Y.i;  buf.v[2] = Z.i;
            buf.v[3] = SX.i; buf.v[4] = SY.i; buf.v[5] = SZ.i;
            // Escreve so o que mudou. Non-magic ignora os 12 bytes finais.
            const size_t writeBytes = magic ? kDataCaveSize : 12;
            if (!Mem.WriteBytesImpl(g_DataCave, buf.v, writeBytes))
                return false;

            g_LastX = X.i; g_LastY = Y.i; g_LastZ = Z.i;
            g_LastSX = SX.i; g_LastSY = SY.i; g_LastSZ = SZ.i;

            // Instala o JMP so uma vez, depois do cave estar pronto.
            if (!g_JmpInstalled)
            {
                const intptr_t HookRel = (intptr_t)g_Cave - (intptr_t)(g_HookSite + 5);
                const uint32_t Rel = (uint32_t)HookRel;
                std::vector<uint8_t> Jmp = {
                    0xE9, (uint8_t)Rel, (uint8_t)(Rel>>8), (uint8_t)(Rel>>16), (uint8_t)(Rel>>24)
                };
                if (Mem.PatchFunc(g_HookSite, Jmp))
                    g_JmpInstalled = true;
            }
            return g_JmpInstalled;
        }

        void cSilentAim::HookSilent()
        {
            constexpr auto UpdateInterval = std::chrono::microseconds(1000);
            auto NextTick = std::chrono::steady_clock::now();

            // Estado da camera entre frames. Escopo HookSilent para que o
            // bloco "!ValidTarget" (reset ao desativar) consiga limpar.
            D3DXVECTOR3 s_lastGoodCameraPos{};
            bool s_hasLastGoodCamera = false;
            auto s_lastGoodTime = std::chrono::steady_clock::now();

            while (true)
            {
                std::lock_guard<std::mutex> stateLock(m_StateMutex);
                bool ValidTarget = false;
                const bool CarKeyHeld =
                    g_Config.SilentAimCar->Enabled &&
                    (g_Config.SilentAim->Enabled || g_Config.SilentAim->MagicBullets) &&
                    g_Config.SilentAim->KeyBind &&
                    (GetAsyncKeyState(g_Config.SilentAim->KeyBind) & 0x8000) &&
                    GetForegroundWindow() != g_Variables.g_hCheatWindow &&
                    Core::SDK::Pointers::pLocalPlayer;

                if ((g_Config.SilentAim->Enabled || g_Config.SilentAim->MagicBullets) &&
                    g_Config.SilentAim->KeyBind &&
                    (GetAsyncKeyState(g_Config.SilentAim->KeyBind) & 0x8000) &&
                    GetForegroundWindow() != g_Variables.g_hCheatWindow &&
                    Core::SDK::Pointers::pLocalPlayer)
                {
                    CPed* Ped = Core::SDK::Game::GetClosestPed(
                        g_Config.SilentAim->MaxDistance,
                        g_Config.SilentAim->IgnoreNPCs,
                        g_Config.SilentAim->OnlyVisible
                    );
                    D3DXVECTOR3 HitboxPos{};
                    if (Ped && Core::SDK::Game::GetAimHitboxPosition(
                            Ped, g_Config.SilentAim->Hitbox, HitboxPos))
                    {
                        const D3DXVECTOR2 Screen = Core::SDK::Game::WorldToScreen(HitboxPos);
                        if (Core::SDK::Game::IsOnScreen(Screen))
                        {
                            const float Dx = Screen.x - g_Variables.g_vGameWindowCenter.x;
                            const float Dy = Screen.y - g_Variables.g_vGameWindowCenter.y;
                            const float Radius = FovRadius(g_Config.SilentAim->FOV);
                            if (Dx * Dx + Dy * Dy < Radius * Radius)
                            {
                                // Both the redirected bullet and spoofed camera
                                // must point at the selected hitbox position.
                                const D3DXVECTOR3 PlayerPos =
                                    Core::SDK::Pointers::pLocalPlayer->GetPos();
                                D3DXVECTOR3 ToHitbox = HitboxPos - PlayerPos;
                                // Uma trajetoria horizontal evita cruzar outras
                                // hitboxes antes de chegar ao ponto escolhido.
                                ToHitbox.z = 0.f;
                                const float HorizontalDistance = D3DXVec3Length(&ToHitbox);
                                if (HorizontalDistance > 0.01f)
                                {
                                    ToHitbox /= HorizontalDistance;
                                    const D3DXVECTOR3 RayStart = HitboxPos - ToHitbox * 0.75f;

                                    // Posição REAL da câmera do jogo. Usada pelo
                                    // CameraBypass como origem da trajetória
                                    // forjada; sem ela a native devolveria o
                                    // boneco e divergiria do que o jogo mostra
                                    // em outras natives (GetEntityCoords, screen
                                    // projection do impacto, etc.).
                                    D3DXVECTOR3 CameraPos{};
                                    if (auto* Director = Core::SDK::Pointers::pLocalPlayer->CamGamePlayDirector())
                                        if (auto* Follow = Director->GetFollowPedCamera())
                                            CameraPos = Follow->GetCameraPosition();

                                    // Durante transições de câmera (apertar C,
                                    // troca de FOV, giradas rápidas) o ponteiro
                                    // CamFollowPedCamera pode ser trocado
                                    // mid-read e GetCameraPosition devolve
                                    // garbage/zero/endereço errado. Validamos
                                    // aqui e pulamos o hook no frame — assim
                                    // as natives devolvem o real durante a
                                    // transição (seguro). O snapshot
                                    // s_lastGoodCameraPos esta no escopo da
                                    // funcao (acima).
                                    const bool CameraFinite =
                                        std::isfinite(CameraPos.x) &&
                                        std::isfinite(CameraPos.y) &&
                                        std::isfinite(CameraPos.z);
                                    const bool CameraNonZero =
                                        CameraPos.x != 0.f || CameraPos.y != 0.f || CameraPos.z != 0.f;

                                    const float DxP = CameraPos.x - PlayerPos.x;
                                    const float DyP = CameraPos.y - PlayerPos.y;
                                    const float DzP = CameraPos.z - PlayerPos.z;
                                    const float CamDistSq = DxP * DxP + DyP * DyP + DzP * DzP;
                                    // 3ª pessoa fica <5m, scopes de sniper
                                    // podem esticar; 15m² = 225 é folga sem
                                    // deixar garbage passar.
                                    const bool CameraClose = CamDistSq < 225.f;

                                    bool CameraOk = CameraFinite && CameraNonZero && CameraClose;

                                    // Hysteresis: pulo > 5m entre frames
                                    // consecutivos = transição em curso.
                                    // Mas se o snapshot tem >100ms (silent
                                    // ficou desligado + jogador andou), ele
                                    // esta stale → descarta antes de comparar.
                                    const auto CamNow = std::chrono::steady_clock::now();
                                    if (s_hasLastGoodCamera &&
                                        (CamNow - s_lastGoodTime) > std::chrono::milliseconds(100))
                                    {
                                        s_hasLastGoodCamera = false;
                                    }

                                    if (CameraOk && s_hasLastGoodCamera)
                                    {
                                        const float DxL = CameraPos.x - s_lastGoodCameraPos.x;
                                        const float DyL = CameraPos.y - s_lastGoodCameraPos.y;
                                        const float DzL = CameraPos.z - s_lastGoodCameraPos.z;
                                        if (DxL * DxL + DyL * DyL + DzL * DzL > 25.f)
                                            CameraOk = false;
                                    }
                                    if (CameraOk) s_lastGoodTime = CamNow;

                                    if (CameraOk)
                                    {
                                        s_lastGoodCameraPos = CameraPos;
                                        s_hasLastGoodCamera = true;

                                        // Hook de GetPedLastImpact foi removido, nao precisamos mais do GUID.
                                        g_CameraBypass.HookCamera(
                                            HitboxPos, PlayerPos, CameraPos, 0);

                                        ValidTarget = ApplySilent(HitboxPos, RayStart,
                                            g_Config.SilentAim->MagicBullets);
                                    }
                                    // Se !CameraOk: ValidTarget fica false, o
                                    // bloco de UnhookCamera abaixo restaura.
                                }
                            }
                        }
                    }
                }

                // SilentCar: redireciona a bala para o pneu do veiculo mais
                // proximo do crosshair. Rodado como fallback quando o
                // SilentAim normal nao achou alvo (ped), ou standalone.
                if (!ValidTarget && CarKeyHeld)
                {
                    const float CarFovPx = FovRadius(g_Config.SilentAim->FOV);
                    Core::SDK::Game::VehicleWheelTarget Target{};
                    bool Found = Core::SDK::Game::GetClosestVehicleWheel(
                        g_Config.SilentAim->MaxDistance, CarFovPx, Target,
                        nullptr, 0,
                        [](const Core::SDK::Game::VehicleWheelTarget& Candidate) {
                            return !g_VehicleWheelVisibility.Blocked(Candidate);
                        });
                    if (Found)
                    {
                        g_VehicleWheelVisibility.Request(
                            Core::SDK::Pointers::pLocalPlayer, Target);
                        const D3DXVECTOR3 WheelPos = Target.World;
                        const D3DXVECTOR3 PlayerPos =
                            Core::SDK::Pointers::pLocalPlayer->GetPos();
                        D3DXVECTOR3 ToWheel = WheelPos - PlayerPos;
                        const float RayDistance = D3DXVec3Length(&ToWheel);
                        if (RayDistance > 0.01f)
                        {
                            ToWheel /= RayDistance;
                            // Comeca imediatamente antes da face exposta do
                            // pneu. O deslocamento antigo de 0.75 m podia cair
                            // dentro da carroceria ou abaixo do terreno em
                            // areia, rampas e margens d'agua.
                            const D3DXVECTOR3 RayStart = WheelPos - ToWheel * 0.18f;

                            D3DXVECTOR3 CameraPos{};
                            if (auto* Director = Core::SDK::Pointers::pLocalPlayer->CamGamePlayDirector())
                                if (auto* Follow = Director->GetFollowPedCamera())
                                    CameraPos = Follow->GetCameraPosition();

                            const D3DXVECTOR3 CameraDelta = CameraPos - PlayerPos;
                            const float CameraDistanceSq = CameraDelta.x * CameraDelta.x +
                                CameraDelta.y * CameraDelta.y + CameraDelta.z * CameraDelta.z;
                            const bool CameraOk = std::isfinite(CameraPos.x) &&
                                std::isfinite(CameraPos.y) && std::isfinite(CameraPos.z) &&
                                (CameraPos.x != 0.f || CameraPos.y != 0.f || CameraPos.z != 0.f) &&
                                CameraDistanceSq < 225.f;

                            if (CameraOk)
                            {
                                g_CameraBypass.HookCamera(WheelPos, PlayerPos, CameraPos, 0);

                                // O modo de pneu sempre precisa sobrescrever
                                // origem e destino: somente trocar o destino
                                // ainda deixa o raycast real bater no solo ou
                                // na lataria antes de alcancar a borracha.
                                ValidTarget = ApplySilent(WheelPos, RayStart, true);
                            }
                        }
                    }
                }

                if (!ValidTarget)
                {
                    RestoreSilent();
                    g_CameraBypass.UnhookCamera();
                    // Reseta snapshot da camera: se o jogador andar enquanto
                    // o silent fica desligado, o lastGoodCameraPos vira stale
                    // e trava o hook em "invalid-camera" na proxima ativacao.
                    s_hasLastGoodCamera = false;
                }

                // Mantem uma cadencia de 1 ms incluindo o tempo gasto na
                // leitura e aplicacao, em vez de somar 1 ms ao processamento.
                NextTick += UpdateInterval;
                const auto WaitNow = std::chrono::steady_clock::now();
                if (NextTick > WaitNow)
                    std::this_thread::sleep_until(NextTick);
                else
                    NextTick = WaitNow;
            }
        }
    }
}
