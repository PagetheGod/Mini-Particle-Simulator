#pragma once

#include "ParticleManager.hpp"
namespace ParticleMath
{
    // Maths that we expect to be auto-vectorized, simpler ones like gravity
    template<SIMDLevel Level>
    void SolveGravity(uint32_t StartParticleIndex, uint32_t Count, float GravityScale, float DeltaTime, ParticleStates& ParticleStateRef)
    {
        using namespace Commons;
        // Precompute scaled gravity's influences
        const float ScaledGravityInfluence = GravityScale * Constants::GRAVITY * DeltaTime;

        // Gravity pulls down in world space (negative Y direction)
        // Camera2D flips Y when converting to screen space
        for (uint32_t i = StartParticleIndex; i < StartParticleIndex + Count; i++)
        {
            ParticleStateRef.Vy[i] -= ScaledGravityInfluence;
        }
    }
    /*
     * extern template here is an "explicit instantiation DECLARATION". It tells
     * every translation unit that includes this header: do NOT compile your own
     * copy of this specialization — its object code is emitted in exactly one
     * dedicated .cpp. Without these lines, ParticleManager.cpp's runtime dispatch
     * switch would implicitly instantiate the AVX2/NEON bodies inside itself — a
     * baseline-flagged TU — which is the illegal-instruction leak we are avoiding.
     * The matching NON-extern "explicit instantiation DEFINITION"
     * (template void SolveGravity<Level>(...);) lives in ParticleMath_<ISA>.cpp,
     * each built with that ISA's flags, and that single TU actually emits the code.
     * So: one extern-template-declaration per (kernel x level) here, one explicit
     * instantiation-definition per level in the matching ISA .cpp.
     */
    extern template void SolveGravity<SIMDLevel::SSE2>(uint32_t StartParticleIndex, uint32_t Count, float GravityScale, float DeltaTime, ParticleStates& ParticleStateRef);
    extern template void SolveGravity<SIMDLevel::NEON>(uint32_t StartParticleIndex, uint32_t Count, float GravityScale, float DeltaTime, ParticleStates& ParticleStateRef);
    extern template void SolveGravity<SIMDLevel::AVX2>(uint32_t StartParticleIndex, uint32_t Count, float GravityScale, float DeltaTime, ParticleStates& ParticleStateRef);
    template<SIMDLevel Level>
    void SolveWind(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& WindInfluence, float* restrict ParticleVelX,
        float* restrict ParticleVelY, float* restrict ParticleVelZ)
    {
        // Apply precomputed wind influence to particle velocities
        for (uint32_t i = StartParticleIndex; i < StartParticleIndex + Count; i++)
        {
            ParticleVelX[i] += WindInfluence.x;
            ParticleVelY[i] += WindInfluence.y;
            ParticleVelZ[i] += WindInfluence.z;
        }
    }
    extern template void SolveWind<SIMDLevel::SSE2>(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& WindInfluence, float* restrict ParticleVelX, float* restrict ParticleVelY, float* restrict ParticleVelZ);
    extern template void SolveWind<SIMDLevel::NEON>(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& WindInfluence, float* restrict ParticleVelX, float* restrict ParticleVelY, float* restrict ParticleVelZ);
    extern template void SolveWind<SIMDLevel::AVX2>(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& WindInfluence, float* restrict ParticleVelX, float* restrict ParticleVelY, float* restrict ParticleVelZ);
    template<SIMDLevel Level>
    void SolveDrag(uint32_t StartParticleIndex, uint32_t Count, const float DragCoefficient, const float DeltaTime, float* restrict ParticleVelX,
    float* restrict ParticleVelY, float* restrict ParticleVelZ)
    {
        // Drag scales velocity down each frame. Clamp the multiplier at 0 so a
        // large coefficient can't flip the velocity sign (the MAX_DELTA_TIME cap
        // in Commons also keeps DragCoefficient * DeltaTime bounded)
        const float DragInfluence = std::max(0.f, 1.f - DragCoefficient * DeltaTime);
        for (uint32_t i = StartParticleIndex; i < StartParticleIndex + Count; i++)
        {
            ParticleVelX[i] *= DragInfluence;
            ParticleVelY[i] *= DragInfluence;
            ParticleVelZ[i] *= DragInfluence;
        }
    }
    extern template void SolveDrag<SIMDLevel::SSE2>(uint32_t StartParticleIndex, uint32_t Count, const float DragCoefficient, const float DeltaTime, float* restrict ParticleVelX, float* restrict ParticleVelY, float* restrict ParticleVelZ);
    extern template void SolveDrag<SIMDLevel::NEON>(uint32_t StartParticleIndex, uint32_t Count, const float DragCoefficient, const float DeltaTime, float* restrict ParticleVelX, float* restrict ParticleVelY, float* restrict ParticleVelZ);
    extern template void SolveDrag<SIMDLevel::AVX2>(uint32_t StartParticleIndex, uint32_t Count, const float DragCoefficient, const float DeltaTime, float* restrict ParticleVelX, float* restrict ParticleVelY, float* restrict ParticleVelZ);
    template<SIMDLevel Level>
    void UpdateParticleColor(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& StartColor,
        const glm::vec3& EndColor, ParticleStates& ParticleStateRef, float* restrict ParticleR, float* restrict ParticleG, float* restrict ParticleB)
    {
        // Lerp start->end color by each particle's relative remaining lifetime.
        // Reads LifeTime/MaxLifeTime through the struct; writes the restrict R/G/B
        // pointers so the compiler knows the three outputs don't alias
        for (uint32_t i = StartParticleIndex; i < StartParticleIndex + Count; i++)
        {
            const float ScaledLifeTime = ParticleStateRef.LifeTime[i] / ParticleStateRef.MaxLifeTime[i];
            const float InverseLifeTime = 1.f - ScaledLifeTime;
            ParticleR[i] = StartColor.r * ScaledLifeTime + EndColor.r * InverseLifeTime;
            ParticleG[i] = StartColor.g * ScaledLifeTime + EndColor.g * InverseLifeTime;
            ParticleB[i] = StartColor.b * ScaledLifeTime + EndColor.b * InverseLifeTime;
        }
    }
    extern template void UpdateParticleColor<SIMDLevel::SSE2>(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& StartColor, const glm::vec3& EndColor, ParticleStates& ParticleStateRef, float* restrict ParticleR, float* restrict ParticleG, float* restrict ParticleB);
    extern template void UpdateParticleColor<SIMDLevel::NEON>(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& StartColor, const glm::vec3& EndColor, ParticleStates& ParticleStateRef, float* restrict ParticleR, float* restrict ParticleG, float* restrict ParticleB);
    extern template void UpdateParticleColor<SIMDLevel::AVX2>(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& StartColor, const glm::vec3& EndColor, ParticleStates& ParticleStateRef, float* restrict ParticleR, float* restrict ParticleG, float* restrict ParticleB);
    template<SIMDLevel Level>
    void UpdateParticleLifeTime(uint32_t StartParticleIndex, uint32_t Count, float DeltaTime, ParticleStates& ParticleStateRef)
    {
        // Just decrement remaining lifetime. The actual kill/swap sweep runs later
        // and sequentially, so nothing here needs to branch
        for (uint32_t i = StartParticleIndex; i < StartParticleIndex + Count; i++)
        {
            ParticleStateRef.LifeTime[i] -= DeltaTime;
        }
    }
    extern template void UpdateParticleLifeTime<SIMDLevel::SSE2>(uint32_t StartParticleIndex, uint32_t Count, float DeltaTime, ParticleStates& ParticleStateRef);
    extern template void UpdateParticleLifeTime<SIMDLevel::NEON>(uint32_t StartParticleIndex, uint32_t Count, float DeltaTime, ParticleStates& ParticleStateRef);
    extern template void UpdateParticleLifeTime<SIMDLevel::AVX2>(uint32_t StartParticleIndex, uint32_t Count, float DeltaTime, ParticleStates& ParticleStateRef);
    template<SIMDLevel Level>
    void UpdateParticlePositionForAxis_Scalar(float* restrict StartParticlePtr, uint32_t Count, const float* restrict Velocity, float DeltaTime)
    {
        // Semi-implicit Euler position step for a single axis. StartParticlePtr is
        // already offset to the chunk start, so the loop is 0-based. The restrict
        // qualifiers are what let this auto-vectorize without a runtime overlap check
        for (uint32_t i = 0; i < Count; i++)
        {
            StartParticlePtr[i] += Velocity[i] * DeltaTime;
        }
    }
    extern template void UpdateParticlePositionForAxis_Scalar<SIMDLevel::SSE2>(float* restrict StartParticlePtr, uint32_t Count, const float* restrict Velocity, float DeltaTime);
    extern template void UpdateParticlePositionForAxis_Scalar<SIMDLevel::NEON>(float* restrict StartParticlePtr, uint32_t Count, const float* restrict Velocity, float DeltaTime);
    extern template void UpdateParticlePositionForAxis_Scalar<SIMDLevel::AVX2>(float* restrict StartParticlePtr, uint32_t Count, const float* restrict Velocity, float DeltaTime);

// Custom SIMD implementations, for more complex forces like point and vortex
template<SIMDLevel Level>
void SolvePointForce_Vector(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& ForcePosition,
        const float Strength, const float Radius, const float DeltaTime, ParticleStates& ParticleStateRef)
{
    using namespace Commons;
    using SIMDStruct = SIMDTraits<Level>;
    constexpr uint32_t SIMDWidth = SIMDStruct::SIMDWidth;
    // Calculate strength scaled by delta time
    const float DtStrength = Strength * DeltaTime;

    // Broadcast loop-invariant values
    auto PointPositionX = SIMDStruct::VectorizedBroadcast(ForcePosition.x);
    auto PointPositionY = SIMDStruct::VectorizedBroadcast(ForcePosition.y);
    auto PointPositionZ = SIMDStruct::VectorizedBroadcast(ForcePosition.z);
    auto StrengthVec = SIMDStruct::VectorizedBroadcast(DtStrength);
    const float InvRadius = 1.f / Radius;
    auto InvRadiusVec = SIMDStruct::VectorizedBroadcast(InvRadius);
    auto Epsilon = SIMDStruct::VectorizedBroadcast(Constants::CUSTOM_EPSILON);
    auto One = SIMDStruct::VectorizedBroadcast(1.f);
    auto Three = SIMDStruct::VectorizedBroadcast(3.f);
    auto Two = SIMDStruct::VectorizedBroadcast(2.f);
    auto Zero = SIMDStruct::VectorizedZero();

    uint32_t i = StartParticleIndex;
    for (; i + SIMDWidth <= StartParticleIndex + Count; i += SIMDWidth)
    {
        // Delta from particle to force position
        auto DeltaX = SIMDStruct::VectorizedSub(PointPositionX, SIMDStruct::VectorizedLoad(&ParticleStateRef.Px[i]));
        auto DeltaY = SIMDStruct::VectorizedSub(PointPositionY, SIMDStruct::VectorizedLoad(&ParticleStateRef.Py[i]));
        auto DeltaZ = SIMDStruct::VectorizedSub(PointPositionZ, SIMDStruct::VectorizedLoad(&ParticleStateRef.Pz[i]));

        // Distance squared
        auto DistSquare = SIMDStruct::VectorizedAdd(
            SIMDStruct::VectorizedAdd(SIMDStruct::VectorizedMul(DeltaX, DeltaX), SIMDStruct::VectorizedMul(DeltaY, DeltaY)),
            SIMDStruct::VectorizedMul(DeltaZ, DeltaZ));

        // Clamp to epsilon before rsqrt to prevent div by 0
        auto ClampedDistSquare = SIMDStruct::VectorizedMax(DistSquare, Epsilon);
        auto InvDist = SIMDStruct::VectorizedRSqrt(ClampedDistSquare);
        // Dist = 1 / InvDist, but we can also get it from DistSquare * InvDist
        auto Dist = SIMDStruct::VectorizedMul(ClampedDistSquare, InvDist);

        // Normalize direction
        DeltaX = SIMDStruct::VectorizedMul(DeltaX, InvDist);
        DeltaY = SIMDStruct::VectorizedMul(DeltaY, InvDist);
        DeltaZ = SIMDStruct::VectorizedMul(DeltaZ, InvDist);

        // Smoothstep falloff: T = clamp(1 - Dist/Radius, 0, 1), Falloff = T*T*(3 - 2*T)
        // Particles outside the radius get T=0 → Falloff=0 → no force applied
        auto T = SIMDStruct::VectorizedSub(One, SIMDStruct::VectorizedMul(Dist, InvRadiusVec));
        T = SIMDStruct::VectorizedMax(T, Zero);
        T = SIMDStruct::VectorizedMin(T, One);
        auto Falloff = SIMDStruct::VectorizedMul(
            SIMDStruct::VectorizedMul(T, T),
            SIMDStruct::VectorizedSub(Three, SIMDStruct::VectorizedMul(Two, T)));

        // Force magnitude = Strength * dt * smoothstep
        auto ForceInfluence = SIMDStruct::VectorizedMul(StrengthVec, Falloff);

        // Load velocities, apply force along normalized direction, then store back
        auto AdjustedVx = SIMDStruct::VectorizedLoad(&ParticleStateRef.Vx[i]);
        auto AdjustedVy = SIMDStruct::VectorizedLoad(&ParticleStateRef.Vy[i]);
        auto AdjustedVz = SIMDStruct::VectorizedLoad(&ParticleStateRef.Vz[i]);

        AdjustedVx = SIMDStruct::VectorizedAdd(AdjustedVx, SIMDStruct::VectorizedMul(DeltaX, ForceInfluence));
        AdjustedVy = SIMDStruct::VectorizedAdd(AdjustedVy, SIMDStruct::VectorizedMul(DeltaY, ForceInfluence));
        AdjustedVz = SIMDStruct::VectorizedAdd(AdjustedVz, SIMDStruct::VectorizedMul(DeltaZ, ForceInfluence));

        SIMDStruct::VectorizedStore(&ParticleStateRef.Vx[i], AdjustedVx);
        SIMDStruct::VectorizedStore(&ParticleStateRef.Vy[i], AdjustedVy);
        SIMDStruct::VectorizedStore(&ParticleStateRef.Vz[i], AdjustedVz);
    }
    // Scalar cleanup for remaining particles
    for (; i < StartParticleIndex + Count; i++)
    {
        float DeltaX = ForcePosition.x - ParticleStateRef.Px[i];
        float DeltaY = ForcePosition.y - ParticleStateRef.Py[i];
        float DeltaZ = ForcePosition.z - ParticleStateRef.Pz[i];

        float DistSquare = DeltaX * DeltaX + DeltaY * DeltaY + DeltaZ * DeltaZ;
        float Dist = std::sqrt(DistSquare);

        if (Dist >= Radius || Dist < Constants::CUSTOM_EPSILON)
        {
            continue;
        }

        float InverseDist = 1.f / Dist;
        DeltaX *= InverseDist;
        DeltaY *= InverseDist;
        DeltaZ *= InverseDist;

        float T = 1.f - Dist / Radius;
        float Falloff = T * T * (3.f - 2.f * T);
        float PointInfluence = DtStrength * Falloff;

        ParticleStateRef.Vx[i] += PointInfluence * DeltaX;
        ParticleStateRef.Vy[i] += PointInfluence * DeltaY;
        ParticleStateRef.Vz[i] += PointInfluence * DeltaZ;
    }
}
extern template void SolvePointForce_Vector<SIMDLevel::SSE2>(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& ForcePosition,
            const float Strength, const float Radius, const float DeltaTime, ParticleStates& ParticleStateRef);
extern template void SolvePointForce_Vector<SIMDLevel::NEON>(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& ForcePosition,
                const float Strength, const float Radius, const float DeltaTime, ParticleStates& ParticleStateRef);
extern template void SolvePointForce_Vector<SIMDLevel::AVX2>(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& ForcePosition,
            const float Strength, const float Radius, const float DeltaTime, ParticleStates& ParticleStateRef);
template<SIMDLevel Level>
void SolveVortex_Vector(uint32_t StartParticleIndex, uint32_t Count, const float VortexStrength, const float VortexPull,
    const float DeltaTime, const glm::vec3& VortexCenter, ParticleStates& ParticleStateRef)
{
    using namespace Commons;
    // SIMD version of the vortex solver
    using SIMDStruct = SIMDTraits<Level>;
    constexpr uint32_t SIMDWidth = SIMDStruct::SIMDWidth;
    const float DtStrength = VortexStrength * DeltaTime;
    const float DtPull     = VortexPull * DeltaTime;

    // Precompute and pre-load
    auto DtStrengthVec = SIMDStruct::VectorizedBroadcast(DtStrength);
    auto DtPullVec = SIMDStruct::VectorizedBroadcast(DtPull);
    auto CenterXVec = SIMDStruct::VectorizedBroadcast(VortexCenter.x);
    auto CenterZVec = SIMDStruct::VectorizedBroadcast(VortexCenter.z);

    auto Epsilon = SIMDStruct::VectorizedBroadcast(Constants::CUSTOM_EPSILON);

    uint32_t i = StartParticleIndex;
    for (; i + SIMDWidth <= StartParticleIndex + Count; i += SIMDWidth)
    {
        // Radial components
        auto RadialX = SIMDStruct::VectorizedSub(
            SIMDStruct::VectorizedLoad(&ParticleStateRef.Px[i]), CenterXVec);
        auto RadialZ = SIMDStruct::VectorizedSub(
            SIMDStruct::VectorizedLoad(&ParticleStateRef.Pz[i]), CenterZVec);

        // Distance square
        auto DistSquare = SIMDStruct::VectorizedAdd(SIMDStruct::VectorizedMul(RadialX, RadialX),
            SIMDStruct::VectorizedMul(RadialZ, RadialZ));
        DistSquare = SIMDStruct::VectorizedMax(DistSquare, Epsilon);

        // Again, this only have 12-bit precision, probably good enough for our scale
        auto InverseDist = SIMDStruct::VectorizedRSqrt(DistSquare);

        // Normalize the radial components first, so we can use normalized ones
        // To directly get normalized tangents
        RadialX = SIMDStruct::VectorizedMul(RadialX, InverseDist);
        RadialZ = SIMDStruct::VectorizedMul(RadialZ, InverseDist);

        auto TangentX = RadialZ;
        auto TangentZ = SIMDStruct::VectorizedSub(SIMDStruct::VectorizedZero(), RadialX);

        // Velocity update, tangent used for vortex strength, radial for pull
        auto DeltaVx = SIMDStruct::VectorizedSub(SIMDStruct::VectorizedMul(TangentX, DtStrengthVec),
            SIMDStruct::VectorizedMul(RadialX,  DtPullVec));
        auto DeltaVz = SIMDStruct::VectorizedSub(SIMDStruct::VectorizedMul(TangentZ, DtStrengthVec),
            SIMDStruct::VectorizedMul(RadialZ,  DtPullVec));

        // Load existing velocity, accumulate, store back
        auto AdjustedVx = SIMDStruct::VectorizedAdd(SIMDStruct::VectorizedLoad(&ParticleStateRef.Vx[i]),
            DeltaVx);
        auto AdjustedVz = SIMDStruct::VectorizedAdd(SIMDStruct::VectorizedLoad(&ParticleStateRef.Vz[i]),
            DeltaVz);
        SIMDStruct::VectorizedStore(&ParticleStateRef.Vx[i], AdjustedVx);
        SIMDStruct::VectorizedStore(&ParticleStateRef.Vz[i], AdjustedVz);
    }

    // Scalar cleanup
    for (; i < StartParticleIndex + Count; i++)
    {
        float RadialX = ParticleStateRef.Px[i] - VortexCenter.x;
        float RadialZ = ParticleStateRef.Pz[i] - VortexCenter.z;
        float DistanceSquare = RadialX * RadialX + RadialZ * RadialZ;
        DistanceSquare = std::max(DistanceSquare, Constants::CUSTOM_EPSILON);
        const float InverseDistance = 1.f / std::sqrt(DistanceSquare);

        // Normalize first, then derive tangent from the normalized radial
        RadialX *= InverseDistance;
        RadialZ *= InverseDistance;
        const float TangentX = RadialZ;
        const float TangentZ = -RadialX;

        ParticleStateRef.Vx[i] += TangentX * DtStrength - RadialX * DtPull;
        ParticleStateRef.Vz[i] += TangentZ * DtStrength - RadialZ * DtPull;
    }
}
extern template void SolveVortex_Vector<SIMDLevel::SSE2>(uint32_t StartParticleIndex, uint32_t Count, const float VortexStrength, const float VortexPull,
        const float DeltaTime, const glm::vec3& VortexCenter, ParticleStates& ParticleStateRef);
extern template void SolveVortex_Vector<SIMDLevel::NEON>(uint32_t StartParticleIndex, uint32_t Count, const float VortexStrength, const float VortexPull,
        const float DeltaTime, const glm::vec3& VortexCenter, ParticleStates& ParticleStateRef);
extern template void SolveVortex_Vector<SIMDLevel::AVX2>(uint32_t StartParticleIndex, uint32_t Count, const float VortexStrength, const float VortexPull,
        const float DeltaTime, const glm::vec3& VortexCenter, ParticleStates& ParticleStateRef);
}



