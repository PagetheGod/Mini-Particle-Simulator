//
// Created by YWvin on 2026/3/23.
//

#include "ParticleManager.hpp"
#include <cstdlib>
#include <iostream>

#include "glm/gtx/common.inl"

void ParticleManager::InitializeParticles()
{
    // Allocate the aligned arrays for all particle states
    // We are using 64-byte alignment to account for all three SIMD alignment requirements
    // SSE2 - 16, AVX2 - 32, AVX512 - 64 bytes. All match their vector register width
    m_ParticleStates.Px = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.Py = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.Pz = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.Vx = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.Vy = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.Vz = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.R = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.G = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.B = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.Size = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.LifeTime = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.MaxLifeTime = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
}

void ParticleManager::ParticleFrame(const float DeltaTime, const ParticleSimulatorConfig& Config,
    const bool IsConfigDirty) {
    // If config is dirty(user changed things), killed all particles, restart render
    // Reset all state trackers
    if (IsConfigDirty)
    {
        m_ParticleCount = 0;
        m_TimeSinceLastWind = 0.f;
        m_TimeSinceLastBurst = 0.f;
        m_BurstInterval = 0.f;
    }
}



void ParticleManager::SpawnParticles(const ParticleSimulatorConfig& Config, const bool IsConfigDirty,
    const float DeltaTime)
{
    // We are already at limit, skip spawning for this frame
    if (m_ParticleCount >= NUM_MAX_PARTICLES)
    {
        return;
    }
    uint32_t NumParticleSpawn = 0;
    if (Config.Mode == EmitterMode::Burst)
    {
        // Are we there to burst?
        m_TimeSinceLastBurst += DeltaTime;
        if (m_TimeSinceLastBurst >= Config.BurstInterval)
        {
            // Calculate how many particles we got to spawn, rounding down
            NumParticleSpawn += std::floor(static_cast<float>(Config.EmissionRate) * DeltaTime);
        }
        // Spawn all the particles or until we run into max particle count
        for (uint32_t i = m_ParticleCount; i < NUM_MAX_PARTICLES && i < m_ParticleCount + NumParticleSpawn; i++)
        {
            // Spawning is pretty easy, just set the vals according to the config
        }
    }
    else
    {

    }
}

void ParticleManager::UpdateParticles(float DeltaTime, const bool IsConfigDirty)
{
    // Call all the particle update functions in order
    // As of right now, we will update drag before any other forces kick in

    // Before we start, calculate the number of particles each thread has to process
    const uint32_t NumParticlesPerThread = std::ceil(static_cast<float>(m_ParticleCount)
        / static_cast<float>(NUM_THREADS_USED));

}

void ParticleManager::SolveGravity(uint32_t StartParticleIndex, uint32_t Count, float GravityScale, float DeltaTime)
{
    // Precompute scaled gravity's influences
    const float ScaledGravityInfluence = GravityScale * Commons::Constants::GRAVITY * DeltaTime;

    // Gravity only affects Vy (vertical axis)
    for (uint32_t i = StartParticleIndex; i < StartParticleIndex + Count; i++)
    {
        m_ParticleStates.Vy[i] -= ScaledGravityInfluence;
    }
}

glm::vec3 ParticleManager::ComputeWindInfluence(const float Strength, const glm::vec3& Direction,
    const float Period, const float DeltaTime)
{
    // Update wind timer — called once per frame, before dispatching SolveWind to threads
    m_TimeSinceLastWind += DeltaTime;
    // Wrap to [0, Period) so this number doesn't grow forever
    constexpr float TWO_PI = glm::radians(360.f);
    m_TimeSinceLastWind = glm::mod(m_TimeSinceLastWind, Period);
    const float SinTime = m_TimeSinceLastWind * (TWO_PI / Period);
    return Direction * Strength * glm::sin(SinTime) * DeltaTime;
}

