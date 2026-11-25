#pragma once
#include "AimMath.hpp"
#include <chrono>

// ============================================================================
// AimEngine - o mesmo aimbot de sempre, com Smooth, mas com o movimento
// resolvido direito por baixo.
//
// O que mudou em relacao a versao antiga:
//
//   1. A mira tem VELOCIDADE. O motor escreve aceleracao, nao posicao. E o
//      que faz a chegada no alvo ser suave de verdade em vez de ir frenando
//      em degraus.
//
//   2. Trabalha em (yaw, pitch). O campo em camera+0x40 e um vetor direcao
//      unitario; interpolar dois unitarios linearmente da um vetor NAO
//      unitario, e era por isso que o mesmo Smooth parecia rapido de perto e
//      lento de longe.
//
//   3. Passo fixo de 240 Hz. Antes o loop girava sem medir tempo, entao o
//      comportamento mudava com a carga da maquina.
//
//   4. Os dois eixos recebem o mesmo tratamento. Na versao antiga so .x e .y
//      passavam pelo Smooth -- o .z ia direto, ou seja, a mira VERTICAL dava
//      snap instantaneo enquanto a horizontal suavizava.
// ============================================================================

namespace Core::Features
{
    // Parametros do movimento. X e Y tem frequencia propria: da para deixar
    // o horizontal solto e o vertical travado, ou o contrario.
    struct AimParams
    {
        float FreqYaw = 16.0f;   // X / horizontal
        float FreqPitch = 16.0f;   // Y / vertical
        float Damping = 1.0f;    // z = 1 -> chega no menor tempo sem passar
        float MaxVelDegPerSec = 900.0f;
        float MaxAccelDegPerSec2 = 7000.0f;
    };

    // Traduz um slider Smooth (0-100) em frequencia.
    //
    // A curva e ancorada em tres pontos de referencia, interpolando em escala
    // LOGARITMICA entre eles -- em escala linear a metade de baixo do slider
    // ficaria toda amontoada no "muito forte".
    //
    //     0  -> w 250   absurdo. trava no alvo praticamente instantaneo
    //    36  -> w  32   forte. da para girar, matar rolando, highlight
    //    77  -> w  10   legit. sutil o bastante para nao chamar atencao
    //   100  -> w   6   bem lento
    inline float FreqFromSmooth(int Smooth)
    {
        const float S = (float)std::clamp(Smooth, 0, 100);

        // Interpolacao geometrica: a razao entre os extremos e o que se
        // mantem constante, nao a diferenca.
        auto LerpLog = [](float A, float B, float T) {
            return A * std::pow(B / A, std::clamp(T, 0.f, 1.f));
            };

        if (S <= 36.0f)  return LerpLog(250.0f, 32.0f, S / 36.0f);
        if (S <= 77.0f)  return LerpLog(32.0f, 10.0f, (S - 36.0f) / 41.0f);
        return LerpLog(10.0f, 6.0f, (S - 77.0f) / 23.0f);
    }

    inline AimParams ParamsFromSmooth(int SmoothX, int SmoothY)
    {
        AimParams P{};

        P.FreqYaw = FreqFromSmooth(SmoothX);
        P.FreqPitch = FreqFromSmooth(SmoothY);

        // Amortecimento critico: chegar rapido SEM passar do ponto.
        P.Damping = 1.0f;

        // Os orcamentos acompanham a frequencia. Se fossem fixos, seriam eles
        // -- e nao o slider -- a decidir o teto na ponta forte da escala, e o
        // Smooth 0 nunca conseguiria ser absurdo.
        const float W = std::max(P.FreqYaw, P.FreqPitch);
        P.MaxVelDegPerSec = W * 60.0f;
        P.MaxAccelDegPerSec2 = W * W * 30.0f;

        return P;
    }

    class AimEngine
    {
    public:
        // 480 Hz. O Euler semi-implicito e estavel enquanto w*dt < 2; no
        // Smooth 0 (w = 250) isso da 0.52, com folga. A 240 Hz ficaria em
        // 1.04 -- ainda estavel, mas perto demais da borda.
        static constexpr float kFixedStep = 1.0f / 480.0f;

        // Teto de acumulo. Se a thread travou, descartamos o excedente em vez
        // de rodar milhares de substeps de uma vez.
        static constexpr float kMaxAccum = 0.10f;

    private:
        AimMath::Angles m_Angles{};
        float m_VelYaw = 0.f;
        float m_VelPitch = 0.f;

        float m_Accum = 0.f;
        bool  m_Active = false;

