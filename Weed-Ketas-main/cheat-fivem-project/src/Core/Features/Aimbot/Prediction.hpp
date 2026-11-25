#pragma once
#include <Includes/Includes.hpp>
#include <cmath>

struct PredictionConfig
{
    float velocityMultiplier;      // Multiplicador de velocidade (stealth factor)
    float reactionTimeMs;           // Tempo de reação em milissegundos
    float smoothingFactor;          // Fator de suavização (0.0 - 1.0)
    float gravityCompensation;      // Compensação de gravidade
    float leadingFactor;            // Fator de leading (antecipação)
    bool useAcceleration;           // Usar aceleração do alvo
    bool compensateGravity;         // Compensar queda de bala
};

inline PredictionConfig MakeConfigFromLevel(int level)
{
    PredictionConfig cfg{};
    
    switch (level)
    {
        case 0: // Fácil
            cfg.velocityMultiplier = 1.0f;
            cfg.reactionTimeMs = 120.0f;
            cfg.smoothingFactor = 0.8f;
            cfg.gravityCompensation = 0.0f;
            cfg.leadingFactor = 0.8f;
            cfg.useAcceleration = false;
            cfg.compensateGravity = false;
            break;
            
        case 1: // Normal
            cfg.velocityMultiplier = 1.05f;
            cfg.reactionTimeMs = 80.0f;
            cfg.smoothingFactor = 0.6f;
            cfg.gravityCompensation = 0.1f;
            cfg.leadingFactor = 0.9f;
            cfg.useAcceleration = true;
            cfg.compensateGravity = false;
            break;
            
        case 2: // Difícil
            cfg.velocityMultiplier = 1.2f;
            cfg.reactionTimeMs = 50.0f;
            cfg.smoothingFactor = 0.35f;
            cfg.gravityCompensation = 0.3f;
            cfg.leadingFactor = 1.0f;
            cfg.useAcceleration = true;
            cfg.compensateGravity = true;
            break;
            
        case 3: // Impossível
            cfg.velocityMultiplier = 1.5f;
            cfg.reactionTimeMs = 20.0f;
            cfg.smoothingFactor = 0.05f;
            cfg.gravityCompensation = 0.5f;
            cfg.leadingFactor = 1.2f;
            cfg.useAcceleration = true;
            cfg.compensateGravity = true;
            break;
            
        default:
            cfg = MakeConfigFromLevel(1); 
            break;
    }
    
    return cfg;
}

inline float CalculateGravityDrop(float timeToHit)
{
    // GTA V gravity is approximately 9.8 m/s² (same as real world)
    constexpr float GRAVITY = 9.81f;
    return (GRAVITY * timeToHit * timeToHit) / 2.0f;
}

inline D3DXVECTOR3 ValidateVelocity(const D3DXVECTOR3& velocity)
{
    D3DXVECTOR3 result = velocity;
    
    // Verificar NaN ou Inf
    if (std::isnan(result.x) || std::isinf(result.x)) result.x = 0.0f;
    if (std::isnan(result.y) || std::isinf(result.y)) result.y = 0.0f;
    if (std::isnan(result.z) || std::isinf(result.z)) result.z = 0.0f;
    
    // Limitar velocidade máxima (segurança)
    constexpr float MAX_VELOCITY = 100.0f;
    float speed = D3DXVec3Length(&result);
    if (speed > MAX_VELOCITY)
    {
        D3DXVec3Normalize(&result, &result);
        result *= MAX_VELOCITY;
    }
    
    return result;
}