void ParticleManager::SolveWind(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& WindInfluence)
{
    // Apply precomputed wind influence to particle velocities
    for (uint32_t i = StartParticleIndex; i < StartParticleIndex + Count; i++)
    {
        m_ParticleStates.Vx[i] += WindInfluence.x;
        m_ParticleStates.Vy[i] += WindInfluence.y;
        m_ParticleStates.Vz[i] += WindInfluence.z;
    }
}

void ParticleManager::SolveDrag(uint32_t StartParticleIndex, uint32_t Count, const float DragCoefficient,
    const float DeltaTime)
{
    /*
     * In terms of drag, since it's relative to our current speed, there are two ways of doing it using our method:
     * 1. Solve it BEFORE any other force affect our speed
     * 2. Solve it AFTER any other force affect our speed
     * The difference probably wouldn't be huge
     * The more accurate method will be to do some integration over time but too bad no time to make it work
     */

    // Precompute the drag coefficient times delta time
    // Would have to clamp here if we did not provide a max frame time in Commons
    const float DragInfluence = (1.f - DragCoefficient * DeltaTime);

    for (uint32_t i = StartParticleIndex; i < StartParticleIndex + Count; i++)
    {
        // We need to multiply each velocity by the drag influence
        m_ParticleStates.Vx[i] *= DragInfluence;
        m_ParticleStates.Vy[i] *= DragInfluence;
        m_ParticleStates.Vz[i] *= DragInfluence;
    }
}

void ParticleManager::SolveVortex(uint32_t StartParticleIndex, uint32_t Count, const float VortexStrength,
    const float VortexPull, const float DeltaTime, const glm::vec3& VortexCenter)
{
    /*
     * This is the trickiest force to solve. Details:
     * 1. It needs two components, one from the particle to the center of the vortex, this is the radial component,
     * it pulls the particle towards the center of the vortex;
     * The second component is the one that's tangent to the orbit of particle around the vortex, unsurprisingly,
     * this is the tangential component
     * 2. The radial component is easy to deal with. However, the tangential component would require a cross product between
     * the radial component and the axis of the vortex center. This is difficult to parallelize.
     * 3. Therefore, we fixed the vortex's center axis to be the Y-axis(Up in 3D), this allows us to get the tangential component by:
     * simply do (x, y, z) -> (z, 0, -x), the two's dot product equals 0, nice!
     * To see this, take a look at the cross product of j x (a, b, c)
     * | i  j  k |
     * | 0  1  0 | = (c, 0, -a), isn't that beautiful?
     * | a  b  c |
     */
    const float DtStrength = VortexStrength * DeltaTime;
    const float DtPull = VortexPull * DeltaTime;
    for (uint32_t i = StartParticleIndex; i < StartParticleIndex + Count; i++)
    {
        // Calculate the x and z components of vortex center to particle
        float RadialX = m_ParticleStates.Px[i] - VortexCenter.x;
        float RadialZ = m_ParticleStates.Pz[i] - VortexCenter.z;
        // Calculate the inverse distance, for normalization
        const float DistanceSquare = RadialX * RadialX + RadialZ * RadialZ;
        const float Distance = std::max(glm::sqrt(DistanceSquare), glm::epsilon<float>());
        const float InverseDistance = 1.f / Distance;
        // Get the tangential components, normalize everything
        float TangentX = RadialZ;
        float TangentZ = -RadialX;
        TangentX *= InverseDistance;
        TangentZ *= InverseDistance;
        RadialX *= InverseDistance;
        RadialZ *= InverseDistance;
        // Scale tangents and radial vectors with the forces and add them to velocity
        m_ParticleStates.Vx[i] += (TangentX * DtStrength + RadialX * DtPull);
        m_ParticleStates.Vz[i] += (TangentZ * DtStrength + RadialZ * DtPull);
    }
}

