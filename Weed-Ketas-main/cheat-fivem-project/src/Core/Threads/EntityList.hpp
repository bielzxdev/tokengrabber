#pragma once
#include <Includes/Includes.hpp>
#include <Includes/Utils.hpp>
#include <Core/Offsets.hpp>
#include <Core/Core.hpp>
#include <Core/Features/VisibleCheck.hpp>
#include <Core/Features/Esp/WeaponNames.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <regex>

namespace Core
{
    namespace Threads
    {
        class cEntityList
        {
        public:
            // Alias para o mutex que agora vive junto do vetor, em SDK.hpp.
            // Mantido para nao quebrar quem ja referencia g_EntityList.EntityListMutex.
            std::mutex& EntityListMutex = Core::SDK::Game::EntityListMutex;
        private:
            CPedInterFace* CPedInterFace = nullptr;
            CPedList* CPedList = nullptr;
            bool bVisCheckInitialized = false;
        public:
            std::unordered_map<CPed*, Core::SDK::Game::EntityStruct> CachedEntities;
        public:
            void Update()
            {
                while (true)
                {
                    // 1200 NANOsegundos: a thread refazia a lista inteira de
                    // entidades sem parar, no maximo que a CPU aguentasse, e
                    // cada volta e uma rajada de leituras no processo do jogo.
                    // Quem consome sao o ESP (um snapshot por frame) e o aimbot
                    // (um loop de 1ms), entao 2ms ja entrega a lista mais fresca
                    // do que qualquer um dos dois consegue usar.
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));

                    if (!Core::SDK::Pointers::pReplayInterFace ||
                        !Core::SDK::Pointers::pLocalPlayer)
                        continue;

                    // O initialize() ficava FORA do loop, entao rodava uma unica
                    // vez no start da thread -- antes do jogo carregar. Se a
                    // assinatura do array de occlusion nao resolvesse naquele
                    // instante, ficava desligado pela sessao inteira.
                    if (!bVisCheckInitialized)
                        bVisCheckInitialized = Core::Features::g_visibilitychecker.initialize();

                    // Fora do if de proposito: o raycast NAO usa o array de
                    // occlusion. Estava tudo gated no mesmo flag, entao uma
                    // falha na occlusion query deixava m_localplayer em 0 e o
                    // raycast nem chegava a ser tentado -- eram dois sistemas
                    // independentes amarrados num flag so.
                    Core::Features::g_visibilitychecker.setlocalplayer(
                        (uintptr_t)Core::SDK::Pointers::pLocalPlayer);
                    Core::Features::g_visibilitychecker.updateframe();

                    CPedInterFace = Core::SDK::Pointers::pReplayInterFace->InterfacePed();
                    if (!CPedInterFace)
                        continue;

                    CPedList = CPedInterFace->PedList();
                    if (!CPedList)
                        continue;

                    // Uma leitura por passada. Estava dentro do laco, entao
                    // era uma syscall por ped so para buscar a MESMA posicao.
                    const D3DXVECTOR3 LocalPos = Core::SDK::Pointers::pLocalPlayer->GetPos();

                    size_t Size = Core::SDK::Game::EntityList.size();
                    size_t Index = 0;
                    static const int PedCount = CPedInterFace->MaxPed();

                    // Monta numa lista LOCAL e so toma o mutex no fim, para a troca.
                    // Antes o lock ficava preso durante a varredura inteira de peds
                    // (com sleep de 1.2us entre voltas), entao ficava ocupado ~sempre
                    // e a GUI nunca conseguia adquirir. Mesmo padrao do VehicleList.
                    // A lista de peds e um array de stride 0x10. Lido slot a
                    // slot, eram PedCount syscalls por passada (tipicamente
                    // 256), a maioria devolvendo slot vazio. Uma leitura so
                    // resolve o array inteiro.
                    static std::vector<uint8_t> PedBlock;
                    PedBlock.assign((size_t)PedCount * 0x10, 0);
                    Mem.ReadBuffer((uintptr_t)CPedList, PedBlock.data(), PedBlock.size());

