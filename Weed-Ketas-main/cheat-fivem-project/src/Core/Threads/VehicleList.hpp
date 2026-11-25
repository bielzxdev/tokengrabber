#pragma once
#include <Includes/Includes.hpp>
#include <Includes/Utils.hpp>
#include <Core/Offsets.hpp>
#include <Core/Core.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <regex>

namespace Core
{
    namespace Threads
    {
        namespace VehName
        {
            // O gameName do CVehicleModelInfo fica inline como char[] ("ADDER",
            // "POLICE", ...). O offset historico e 0x298, mas ele anda entre
            // builds. Tentamos 0x298 primeiro e, se nao bater, descobrimos o
            // offset certo escaneando -- confirmando em 2 modelos diferentes
            // antes de fixar, para nao travar num campo qualquer.
            inline bool Plausible(const std::string& S)
            {
                if (S.size() < 2 || S.size() > 15) return false;

                bool HasAlpha = false;
                for (char c : S)
                {
                    if (c >= 'A' && c <= 'Z') { HasAlpha = true; continue; }
                    if (c >= '0' && c <= '9') continue;
                    if (c == '_' || c == '-')  continue;
                    return false;
                }
                return HasAlpha;
            }

            inline int FirstPlausibleOffset(uintptr_t ModelInfo)
            {
                if (Plausible(Mem.ReadString(ModelInfo + 0x298)))
                    return 0x298;

                for (int Off = 0x80; Off < 0x800; Off += 4)
                    if (Plausible(Mem.ReadString(ModelInfo + Off)))
                        return Off;

                return -1;
            }

            inline int ResolveOffset(uintptr_t ModelInfo)
            {
                static int Locked = -1;
                static int Candidate = -1;
                static int Confirms = 0;

                if (Locked >= 0)  return Locked;
                if (!ModelInfo)   return -1;

                int Off = FirstPlausibleOffset(ModelInfo);
                if (Off < 0) return -1;

                if (Off == Candidate)
                {
                    if (++Confirms >= 2) Locked = Off;
                }
                else
                {
                    Candidate = Off;
                    Confirms = 1;
                }

                return Off;
            }

            inline std::string Get(uintptr_t ModelInfo)
            {
                if (!ModelInfo) return "";

                int Off = ResolveOffset(ModelInfo);
                if (Off < 0) return "";

                std::string Name = Mem.ReadString(ModelInfo + Off);
                return Plausible(Name) ? Name : std::string();
            }
        }

        class cVehicleList
        {
        public:
            CVehInterFace* VehInterface = nullptr;
            CVehicleList* VehList = nullptr;
            int MaxVehicles = 0;
        public:
            std::unordered_map<CVehicle*, Core::SDK::Game::VehicleStructure> CachedVehicles;
            // nome por CVehicleModelInfo*, persiste entre as varreduras
            std::unordered_map<uintptr_t, std::string> CachedNames;
        public:
            void Update()
            {
                while (true)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds( 2 ));

                    if (!Core::SDK::Pointers::pReplayInterFace ||
                        !Core::SDK::Pointers::pLocalPlayer)
                        continue;

                    VehInterface = Core::SDK::Pointers::pReplayInterFace->InterfaceVeh();
                    if (!VehInterface)
                        continue;

                    VehList = VehInterface->VehicleList();
                    if (!VehList)
                        continue;

                    if (!MaxVehicles)
                        MaxVehicles = VehInterface->MaxVehicles();

                    size_t Size = Core::SDK::Game::VehicleList.size();
                    size_t Index = 0;

                    {
						std::lock_guard<std::mutex> Guard(Core::SDK::Game::VehicleListMutex);

                        for (int i = 0; i < MaxVehicles; i++)
                        {
                            CVehicle* CurrentVeh = VehList->Vehicle(i);
                            if (!CurrentVeh)
                                continue;

                            if (CachedVehicles.find(CurrentVeh) == CachedVehicles.end())
                            {
                                uintptr_t modelInfo = Mem.Read<uintptr_t>((uintptr_t)CurrentVeh + 0x20);

                                // O nome depende so do modelo, entao cacheamos por
                                // CVehicleModelInfo* e nao por veiculo: sem isso a
                                // string era relida do processo a cada 2ms.
                                std::string vehicleName;
                                if (modelInfo)
                                {
                                    auto It = CachedNames.find(modelInfo);
                                    if (It != CachedNames.end())
                                    {
                                        vehicleName = It->second;
                                    }
                                    else
                                    {
                                        vehicleName = VehName::Get(modelInfo);
                                        if (!vehicleName.empty())
                                            CachedNames[modelInfo] = vehicleName;
                                    }
                                }

                                if (vehicleName.empty())
                                    vehicleName = xorstr("Vehicle");

                                Core::SDK::Game::VehicleStructure NewVehicle;
                                NewVehicle.Pointer = CurrentVeh;
                                NewVehicle.Name = Utils::StringToFirstUpperCase(vehicleName);
                                NewVehicle.Pos = CurrentVeh->GetPos();
                                NewVehicle.Health = CurrentVeh->GetHealth();
                                NewVehicle.IsLocked = CurrentVeh->IsLocked();
                                NewVehicle.Driver = CurrentVeh->GetDriver();
                                CachedVehicles[CurrentVeh] = NewVehicle;
                            }

                            auto& CachedVehicle = CachedVehicles[CurrentVeh];
                            CachedVehicle.Pos = CurrentVeh->GetPos();
                            CachedVehicle.Health = CurrentVeh->GetHealth();
                            CachedVehicle.IsLocked = CurrentVeh->IsLocked();
                            CachedVehicle.Driver = CurrentVeh->GetDriver();

                            D3DXVECTOR3 delta = CachedVehicle.Pos - Core::SDK::Pointers::pLocalPlayer->GetPos();
                            float Dist = sqrtf(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);

                            Core::SDK::Game::VehicleStructure Vehicle;
                            Vehicle.Pointer = CurrentVeh;
                            Vehicle.Name = CachedVehicle.Name;
                            Vehicle.Pos = CachedVehicle.Pos;
                            Vehicle.Health = CachedVehicle.Health;
                            Vehicle.IsLocked = CachedVehicle.IsLocked;
                            Vehicle.Driver = CachedVehicle.Driver;
                            Vehicle.Dist = Dist;

                            if (Index < Size)
                                Core::SDK::Game::VehicleList[Index] = Vehicle;
                            else
                                Core::SDK::Game::VehicleList.push_back(Vehicle);

                            Index++;
                        }

                        CachedVehicles.clear();

                        // resize e sort PRECISAM ficar sob o lock: ambos mexem no vetor inteiro
                        // (o sort move as std::string) enquanto a GUI pode estar lendo.
                        if (Index < Size)
                            Core::SDK::Game::VehicleList.resize(Index);

                        std::sort(
                        Core::SDK::Game::VehicleList.begin(),
                        Core::SDK::Game::VehicleList.end(),
                        [](const auto& a, const auto& b)
                        {
                            return a.Dist < b.Dist;
                        }
                    );
                    }
                }
            }
        };

        inline cVehicleList g_VehicleList;
    }
}