void ParticleManager::SolvePointForce(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& ForcePosition,
    const float Strength, const float DeltaTime)
{
    /*
     * Calculate the point attractor/repulsor force
     * This is a relatively tricky one. Steps:
     * 1. Calculate the vector between the point and the particle, take its length sqaure
     * 2. Factor in the inverse square falloff
     * 3. Calculate the final influence: force * dt
     * 4. This might not get vectorized by the compilers, need to check disassembly
     * Maybe there is some compiler flags we can use to check for that?
     */
    const float DtStrength = Strength * DeltaTime;
    for (uint32_t i = StartParticleIndex; i < StartParticleIndex + Count; i++)
    {
        // Get the three different components
        float PointParticleX = m_ParticleStates.Px[i] - ForcePosition.x;
        float PointParticleY = m_ParticleStates.Py[i] - ForcePosition.y;
        float PointParticleZ = m_ParticleStates.Pz[i] - ForcePosition.z;
        // Calculate distance square
        float DistanceSquare = PointParticleX * PointParticleX + PointParticleY * PointParticleY +
            PointParticleZ * PointParticleZ;
        // Get the inverse of distance, use an epsilon to avoid 0-related explosions
        DistanceSquare = std::max(DistanceSquare, glm::epsilon<float>());
        const float InverseDistanceSquare = 1.f / DistanceSquare;
        const float InverseDistance = glm::sqrt(InverseDistanceSquare);
        // Normalize
        PointParticleX *= InverseDistance;
        PointParticleY *= InverseDistance;
        PointParticleZ *= InverseDistance;
        // Get the final influence by multiplying the force with fall off and dt
        const float PointInfluence = InverseDistanceSquare * DtStrength;
        // Add to the velocity components
        m_ParticleStates.Vx[i] -= PointInfluence * PointParticleX;
        m_ParticleStates.Vy[i] -= PointInfluence * PointParticleY;
        m_ParticleStates.Vz[i] -= PointInfluence * PointParticleZ;

    }
}

void ParticleManager::UpdateParticleLifeTime(uint32_t StartParticleIndex, uint32_t Count, float DeltaTime)
{
    // Easy update, just decrement delta time from all particles
    for (uint32_t i = StartParticleIndex; i < StartParticleIndex + Count; i++)
    {
        m_ParticleStates.LifeTime[i] -= DeltaTime;
    }
}

void ParticleManager::KillParticle(const uint32_t KillIndex) {
    /*
     * Function to kill a single particle, this could be invoked by kill-Y ar life time expirations
     * Basically, this swaps the dead particle with the last alive particle
     * We go from the back to front to make the logics cleaner
     */

    // Sanity check - skip if we have no particle at all or index out of bounds
    if (m_ParticleCount == 0 || KillIndex >= m_ParticleCount)
    {
        return;
    }

    const uint32_t LastAliveIndex = m_ParticleCount - 1;
    // Only go through with the swap if we are not the last, otherwise just decrement alive count
    if (KillIndex != LastAliveIndex)
    {
        // Positions
        std::swap(m_ParticleStates.Px[KillIndex], m_ParticleStates.Px[LastAliveIndex]);
        std::swap(m_ParticleStates.Py[KillIndex], m_ParticleStates.Py[LastAliveIndex]);
        std::swap(m_ParticleStates.Pz[KillIndex], m_ParticleStates.Pz[LastAliveIndex]);
        // Velocities
        std::swap(m_ParticleStates.Vx[KillIndex], m_ParticleStates.Vx[LastAliveIndex]);
        std::swap(m_ParticleStates.Vy[KillIndex], m_ParticleStates.Vy[LastAliveIndex]);
        std::swap(m_ParticleStates.Vz[KillIndex], m_ParticleStates.Vz[LastAliveIndex]);
        // Colors
        std::swap(m_ParticleStates.R[KillIndex], m_ParticleStates.R[LastAliveIndex]);
        std::swap(m_ParticleStates.G[KillIndex], m_ParticleStates.G[LastAliveIndex]);
        std::swap(m_ParticleStates.B[KillIndex], m_ParticleStates.B[LastAliveIndex]);
        // Size, Lifetime, MaxLifeTime
        std::swap(m_ParticleStates.Size[KillIndex], m_ParticleStates.Size[LastAliveIndex]);
        std::swap(m_ParticleStates.LifeTime[KillIndex], m_ParticleStates.LifeTime[LastAliveIndex]);
        std::swap(m_ParticleStates.MaxLifeTime[KillIndex], m_ParticleStates.MaxLifeTime[LastAliveIndex]);
    }
    m_ParticleCount--;
}

