#pragma once
#include <cmath>
#include <algorithm>
#include <d3dx9math.h>

// ============================================================================
// AimMath - matematica pura do motor de mira. Sem dependencia do jogo.
//
// A camera do GTA guarda a orientacao em camera+0x40 como um VETOR DIRECAO
// UNITARIO, nao como angulos de Euler. O codigo antigo interpolava linearmente
// entre dois vetores unitarios, o que produz um vetor NAO unitario -- por isso
// o mesmo "smoothing" parecia rapido de perto e lento de longe: a velocidade
// angular efetiva virava funcao da distancia angular.
//
// Aqui a orientacao vira (yaw, pitch) em radianos, onde velocidade e
// aceleracao angular tem significado fisico, e volta para vetor so na hora de
// escrever na camera.
// ============================================================================

namespace Core::Features::AimMath
{
    inline constexpr float kPi = 3.14159265358979323846f;
    inline constexpr float kTwoPi = kPi * 2.0f;
    inline constexpr float kRad2Deg = 180.0f / kPi;
    inline constexpr float kDeg2Rad = kPi / 180.0f;

    // Orientacao angular. Convencao do GTA (Z para cima, +Y para o norte):
    //   yaw   = atan2(-x, y)
    //   pitch = asin(z)
    struct Angles
    {
        float Yaw = 0.f;
        float Pitch = 0.f;
    };

    // Normaliza para (-pi, pi]. Todo erro de yaw passa por aqui: sem isso a
    // mira daria a volta longa ao cruzar a descontinuidade.
    inline float WrapPi(float A)
    {
        A = std::fmod(A + kPi, kTwoPi);
        if (A < 0.f) A += kTwoPi;
        return A - kPi;
    }

    inline bool IsFinite(float V)
    {
        return !std::isnan(V) && !std::isinf(V);
    }

    // Vetor direcao unitario -> (yaw, pitch)
    inline Angles FromDirection(const D3DXVECTOR3& Dir)
    {
        Angles A{};

        D3DXVECTOR3 D = Dir;
        const float Len = D3DXVec3Length(&D);
        if (Len < 1e-6f || !IsFinite(Len))
            return A;

        D /= Len;

        // asin satura fora de [-1,1] por erro de arredondamento
        A.Pitch = std::asin(std::clamp(D.z, -1.0f, 1.0f));
        A.Yaw = std::atan2(-D.x, D.y);
        return A;
    }

    // (yaw, pitch) -> vetor direcao unitario. Inverso exato de FromDirection.
    inline D3DXVECTOR3 ToDirection(const Angles& A)
    {
        const float CosPitch = std::cos(A.Pitch);

        return D3DXVECTOR3(
            -std::sin(A.Yaw) * CosPitch,
            std::cos(A.Yaw) * CosPitch,
            std::sin(A.Pitch)
        );
    }

    // Erro angular pelo caminho mais curto.
    inline Angles ShortestDelta(const Angles& From, const Angles& To)
    {
        Angles D{};
        D.Yaw = WrapPi(To.Yaw - From.Yaw);
        D.Pitch = To.Pitch - From.Pitch;   // pitch nao da a volta
        return D;
    }

    // Magnitude do erro, para as bandas de precisao e a maquina de fases.
    inline float DeltaMagnitude(const Angles& D)
    {
        return std::sqrt(D.Yaw * D.Yaw + D.Pitch * D.Pitch);
    }

    // Pitch fica preso pouco antes do zenite: exatamente +-90 graus o yaw
    // perde definicao e a camera flipa.
    inline float ClampPitch(float Pitch)
    {
        constexpr float Limit = 89.0f * kDeg2Rad;
        return std::clamp(Pitch, -Limit, Limit);
    }

    // Satura um vetor 2D preservando a direcao. Usado nos orcamentos de
    // velocidade e aceleracao: limitar cada eixo em separado distorceria a
    // trajetoria em diagonal.
    inline void ClampMagnitude(float& X, float& Y, float Max)
    {
        if (Max <= 0.f) return;

        const float M = std::sqrt(X * X + Y * Y);
        if (M <= Max || M < 1e-9f) return;

        const float S = Max / M;
        X *= S;
        Y *= S;
    }

    // Curva suave 0->1 usada para misturar ganhos entre bandas sem degrau.
    inline float SmoothStep(float Edge0, float Edge1, float X)
    {
        if (Edge1 <= Edge0) return X < Edge0 ? 0.f : 1.f;
        const float T = std::clamp((X - Edge0) / (Edge1 - Edge0), 0.f, 1.f);
        return T * T * (3.0f - 2.0f * T);
    }
}