                    std::vector<Core::SDK::Game::EntityStruct> Updated;
                    Updated.reserve(Size);
                    {
                        for (int i = 0; i < PedCount; i++)
                        {
                            CPed* CurrentPed = *reinterpret_cast<CPed**>(
                                PedBlock.data() + (size_t)i * 0x10);

                            if (!CurrentPed)
                                continue;

                            uintptr_t PedAddr = reinterpret_cast<uintptr_t>(CurrentPed);

                            if (CachedEntities.find(CurrentPed) == CachedEntities.end())
                            {
                                Core::SDK::Game::EntityStruct NewEntity;
                                NewEntity.Id = CurrentPed->GetID();
                                NewEntity.Index = i;
                                NewEntity.Pos = CurrentPed->GetPos();
                                NewEntity.IsFriend = Core::SDK::Game::FriendMap[NewEntity.Id];
                                NewEntity.MaxHealth = CurrentPed->GetMaxHealth();
                                NewEntity.Health = CurrentPed->GetHealth();
                                NewEntity.Armor = CurrentPed->GetArmor();
                                NewEntity.PedType = CurrentPed->GetPedType();
                                NewEntity.Distance = CurrentPed->GetDistance(LocalPos, NewEntity.Pos);
                                
                                // Nao calcula aqui: o bloco logo abaixo roda
                                // para TODO ped, inclusive este, na mesma
                                // passada. Calcular nos dois era consulta
                                // duplicada por ped novo.
                                NewEntity.IsVisible = false;

                                CachedEntities[CurrentPed] = NewEntity;
                            }

                            Core::SDK::Game::EntityStruct& CachedEntity = CachedEntities[CurrentPed];
                            // MaxHealth can be unavailable while the ped is still being streamed.
                            // Refresh it together with Health instead of freezing the first read
                            // in the cache for the entity's whole lifetime.
                            const float CurrentMaxHealth = CurrentPed->GetMaxHealth();
                            if (std::isfinite(CurrentMaxHealth) && CurrentMaxHealth > 1.f)
                                CachedEntity.MaxHealth = CurrentMaxHealth;
                            CachedEntity.Health = CurrentPed->GetHealth();
                            CachedEntity.Armor = CurrentPed->GetArmor();
                            CachedEntity.Pos = CurrentPed->GetPos();
                            CachedEntity.IsFriend = Core::SDK::Game::FriendMap[CachedEntity.Id];
                            CachedEntity.Distance = CurrentPed->GetDistance(LocalPos, CachedEntity.Pos);
                            {
                                auto* WM = CurrentPed->GetWeaponManager();
                                if (!WM)
                                {
                                    CachedEntity.WeaponHash = 0;
                                    CachedEntity.WeaponName.clear();
                                }
                                else
                                {
                                    CachedEntity.WeaponHash = (uint32_t)WM->GetWeaponHash();

                                    if (Core::Features::WeaponNames::FromHash(CachedEntity.WeaponHash).empty())
                                    {
                                        auto* WeaponInfo = WM->GetWeaponInfo();
                                        CachedEntity.WeaponName = WeaponInfo
                                            ? WeaponInfo->GetName()
                                            : std::string();
                                    }
                                    else
                                    {
                                        CachedEntity.WeaponName.clear();
                                    }
                                }
                            }

                            // SEMPRE pelo isentityvisiblefast.
                            //
                            // Antes isso caia em CPed::IsVisible() quando a
                            // occlusion query nao resolvia -- mesmo acoplamento
                            // que ja corrigi no loop. E aquele fallback, sem o
                            // offset resolvido, le um float qualquer no ped
                            // (+0x1A0) e testa "> 0.1f": passa para quase todo
                            // mundo, ou seja, visivel geral.
                            //
                            // O isentityvisiblefast ja decide sozinho entre
                            // raycast e occlusion, e so abre a mao quando nao
                            // tem nenhum dos dois.
                            CachedEntity.IsVisible =
                                Core::Features::g_visibilitychecker.isentityvisiblefast(PedAddr, CachedEntity.Distance);

                            Core::SDK::Game::EntityStruct Entity;
                            Entity.Ped = CurrentPed;
                            Entity.Id = CachedEntity.Id;
                            Entity.Index = CachedEntity.Index;
                            Entity.Pos = CachedEntity.Pos;
                            Entity.IsFriend = CachedEntity.IsFriend;
                            Entity.MaxHealth = CachedEntity.MaxHealth;
                            Entity.Health = CachedEntity.Health;
                            Entity.Armor = CachedEntity.Armor;
                            Entity.PedType = CachedEntity.PedType;
                            Entity.Distance = CachedEntity.Distance;
                            Entity.WeaponName = CachedEntity.WeaponName;
                            Entity.WeaponHash = CachedEntity.WeaponHash;
                            Entity.IsVisible = CachedEntity.IsVisible;

                            Entity.NetworkInfo.UserName = Utils::StringToFirstUpperCase(
                                g_UpdateNames.GetPlayerNameSafe(Entity.Id));

                            if (Entity.NetworkInfo.UserName.empty()) {
                                Entity.NetworkInfo.UserName = xorstr("Ped");
                            }

                            Updated.push_back(Entity);
                            Index++;
                        }

                        CachedEntities.clear();

                        {
                            std::lock_guard<std::mutex> Guard(EntityListMutex);
                            Core::SDK::Game::EntityList = std::move(Updated);
                        }
                    }
                }
            }
        };

        inline cEntityList g_EntityList;
    }
}