// Function that will update one component of particle positions
// We pass it the component position array ptr, and the component velocity array ptr
void ParticleManager::UpdateParticlePositionForAxis_Scalar(float* StartParticlePtr, uint32_t Count,
    const float *Velocity, float DeltaTime)
{
    for (uint32_t i = 0; i < Count; i++)
    {
        StartParticlePtr[i] += Velocity[i] * DeltaTime;
    }
}

void ParticleManager::CheckParticleLifeTime()
{
    // Go through all the particles, check their lifetime, if below 0, kill
    // This cast should be avoided in general, in our case this is fine since our
    // Particle count should never go over 100k
    for (int i = static_cast<int>(m_ParticleCount); i >= 0; i--)
    {
        if (m_ParticleStates.LifeTime[i] <= 0.f)
        {
            KillParticle(i);
        }
    }
}

void ParticleManager::SpawnParticles_Sphere(const ParticleSimulatorConfig& Config, const float DeltaTime)
{

}

void ParticleManager::SpawnParticles_BoxPlane(const ParticleSimulatorConfig& Config, const float DeltaTime)
{
}

void ParticleManager::SpawnParticles_RingDisc(const ParticleSimulatorConfig& Config, const float DeltaTime)
{
}

void ParticleManager::SpwanParticles_Cylinder(const ParticleSimulatorConfig& Config, const float DeltaTime)
{
}

void ParticleManager::UpdateParticleColor(const uint32_t StartParticleIndex, const uint32_t Count,
                                          const glm::vec3& StartColor, const glm::vec3& EndColor)
{
    for (uint32_t i = StartParticleIndex; i < StartParticleIndex + Count; i++)
    {
        // Lerp between start and end colors using the relative life time of each particle
        const float ScaledLifeTime = m_ParticleStates.LifeTime[i] / m_ParticleStates.MaxLifeTime[i];
        const float InverseLifeTime = 1.f - ScaledLifeTime;
        m_ParticleStates.R[i] = StartColor.r * ScaledLifeTime + EndColor.r * InverseLifeTime;
        m_ParticleStates.G[i] = StartColor.g * ScaledLifeTime + EndColor.g * InverseLifeTime;
        m_ParticleStates.B[i] = StartColor.b * ScaledLifeTime + EndColor.b * InverseLifeTime;
    }
}

template<SIMDLevel Level>
void ParticleManager::UpdateParticlePositionForAxis(float *StartParticlePtr, uint32_t Count, const float *Velocity,
    float DeltaTime)
{
    using SIMDStruct = SIMDTraits<Level>;
    // Broadcast delta time to all lanes
    // This is auto because... it depends on what functions are generated
    // In other words, I have no idea what the type is lmao
    auto BroadcastDt = SIMDStruct::VectorizedBroadcast(DeltaTime);

    // Main loop with SIMD
    // We process until the REMAINING number of particles are less than the SIMD WIDTH
    // We declare the loop counter outside because we need it to process the remainders
    uint32_t i = 0;
    // Note the loop condition, imagine we are using SSE2(4 floats) and we got 4 particles
    // 0 + 4 == count, this means we will enter the SIMD loop and process the 4 floats, correct
    // if we did < count, 0 + 4 !< count, we would have done scalar codes incorrectly
    for (; i + SIMDStruct::WIDTH <= Count; i += SIMDStruct::WIDTH)
    {
        // Load particle positions, number of particle processed = width
        auto ParticlePos = SIMDStruct::VectorizedLoad(&StartParticlePtr[i]);
        // Load velocities
        auto ParticleVel = SIMDStruct::VectorizedLoad(&Velocity[i]);
        // We then multiply velocities by delta time
        auto ParticleDisplacement = SIMDStruct::VectorizedMul(ParticleVel, BroadcastDt);
        // Then add the displacements to positions
        auto NewParticlePos = SIMDStruct::VectorizedAdd(ParticlePos, ParticleDisplacement);
        // Don't forget to store the results back!
        SIMDStruct::VectorizedStore(&StartParticlePtr[i], NewParticlePos);
    }

    // Pick up the remainder
    for (; i < Count; i++)
    {
        StartParticlePtr[i] += Velocity[i] * DeltaTime;
    }
}