inline D3DXVECTOR3 CalculatePredictedPosition(
    float bulletSpeed,
    const D3DXVECTOR3& currentBonePos,
    const D3DXVECTOR3& camPos,
    const D3DXVECTOR3& targetVelocity,
    const D3DXVECTOR3& targetAcceleration = D3DXVECTOR3(0, 0, 0),
    int predictionLevel = 1
)
{
    PredictionConfig cfg = MakeConfigFromLevel(predictionLevel);
    D3DXVECTOR3 predictedPos = currentBonePos;
    
    // Validações iniciais
    if (bulletSpeed <= 0.1f)
        return predictedPos; // Velocidade de bala inválida
    
    D3DXVECTOR3 validVelocity = ValidateVelocity(targetVelocity);
    D3DXVECTOR3 validAcceleration = ValidateVelocity(targetAcceleration);
    
    // Calcular distância e tempo até o alvo
    D3DXVECTOR3 targetDelta = currentBonePos - camPos;
    float targetDistance = D3DXVec3Length(&targetDelta);
    
    if (targetDistance <= 0.1f)
        return predictedPos; // Distância muito pequena
    
    // Cálculo iterativo de tempo (mais preciso que linear)
    // Problema: a bala leva tempo para chegar, durante o qual o alvo se move
    float timeToHit = targetDistance / bulletSpeed;
    
    // Iteração para refinar o tempo (2 iterações são suficientes)
    for (int iter = 0; iter < 2; ++iter)
    {
        D3DXVECTOR3 futurePos = currentBonePos;
        
        // Movimento linear (velocidade)
        futurePos.x += validVelocity.x * timeToHit * cfg.leadingFactor;
        futurePos.y += validVelocity.y * timeToHit * cfg.leadingFactor;
        futurePos.z += validVelocity.z * timeToHit * cfg.leadingFactor;
        
        // Aceleração (opcional, para movimentos mais complexos)
        if (cfg.useAcceleration && (validAcceleration.x != 0 || validAcceleration.y != 0 || validAcceleration.z != 0))
        {
            float t2 = timeToHit * timeToHit;
            futurePos.x += 0.5f * validAcceleration.x * t2;
            futurePos.y += 0.5f * validAcceleration.y * t2;
            futurePos.z += 0.5f * validAcceleration.z * t2;
        }
        
        // Recalcular tempo baseado na nova posição
        D3DXVECTOR3 newDelta = futurePos - camPos;
        timeToHit = D3DXVec3Length(&newDelta) / bulletSpeed;
    }
    
    // Aplicar movimento predito
    predictedPos.x += validVelocity.x * timeToHit * cfg.leadingFactor;
    predictedPos.y += validVelocity.y * timeToHit * cfg.leadingFactor;
    predictedPos.z += validVelocity.z * timeToHit * cfg.leadingFactor;
    
    // Compensação de aceleração (se ativada)
    if (cfg.useAcceleration && (validAcceleration.x != 0 || validAcceleration.y != 0 || validAcceleration.z != 0))
    {
        float t2 = timeToHit * timeToHit;
        predictedPos.x += 0.5f * validAcceleration.x * t2;
        predictedPos.y += 0.5f * validAcceleration.y * t2;
        predictedPos.z += 0.5f * validAcceleration.z * t2;
    }
    
    // Compensação de gravidade (se ativada)
    if (cfg.compensateGravity)
    {
        float gravityDrop = CalculateGravityDrop(timeToHit);
        predictedPos.z -= gravityDrop;
    }
    
    // Aplicar smoothing factor para resultado mais realista
    D3DXVECTOR3 smoothedPrediction = currentBonePos;
    D3DXVec3Lerp(&smoothedPrediction, &currentBonePos, &predictedPos, cfg.smoothingFactor);
    
    return smoothedPrediction;
}

/**
 * @brief Versão simplificada para compatibilidade
 */
inline D3DXVECTOR3 CalculatePredictedPosition(
    float bulletSpeed,
    const D3DXVECTOR3& currentBonePos,
    const D3DXVECTOR3& camPos,
    const D3DXVECTOR3& targetVelocity,
    int predictionLevel
)
{
    return CalculatePredictedPosition(bulletSpeed, currentBonePos, camPos, targetVelocity, 
                                     D3DXVECTOR3(0, 0, 0), predictionLevel);
}