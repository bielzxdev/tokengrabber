#pragma once
#include <Core/Features/LosProbe.hpp>
#include <Core/SDK/SDK.hpp>
#include <chrono>
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace Core::Features {

    // Consulta exclusiva dos pneus. Nunca usa a fila nem o cache de ped.
    class cVehicleWheelVisibility {
        struct Key {
            uintptr_t Vehicle;
            int WheelIndex;
            bool operator==(const Key& Other) const {
                return Vehicle == Other.Vehicle && WheelIndex == Other.WheelIndex;
            }
        };
        struct KeyHash {
            size_t operator()(const Key& Value) const {
                return std::hash<uintptr_t>{}(Value.Vehicle) ^
                    (std::hash<int>{}(Value.WheelIndex) << 1);
            }
        };
        struct Result {
            uintptr_t Wheel = 0;
            D3DXVECTOR3 Point{};
            bool Clear = false;
            std::chrono::steady_clock::time_point Checked{};
        };

        cLosProbe m_probe;
        std::unordered_map<Key, Result, KeyHash> m_results;
        Key m_pendingKey{};
        Result m_pendingResult{};
        bool m_pending = false;
        std::mutex m_mutex;

        static Key MakeKey(const Core::SDK::Game::VehicleWheelTarget& Target) {
            return { reinterpret_cast<uintptr_t>(Target.Vehicle), Target.WheelIndex };
        }
        static bool Fresh(const Result& Cached,
            const Core::SDK::Game::VehicleWheelTarget& Target) {
            const auto Delta = Cached.Point - Target.World;
            const float MovedSq = Delta.x * Delta.x + Delta.y * Delta.y + Delta.z * Delta.z;
            return Cached.Wheel == Target.Wheel && MovedSq < 0.25f &&
                std::chrono::steady_clock::now() - Cached.Checked <
                    std::chrono::milliseconds(200);
        }
        void PumpLocked() {
            if (!m_pending) return;
            const int Reply = m_probe.Collect();
            if (Reply >= 0) {
                m_pendingResult.Clear = Reply == 1;
                m_pendingResult.Checked = std::chrono::steady_clock::now();
                if (m_results.size() >= 256) {
                    const auto Oldest = std::chrono::steady_clock::now() -
                        std::chrono::seconds(1);
                    for (auto It = m_results.begin(); It != m_results.end();) {
                        if (It->second.Checked < Oldest)
                            It = m_results.erase(It);
                        else
                            ++It;
                    }
                    if (m_results.size() >= 256) m_results.clear();
                }
                m_results[m_pendingKey] = m_pendingResult;
                m_pending = false;
            } else if (!m_probe.Busy()) {
                m_pending = false;
            }
        }

    public:
        bool Blocked(const Core::SDK::Game::VehicleWheelTarget& Target) {
            std::lock_guard<std::mutex> Lock(m_mutex);
            PumpLocked();
            const auto It = m_results.find(MakeKey(Target));
            return It != m_results.end() && Fresh(It->second, Target) &&
                !It->second.Clear;
        }

        void Request(CPed* Local,
            const Core::SDK::Game::VehicleWheelTarget& Target) {
            if (!Local || !Target.Vehicle) return;
            std::lock_guard<std::mutex> Lock(m_mutex);
            PumpLocked();
            if (m_pending) return;

            const Key TargetKey = MakeKey(Target);
            const auto It = m_results.find(TargetKey);
            if (It != m_results.end() && Fresh(It->second, Target)) return;

            if (!m_probe.Available() && !m_probe.Initialize()) return;
            // A ABI Steam nao aceita deslocamento 3D; sem ele o teste cairia
            // na carroceria. Deixar o pneu elegivel e mais seguro nesse caso.
            if (!m_probe.SupportsOffset()) return;

            // No dump b3258, CPed::OurPedCanSeeThisEntity soma 0.75 ao Z
            // de CEntity::m_position antes de somar vOffset.
            const D3DXVECTOR3 Offset = Target.World - Target.VehicleOrigin;
            const float Z = Offset.z - 0.75f;
            if (!std::isfinite(Offset.x) || !std::isfinite(Offset.y) ||
                !std::isfinite(Z)) return;

            if (m_probe.PostOffset(reinterpret_cast<uintptr_t>(Local),
                TargetKey.Vehicle, Offset.x, Offset.y, Z)) {
                m_pendingKey = TargetKey;
                m_pendingResult = { Target.Wheel, Target.World, false, {} };
                m_pending = true;
            }
        }
    };

    inline cVehicleWheelVisibility g_VehicleWheelVisibility;
}
