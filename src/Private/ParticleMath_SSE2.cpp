#include "ParticleMath_Shared.h"


// This .h/.cpp pair provides explicit instantiation for SSE2 particle math
/*
 * x86-only TU, baseline SSE2 — no special compile flag is needed because SSE2 is
 * part of the x86-64 baseline. SIMDTraits<SSE2> exists only under this guard, so
 * this file compiles to an empty object on ARM.
 */
#if defined(__x86_64__) || defined(_M_X64)
namespace ParticleMath
{
    /*
     * SYNTAX NOTE — what "template void SolveGravity<SIMDLevel::SSE2>(...);" means
     * (the thing to come back and re-read):
     *
     * This is an explicit instantiation DEFINITION. The keyword "template" with no
     * angle brackets after it tells the compiler: "take the EXISTING generic body
     * from ParticleMath_Shared.h and emit the machine code for the argument SSE2,
     * right here in this translation unit." Same body as the template — we are not
     * changing behavior, just forcing one concrete copy to be generated in a TU
     * compiled with this ISA's flags (here: plain SSE2 baseline).
     *
     * It is the matched partner of "extern template void SolveGravity<SSE2>(...);"
     * in the header, which is the explicit instantiation DECLARATION: "DO NOT emit
     * this in any TU that includes the header." The header suppresses everywhere →
     * this file emits in exactly one place → AVX2/NEON instructions can never leak
     * into baseline code that runs before the runtime CPU check.
     *
     * Do NOT confuse this with "template<> void SolveGravity<SSE2>(...)". Those empty
     * angle brackets after "template" make it an explicit SPECIALIZATION — a SEPARATE,
     * hand-written body just for SSE2 (a different algorithm). We don't want that, and
     * writing it here would also error with "explicit specialization after
     * instantiation", because the header's extern template already committed this
     * specialization to the generic body.
     *
     * Mnemonic: "template" (bare) = stamp out the existing mold. "template<>" = carve
     * a brand-new special case. "extern template" = make it somewhere else, not here.
     */

    // Simple auto-vectorized kernels
    template void SolveGravity<SIMDLevel::SSE2>(uint32_t StartParticleIndex, uint32_t Count, float GravityScale, float DeltaTime, ParticleStates& ParticleStateRef);
    template void SolveWind<SIMDLevel::SSE2>(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& WindInfluence, float* restrict ParticleVelX, float* restrict ParticleVelY, float* restrict ParticleVelZ);
    template void SolveDrag<SIMDLevel::SSE2>(uint32_t StartParticleIndex, uint32_t Count, const float DragCoefficient, const float DeltaTime, float* restrict ParticleVelX, float* restrict ParticleVelY, float* restrict ParticleVelZ);
    template void UpdateParticleColor<SIMDLevel::SSE2>(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& StartColor, const glm::vec3& EndColor, ParticleStates& ParticleStateRef, float* restrict ParticleR, float* restrict ParticleG, float* restrict ParticleB);
    template void UpdateParticleLifeTime<SIMDLevel::SSE2>(uint32_t StartParticleIndex, uint32_t Count, float DeltaTime, ParticleStates& ParticleStateRef);
    template void UpdateParticlePositionForAxis<SIMDLevel::SSE2>(float* restrict StartParticlePtr, uint32_t Count, const float* restrict Velocity, float DeltaTime);
    // Custom SIMD kernels, these dispatch through SIMDTraits<SSE2> intrinsics
    template void SolvePointForce<SIMDLevel::SSE2>(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& ForcePosition, const float Strength,
        const float Radius, const float DeltaTime, ParticleStates& ParticleStateRef);
    template void SolveVortex<SIMDLevel::SSE2>(uint32_t StartParticleIndex, uint32_t Count, const float VortexStrength,
        const float VortexPull, const float DeltaTime, const glm::vec3& VortexCenter, ParticleStates& ParticleStateRef);
}
#endif
