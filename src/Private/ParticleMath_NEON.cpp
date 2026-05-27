#include "ParticleMath_Shared.h"

/*
 * AArch64-only TU. SIMDTraits<NEON> is defined only under this guard in SIMD.hpp,
 * so on x86 this whole file compiles to an empty object. NEON is mandatory on
 * ARMv8-A, so no special compile flag is needed (there is no -mavx2 equivalent to
 * gate an optional vector ISA).
 *
 * See ParticleMath_SSE2.cpp for the explanation of what "template void Foo<Level>(...);"
 * (explicit instantiation definition) does and how it pairs with the header's
 * "extern template" declarations.
 */
#if defined(__aarch64__) || defined(_M_ARM64)
namespace ParticleMath
{
    // Simple auto-vectorized kernels (NEON = 128-bit, 4 floats per iteration)
    template void SolveGravity<SIMDLevel::NEON>(uint32_t StartParticleIndex, uint32_t Count, float GravityScale, float DeltaTime, ParticleStates& ParticleStateRef);
    template void SolveWind<SIMDLevel::NEON>(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& WindInfluence, float* restrict ParticleVelX, float* restrict ParticleVelY, float* restrict ParticleVelZ);
    template void SolveDrag<SIMDLevel::NEON>(uint32_t StartParticleIndex, uint32_t Count, const float DragCoefficient, const float DeltaTime, float* restrict ParticleVelX, float* restrict ParticleVelY, float* restrict ParticleVelZ);
    template void UpdateParticleColor<SIMDLevel::NEON>(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& StartColor, const glm::vec3& EndColor, ParticleStates& ParticleStateRef, float* restrict ParticleR, float* restrict ParticleG, float* restrict ParticleB);
    template void UpdateParticleLifeTime<SIMDLevel::NEON>(uint32_t StartParticleIndex, uint32_t Count, float DeltaTime, ParticleStates& ParticleStateRef);
    template void UpdateParticlePositionForAxis<SIMDLevel::NEON>(float* restrict StartParticlePtr, uint32_t Count, const float* restrict Velocity, float DeltaTime);
    // Custom SIMD kernels — these dispatch through SIMDTraits<NEON> intrinsics
    template void SolvePointForce<SIMDLevel::NEON>(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& ForcePosition, const float Strength,
        const float Radius, const float DeltaTime, ParticleStates& ParticleStateRef);
    template void SolveVortex<SIMDLevel::NEON>(uint32_t StartParticleIndex, uint32_t Count, const float VortexStrength,
        const float VortexPull, const float DeltaTime, const glm::vec3& VortexCenter, ParticleStates& ParticleStateRef);
}
#endif