template<SIMDLevel Level>
void ParticleManager::UpdateParticleLifeTime(float *StartParticlePtr, uint32_t Count, float DeltaTime)
{
    using SIMDStruct = SIMDTraits<Level>;

    // Again, broadcast delta time
    auto BroadcastDt = SIMDStruct::VectorizedBroadcast(DeltaTime);

    uint32_t i = 0;
    // This main loop is much easier, just subtract delta time from all the particles
    // Hmmm, looking at the actual logics, there's a pretty big chance that
    // Compilers will auto vectorize this code and it will probably does a better job than I do
    for (; i + SIMDStruct::VectorSize <= Count; i += SIMDStruct::SIMDWidth)
    {
        auto ParticleLifeTime = SIMDStruct::VectorizedLoad(&StartParticlePtr[i]);
        auto NewLifeTime = SIMDStruct::VectorizedSub(ParticleLifeTime, BroadcastDt);
        SIMDStruct::VectorizedStore(&StartParticlePtr[i], NewLifeTime);
    }
    // Clean up remainder
    for (; i < Count; i++)
    {
        StartParticlePtr[i] -= DeltaTime;
    }
}

template<SIMDLevel Level>
void ParticleManager::SolveGravity(float *StartParticlePtr, uint32_t Count, float GravityScale, float DeltaTime)
{
    using SIMDStruct = SIMDTraits<Level>;
    // Precompute
    const float ScaledGravityInfluence = GravityScale * Commons::Constants::GRAVITY * DeltaTime;
    auto BroadcastGravityInfluence = SIMDStruct::VectorizedBroadcast(ScaledGravityInfluence);
    // SIMD loop, just subtract the influence from all particle velocity
    uint32_t i = 0;
    for (; i + m_SIMDWidth <= Count; i += m_SIMDWidth)
    {
        auto ParticleVelocity = SIMDStruct::VectorizedLoad(&StartParticlePtr[i]);
        auto NewVelocity = SIMDStruct::VectorizedAdd(ParticleVelocity, BroadcastGravityInfluence);
        SIMDStruct::VectorizedStore(&StartParticlePtr[i], NewVelocity);
    }
    for (; i < Count; i++)
    {
        StartParticlePtr[i] += ScaledGravityInfluence;
    }
}



AlignedArray ParticleManager::AllocateAlignedArray(size_t NumElements, size_t Alignment)
{
    void* RawBlock = nullptr;
#ifdef _WIN32
    // Similar to the free situation
    // Windows uses this function to allocate aligned memory
    RawBlock = _aligned_malloc(NumElements * sizeof(float), Alignment);
#else
    // Window DOES NOT support this, POSIX does
    RawBlock = std::aligned_alloc(Alignment, NumElements * sizeof(float));
#endif
    if (!RawBlock)
    {
        std::cerr << "Failed to allocate aligned array!" << std::endl;
        return nullptr;
    }
    return AlignedArray(static_cast<float*>(RawBlock));
}