        std::chrono::steady_clock::time_point m_LastTick{};
        bool m_HasTick = false;

    public:
        // Sincroniza com a camera real ao engajar: o jogador mexeu a mira
        // enquanto estavamos desligados.
        void Engage(const AimMath::Angles& CurrentView)
        {
            if (!m_Active)
            {
                m_Angles = CurrentView;
                m_VelYaw = 0.f;
                m_VelPitch = 0.f;
                m_Accum = 0.f;
                m_Active = true;
                m_HasTick = false;
            }
        }

        void Release()
        {
            m_Active = false;
            m_VelYaw = 0.f;
            m_VelPitch = 0.f;
        }

        bool Active() const { return m_Active; }

        // Tempo real decorrido. Devolve 0 no primeiro tick.
        float Delta()
        {
            const auto Now = std::chrono::steady_clock::now();

            if (!m_HasTick)
            {
                m_LastTick = Now;
                m_HasTick = true;
                return 0.f;
            }

            const float Dt = std::chrono::duration<float>(Now - m_LastTick).count();
            m_LastTick = Now;

            return (Dt > 0.f && Dt < 1.0f) ? Dt : 0.f;
        }

        AimMath::Angles Tick(const AimMath::Angles& CurrentView,
            const AimMath::Angles& Target,
            const AimParams& P,
            float Dt)
        {
            // O jogador continua dono da camera: adotamos a posicao dele e
            // mantemos a nossa velocidade, para o motor nao brigar com o mouse.
            m_Angles = CurrentView;

            if (Dt <= 0.f)
                return m_Angles;

            m_Accum += Dt;
            if (m_Accum > kMaxAccum) m_Accum = kMaxAccum;

            const float MaxVel = P.MaxVelDegPerSec * AimMath::kDeg2Rad;
            const float MaxAcc = P.MaxAccelDegPerSec2 * AimMath::kDeg2Rad;

            while (m_Accum >= kFixedStep)
            {
                m_Accum -= kFixedStep;
                Integrate(Target, P, MaxVel, MaxAcc);
            }

            return m_Angles;
        }

    private:
        void Integrate(const AimMath::Angles& Target, const AimParams& P,
            float MaxVel, float MaxAcc)
        {
            constexpr float H = kFixedStep;

            const AimMath::Angles E = AimMath::ShortestDelta(m_Angles, Target);

            const float Wy = P.FreqYaw;     // X / horizontal
            const float Wp = P.FreqPitch;   // Y / vertical
            const float Z = P.Damping;

            // Mola amortecida, uma por eixo:
            //     a = w^2 * erro  -  2*z*w * velocidade
            // O primeiro termo puxa para o alvo, o segundo freia. Com z = 1 a
            // mira chega no menor tempo possivel sem passar do ponto.
            float AccYaw = Wy * Wy * E.Yaw - 2.0f * Z * Wy * m_VelYaw;
            float AccPitch = Wp * Wp * E.Pitch - 2.0f * Z * Wp * m_VelPitch;

            // Saturado como vetor: limitar cada eixo em separado torceria a
            // trajetoria nas diagonais.
            AimMath::ClampMagnitude(AccYaw, AccPitch, MaxAcc);

            // Euler semi-implicito: integra a velocidade primeiro e usa a
            // velocidade nova na posicao. Bem mais estavel para molas.
            m_VelYaw += AccYaw * H;
            m_VelPitch += AccPitch * H;

            AimMath::ClampMagnitude(m_VelYaw, m_VelPitch, MaxVel);

            m_Angles.Yaw = AimMath::WrapPi(m_Angles.Yaw + m_VelYaw * H);
            m_Angles.Pitch = AimMath::ClampPitch(m_Angles.Pitch + m_VelPitch * H);

            // Pitch no limite: zera a velocidade nesse eixo, senao ela
            // acumularia indefinidamente contra a parede.
            constexpr float Limit = 89.0f * AimMath::kDeg2Rad;
            if (std::abs(m_Angles.Pitch) >= Limit - 1e-5f)
                m_VelPitch = 0.f;

            // Um unico NaN escrito na camera trava a visao do jogo ate
            // reiniciar. Nunca deixa passar.
            if (!AimMath::IsFinite(m_Angles.Yaw) || !AimMath::IsFinite(m_Angles.Pitch) ||
                !AimMath::IsFinite(m_VelYaw) || !AimMath::IsFinite(m_VelPitch))
            {
                m_VelYaw = 0.f;
                m_VelPitch = 0.f;
                m_Angles = Target;
            }
        }
    };
}
