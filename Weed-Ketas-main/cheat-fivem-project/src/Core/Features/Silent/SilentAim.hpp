#pragma once

#include <Includes/Includes.hpp>
#include <Core/Offsets.hpp>
#include <Core/SDK/SDK.hpp>
#include <mutex>

namespace Core
{
    namespace Features
    {
        class cSilentAim
        {
        public:
            void HookSilent();
            void Shutdown();

        private:
            bool EnsureCave();
            bool ApplySilent(const D3DXVECTOR3& EndBulletPos,
                const D3DXVECTOR3& StartBulletPos, bool MagicBullets);
            void RestoreSilent();

            // Serializes the hook state with attachment teardown. The remote
            // caves belong to one FiveM process and must never be reused after
            // that process is replaced.
            std::mutex m_StateMutex;
        };

        inline cSilentAim g_SilentAim;
    }
}
