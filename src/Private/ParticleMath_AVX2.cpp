#include "ParticleMath_Shared.h"

/*
 * x86-only TU. SIMDTraits<AVX2> is defined only under this guard in SIMD.hpp, so
 * on ARM this whole file compiles to an empty object (NEON has its own file).
 * Built with -mavx2 -mfma (see CMakeLists) so both the hand-written AVX2
 * intrinsics AND the auto-vectorized simple kernels emit 256-bit code here.
 */
#if defined(__x86_64__) || defined(_M_X64)
namespace ParticleMath
{
    // Simple auto-vectorized kernels. The body ignores Level, but instantiating
    // them in THIS TU (under -mavx2) is what makes the auto-vectorizer target AVX2.
    template void SolveGravity<SIMDLevel::AVX2>(uint32_t StartParticleIndex, uint32_t Count, float GravityScale, float DeltaTime, ParticleStates& ParticleStateRef);
    template void SolveWind<SIMDLevel::AVX2>(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& WindInfluence, float* restrict ParticleVelX, float* restrict ParticleVelY, float* restrict ParticleVelZ);
    template void SolveDrag<SIMDLevel::AVX2>(uint32_t StartParticleIndex, uint32_t Count, const float DragCoefficient, const float DeltaTime, float* restrict ParticleVelX, float* restrict ParticleVelY, float* restrict ParticleVelZ);
    template void UpdateParticleColor<SIMDLevel::AVX2>(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& StartColor, const glm::vec3& EndColor, ParticleStates& ParticleStateRef, float* restrict ParticleR, float* restrict ParticleG, float* restrict ParticleB);
    template void UpdateParticleLifeTime<SIMDLevel::AVX2>(uint32_t StartParticleIndex, uint32_t Count, float DeltaTime, ParticleStates& ParticleStateRef);
    template void UpdateParticlePositionForAxis_Scalar<SIMDLevel::AVX2>(float* restrict StartParticlePtr, uint32_t Count, const float* restrict Velocity, float DeltaTime);
    // Custom SIMD kernels — these genuinely dispatch through SIMDTraits<AVX2> intrinsics
    template void SolvePointForce_Vector<SIMDLevel::AVX2>(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& ForcePosition, const float Strength,
        const float Radius, const float DeltaTime, ParticleStates& ParticleStateRef);
    template void SolveVortex_Vector<SIMDLevel::AVX2>(uint32_t StartParticleIndex, uint32_t Count, const float VortexStrength,
        const float VortexPull, const float DeltaTime, const glm::vec3& VortexCenter, ParticleStates& ParticleStateRef);
}
#endif
