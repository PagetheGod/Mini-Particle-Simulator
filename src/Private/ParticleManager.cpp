//
// Created by YWvin on 2026/3/23.
//


#include <cstdlib>
#include <iostream>
#include "glm/gtc/random.hpp"
#include "glm/gtx/common.inl"

#include "ParticleManager.hpp"
#ifdef _MSC_VER
#define restrict __restrict
#else
#define restrict __restrict__
#endif


using namespace Commons;


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

    // Start the thread pool, with 16 threads, thread pool already has clamping logics
    m_VThreadPool = std::make_unique<VThreadPool>(NUM_THREADS_USED, true);
    // Initialize the SIMD manager and get back the SIMD levels and widths
    m_SIMDManager = SIMDManager();
    m_SIMDLevel = m_SIMDManager.GetSIMDLevel();
}

void ParticleManager::ParticleFrame(const float DeltaTime, const ParticleSimulatorConfig& Config,
    const bool IsConfigDirty)
{
    // If config is dirty(user changed things), killed all particles, restart render
    // Reset all state trackers
    if (IsConfigDirty)
    {
        m_ParticleCount = 0;
        memset(m_WindTimers, 0, sizeof(m_WindTimers));
        m_TimeSinceLastBurst = 0.f;
        m_EmitterLifeTime = Config.EmitterLifeTime;
    }
    SpawnParticles(Config, IsConfigDirty, DeltaTime);
    UpdateParticles(Config, DeltaTime);
}



void ParticleManager::SpawnParticles(const ParticleSimulatorConfig& Config, const bool IsConfigDirty,
    const float DeltaTime)
{
    // We are already at limit, skip spawning for this frame
    if (m_ParticleCount >= NUM_MAX_PARTICLES || m_EmitterLifeTime <= 0.f)
    {
        return;
    }
    uint32_t NumParticleSpawn = 0;
    uint32_t NumParticlesPerThread = 0;
    if (Config.Mode == EmitterMode::Burst)
    {
        // Are we there to burst?
        m_TimeSinceLastBurst += DeltaTime;
        if (m_TimeSinceLastBurst >= Config.BurstInterval)
        {
            // Calculate how many particles we got to spawn, rounding down
            NumParticleSpawn = std::min(NUM_MAX_PARTICLES - m_ParticleCount,
                static_cast<uint32_t>(Config.EmissionRate));
            NumParticlesPerThread = std::ceil(static_cast<float>(NumParticleSpawn) /
                static_cast<float>(NUM_THREADS_USED));
            // Reset timer
            m_TimeSinceLastBurst = 0.f;
        }
    }
    else
    {
        // Similar, clamp spawn counts
        NumParticleSpawn = std::min(
            static_cast<uint32_t>(std::floor(static_cast<float>(Config.EmissionRate) * DeltaTime)),
            NUM_MAX_PARTICLES - m_ParticleCount);
        NumParticlesPerThread = std::ceil(static_cast<float>(NumParticleSpawn) /
                static_cast<float>(NUM_THREADS_USED));
    }
    // Dispatch spawn work to the thread pool in chunks
    std::vector<std::future<void>> SpawnFutures;
    for (uint32_t i = m_ParticleCount; i < NUM_MAX_PARTICLES && i < m_ParticleCount + NumParticleSpawn;
        i += NumParticlesPerThread)
    {
        // Clamp the chunk size so we don't overshoot the spawn count or the max particle limit
        const uint32_t ChunkEnd = std::min(i + NumParticlesPerThread, std::min(m_ParticleCount +
            NumParticleSpawn, NUM_MAX_PARTICLES));
        const uint32_t ChunkCount = ChunkEnd - i;
        SpawnFutures.push_back(SpawnParticles_Dispatch(i, ChunkCount, Config, DeltaTime));
    }
    // Wait for all spawn tasks to finish before updating particle count
    // Newly spawned particles must be fully initialized before UpdateParticles reads them
    for (auto& Future : SpawnFutures)
    {
        Future.get();
    }
    m_ParticleCount += NumParticleSpawn;
    m_EmitterLifeTime -= DeltaTime;
}

std::future<void> ParticleManager::SpawnParticles_Dispatch(uint32_t StartIndex, uint32_t Count,
    const ParticleSimulatorConfig& Config, const float DeltaTime)
{
    // Submit the spawn task to the thread pool by wrapping it in a lambda
    // This way std::bind doesn't get confused and copy our ref params
    switch (Config.Shape)
    {
        case SpawnShape::Sphere:
        {
            return m_VThreadPool->SubmitTask([this, StartIndex, Count, &Config, DeltaTime]()
            {
                SpawnParticles_Sphere(StartIndex, Count, Config, DeltaTime);
            });
        }
        case SpawnShape::Box:
        {
            return m_VThreadPool->SubmitTask([this, StartIndex, Count, &Config, DeltaTime]()
            {
                SpawnParticles_BoxPlane(StartIndex, Count, Config, DeltaTime);
            });
        }
        case SpawnShape::Cone:
        {
            return m_VThreadPool->SubmitTask([this, StartIndex, Count, &Config, DeltaTime]()
            {
                SpawnParticles_Cone(StartIndex, Count, Config, DeltaTime);
            });
        }
        case SpawnShape::Cylinder:
        {
            return m_VThreadPool->SubmitTask([this, StartIndex, Count, &Config, DeltaTime]()
            {
                SpawnParticles_Cylinder(StartIndex, Count, Config, DeltaTime);
            });
        }
        case SpawnShape::Ring:
        {
            return m_VThreadPool->SubmitTask([this, StartIndex, Count, &Config, DeltaTime]()
            {
                SpawnParticles_RingDisc(StartIndex, Count, Config, DeltaTime);
            });
        }
    }
    // Should never reach here, all SpawnShape values handled above
    return {};
}

void ParticleManager::UpdateParticles(const ParticleSimulatorConfig& Config, float DeltaTime)
{
    if (m_ParticleCount == 0)
    {
        return;
    }
    /*
     * Update order:
     * 1: Forces — modifies velocities, must complete before position updates
     * 2: Colors, positions, lifetime — independent of each other, depend on forces being done
     * 3: Kill dead particles — sequential sweep, kill and swap, must run after all threaded work
     */

    const uint32_t NumParticlesPerThread = static_cast<uint32_t>(std::ceil(
        static_cast<float>(m_ParticleCount) / static_cast<float>(NUM_THREADS_USED)));

    // Pre-compute wind influences on the main thread before threaded dispatch
    // ComputeWindInfluence mutates per-force wind timers, so it cannot run per-chunk
    glm::vec3 WindInfluences[Constants::MAX_NUM_FORCES] = {};
    for (uint32_t i = 0; i < Config.ForceConfigData.ExtraForceCount; i++)
    {
        if (Config.ForceConfigData.ForceTypes[i] == ForceType::Directional)
        {
            // Pass the index as well so compute wind force can
            // Check the oscillation timer specific to each wind force
            WindInfluences[i] = ComputeWindInfluence(i,
                Config.ForceConfigData.ForceDataArray[i].Strength,
                Config.ForceConfigData.ForceDataArray[i].Direction,
                Config.ForceConfigData.ForceDataArray[i].WindPeriod, DeltaTime);
        }
    }
    const glm::vec3* WindPtr = WindInfluences;

    // Solve forces
    // Our custom thread pool returns a future object that can help synchronizations
    // Put them all inside a vector so we can query them later
    std::vector<std::future<void>> Futures;
    const uint32_t FutureEstimate = m_ParticleCount / NumParticlesPerThread;
    Futures.reserve(FutureEstimate);
    for (uint32_t i = 0; i < m_ParticleCount; i += NumParticlesPerThread)
    {
        const uint32_t ChunkEnd = std::min(i + NumParticlesPerThread, m_ParticleCount);
        const uint32_t ChunkCount = ChunkEnd - i;
        Futures.push_back(m_VThreadPool->SubmitTask(
            [this, i, ChunkCount, &Config, DeltaTime, WindPtr]()
            {
                SolveForces(i, ChunkCount, Config.ForceConfigData, DeltaTime, WindPtr);
            }));
    }
    for (auto& Future : Futures)
    {
        // Get() will block until the thread is finished with the work
        Future.get();
    }
    Futures.clear();

    // Color, positions, and lifetime, these can also be synchronized
    // These are independent per-particle but depend on forces being done

    /*
     * Note that we are doing all three together because this introduces less overhead
     * If we separate these three, then we have to do three times the number of locks/unlocks
     * condition_variables related sleep/waking, vector pushes...etc
     */
    for (uint32_t i = 0; i < m_ParticleCount; i += NumParticlesPerThread)
    {
        const uint32_t ChunkEnd = std::min(i + NumParticlesPerThread, m_ParticleCount);
        const uint32_t ChunkCount = ChunkEnd - i;
        Futures.push_back(m_VThreadPool->SubmitTask(
            [this, i, ChunkCount, &Config, DeltaTime]()
            {
                // Color scaling over lifetime only if user asks for it.
                // Randomized color at spawn has higher priority, if IsRandomColor is on,
                // we do not scale color
                if (Config.IsScalingColor && !Config.IsRandomColor)
                {
                    UpdateParticleColor(i, ChunkCount, Config.StartColor, Config.EndColor);
                }
                // Positions, remember to call for all three axes
                UpdateParticlePositionForAxis_Scalar(&m_ParticleStates.Px[i], ChunkCount,
                    &m_ParticleStates.Vx[i], DeltaTime);
                UpdateParticlePositionForAxis_Scalar(&m_ParticleStates.Py[i], ChunkCount,
                    &m_ParticleStates.Vy[i], DeltaTime);
                UpdateParticlePositionForAxis_Scalar(&m_ParticleStates.Pz[i], ChunkCount,
                    &m_ParticleStates.Vz[i], DeltaTime);
                // Lifetime
                UpdateParticleLifeTime(i, ChunkCount, DeltaTime);
            }));
    }
    for (auto& Future : Futures)
    {
        Future.get();
    }

    // Sweep, check, kill and then swap, this step has to be sequential
    // Because we are swapping things
    CheckParticleLifeTime();
    CheckParticleY();
}

void ParticleManager::SolveGravity(uint32_t StartParticleIndex, uint32_t Count, float GravityScale, float DeltaTime)
{
    // Precompute scaled gravity's influences
    const float ScaledGravityInfluence = GravityScale * Constants::GRAVITY * DeltaTime;

    // Gravity pulls down in world space (negative Y direction)
    // Camera2D flips Y when converting to screen space
    for (uint32_t i = StartParticleIndex; i < StartParticleIndex + Count; i++)
    {
        m_ParticleStates.Vy[i] -= ScaledGravityInfluence;
    }
}

glm::vec3 ParticleManager::ComputeWindInfluence(const uint32_t ForceIndex, const float Strength,
    const glm::vec3& Direction, const float Period, const float DeltaTime)
{
    // Each wind force has its own timer so multiple winds oscillate independently
    m_WindTimers[ForceIndex] += DeltaTime;
    // Wrap to [0, Period) so this number doesn't grow forever
    constexpr float TWO_PI = glm::radians(360.f);
    m_WindTimers[ForceIndex] = glm::mod(m_WindTimers[ForceIndex], Period);
    const float SinTime = m_WindTimers[ForceIndex] * (TWO_PI / Period);
    // Added the missing normalization for wind direction vectors
    // vector to avoid dividing by zero.
    const float DirLengthSqr = glm::dot(Direction, Direction);
    if (DirLengthSqr < Constants::CUSTOM_EPSILON)
    {
        return glm::vec3(0.f);
    }
    const glm::vec3 UnitDirection = Direction * glm::inversesqrt(DirLengthSqr);
    return UnitDirection * Strength * glm::sin(SinTime) * DeltaTime;
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
    const float DragInfluence = std::max(0.f, 1.f - DragCoefficient * DeltaTime);

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
        const float Distance = std::max(glm::sqrt(DistanceSquare), Constants::CUSTOM_EPSILON);
        const float InverseDistance = 1.f / Distance;
        // Get the tangential components, normalize everything
        float TangentX = RadialZ;
        float TangentZ = -RadialX;
        TangentX *= InverseDistance;
        TangentZ *= InverseDistance;
        RadialX *= InverseDistance;
        RadialZ *= InverseDistance;
        // Tangential spins around the axis, radial pulls toward the center (subtract = inward)
        m_ParticleStates.Vx[i] += TangentX * DtStrength - RadialX * DtPull;
        m_ParticleStates.Vz[i] += TangentZ * DtStrength - RadialZ * DtPull;
    }
}

void ParticleManager::SolvePointForce(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& ForcePosition,
    const float Strength, const float Radius, const float DeltaTime)
{
    /*
     * Calculate the point attractor/repulsor force using smoothstep falloff.
     * Steps:
     * 1. Calculate the vector from the particle to the force position, get its length
     * 2. Compute smoothstep(Radius, 0, dist), full strength at center, zero at Radius
     * 3. Normalize the direction, apply force * smoothstep * dt to velocity
     * Using smoothstep because the inverse square fall off is really EXTREME
     */

    /*
     * The __restrict__ qualifiers SHOULD inform the compilers that the pointers do not overlap
     * However godbolt showed that even with the qualifiers
     * Clang seems to still run extra checks at run time, strange
     */
    float* restrict PosX = m_ParticleStates.Px.get() + StartParticleIndex;
    float* restrict PosY = m_ParticleStates.Py.get() + StartParticleIndex;
    float* restrict PosZ = m_ParticleStates.Pz.get() + StartParticleIndex;

    float* restrict VelX = m_ParticleStates.Vx.get() + StartParticleIndex;
    float* restrict VelY = m_ParticleStates.Vy.get() + StartParticleIndex;
    float* restrict VelZ = m_ParticleStates.Vz.get() + StartParticleIndex;

    const float DtStrength = Strength * DeltaTime;
    for (uint32_t i = 0; i < Count; i++)
    {
        // Delta from particle to force position
        float DeltaX = ForcePosition.x - PosX[i];
        float DeltaY = ForcePosition.y - PosY[i];
        float DeltaZ = ForcePosition.z - PosZ[i];

        float DistSquare = DeltaX * DeltaX + DeltaY * DeltaY + DeltaZ * DeltaZ;
        float Dist = std::sqrt(DistSquare);

        // Skip particles outside the influence radius
        if (Dist >= Radius)
        {
            continue;
        }

        // Normalize direction (avoid div by zero)
        if (Dist < Constants::CUSTOM_EPSILON)
        {
            continue;
        }
        float InvDist = 1.f / Dist;
        DeltaX *= InvDist;
        DeltaY *= InvDist;
        DeltaZ *= InvDist;

        // smoothstep(Radius, 0, Dist) = smoothstep at edges [0, Radius], full at center
        // t = clamp((Radius - Dist) / Radius, 0, 1) = 1 - Dist/Radius (already clamped by the early-out)
        float T = 1.f - Dist / Radius;
        float Falloff = T * T * (3.f - 2.f * T);

        float PointInfluence = DtStrength * Falloff;
        VelX[i] += PointInfluence * DeltaX;
        VelY[i] += PointInfluence * DeltaY;
        VelZ[i] += PointInfluence * DeltaZ;
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
     * Function to kill a single particle, this could be invoked by kill-Y or life time expirations
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
void ParticleManager::UpdateParticlePositionForAxis_Scalar(float* restrict StartParticlePtr, uint32_t Count,
    const float* restrict Velocity, float DeltaTime)
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
    if (m_ParticleCount == 0)
    {
        // Now technically this is not needed since in our flow
        // We already checked for it at the start of the UpdateParticles function, however,
        // Since both CheckParticleY/LifeTime kill particles, we have a chance of exploding
        return;
    }
    for (int i = static_cast<int>(m_ParticleCount - 1); i >= 0; i--)
    {
        if (m_ParticleStates.LifeTime[i] <= 0.f)
        {
            KillParticle(i);
        }
    }
}

void ParticleManager::CheckParticleY()
{
    // Go through all the particles, check their Y coordinates
    // If it's smaller than our set kill Y, kill them
    if (m_ParticleCount == 0) { return; }
    for (int i = static_cast<int>(m_ParticleCount - 1); i >= 0; i--)
    {
        if (m_ParticleStates.Py[i] <= KILL_Y)
        {
            KillParticle(i);
        }
    }
}


void ParticleManager::SpawnParticles_Sphere(uint32_t StartParticleIndex, uint32_t Count, const ParticleSimulatorConfig& Config, const float DeltaTime)
{
    /*
     * Use spherical coordinates to spawn the particles
     * We need: 1. A a radius-ish R, 2. A heading angle, h, which matches the angle formed with Z(forward)
     * 3. A pitch angle, which measures how much we "pitch" down from the xz plane
     * This deviates a bit from the usual math notations
     */
    // Precompute the invariants for all particles within the sphere
    constexpr float RMinCube = 0.1f * 0.1f * 0.1f;
    const float RMaxCube = Config.SphereRadius * Config.SphereRadius * Config.SphereRadius;
    const float Difference = RMaxCube - RMinCube;
    for (uint32_t i = StartParticleIndex; i < StartParticleIndex + Count; i++)
    {
        /*
         * Generate all the spherical coordinates required
         * Because equal angular displacement does not create the same surface areas on a sphere
         * We need to account for that by sampling the sine of pitch uniformly
         * Otherwise, the poles get too dense and the equator looks sparse
         *
         * Additionally, because any of the sub spheres(or shells) within the sphere
         * have volume that's proportional to the cube of radius
         * We cannot just use radius here, we need to basically:
         * 1. Generate a random number in range 0 to 1
         * 2. Take the cube root of the number in 1
         * 3. Scale the radius with it
         */
        const float Radius = std::cbrt(Utility::RandomFloat_01() * Difference + RMinCube);
        const float H = Utility::RandomFloat(-glm::pi<float>(), glm::pi<float>());
        const float SinP = Utility::RandomFloat(-1.f, 1.f);
        // Convert to Cartesian
        const float CosineP = glm::sqrt(1.f - SinP * SinP);
        const float X = Radius * CosineP * glm::sin(H);
        const float Y = Radius * SinP;
        const float Z = Radius * CosineP * glm::cos(H);
        m_ParticleStates.Px[i] = X;
        m_ParticleStates.Py[i] = Y;
        m_ParticleStates.Pz[i] = Z;
        // Velocity, color, size, lifetime
        const glm::vec3 Velocity = SpawnParticles_Speed(X, Y, Z, Config);
        m_ParticleStates.Vx[i] = Velocity.x;
        m_ParticleStates.Vy[i] = Velocity.y;
        m_ParticleStates.Vz[i] = Velocity.z;
        const glm::vec3 Color = SpawnParticles_Color(Config);
        m_ParticleStates.R[i] = Color.r;
        m_ParticleStates.G[i] = Color.g;
        m_ParticleStates.B[i] = Color.b;
        m_ParticleStates.Size[i] = SpawnParticles_Size(Config);
        const float Life = SpawnParticles_LifeTime(Config);
        m_ParticleStates.LifeTime[i] = Life;
        m_ParticleStates.MaxLifeTime[i] = Life;
    }
}

void ParticleManager::SpawnParticles_BoxPlane(uint32_t StartParticleIndex, uint32_t Count,
    const ParticleSimulatorConfig& Config, const float DeltaTime)
{
    // This one is more straightforward, just [-width, width), [-height, height), etc...
    for (uint32_t i = StartParticleIndex; i < StartParticleIndex + Count; i++)
    {
        // Generate and set all the coordinates
        const float X = Utility::RandomFloat(-Config.BoxDimensions.x, Config.BoxDimensions.x);
        const float Y = Utility::RandomFloat(-Config.BoxDimensions.y, Config.BoxDimensions.y);
        const float Z = Utility::RandomFloat(-Config.BoxDimensions.z, Config.BoxDimensions.z);
        m_ParticleStates.Px[i] = X;
        m_ParticleStates.Py[i] = Y;
        m_ParticleStates.Pz[i] = Z;
        // Velocity, color, size, lifetime
        const glm::vec3 Velocity = SpawnParticles_Speed(X, Y, Z, Config);
        m_ParticleStates.Vx[i] = Velocity.x;
        m_ParticleStates.Vy[i] = Velocity.y;
        m_ParticleStates.Vz[i] = Velocity.z;
        const glm::vec3 Color = SpawnParticles_Color(Config);
        m_ParticleStates.R[i] = Color.r;
        m_ParticleStates.G[i] = Color.g;
        m_ParticleStates.B[i] = Color.b;
        m_ParticleStates.Size[i] = SpawnParticles_Size(Config);
        const float Life = SpawnParticles_LifeTime(Config);
        m_ParticleStates.LifeTime[i] = Life;
        m_ParticleStates.MaxLifeTime[i] = Life;
    }
}

void ParticleManager::SpawnParticles_RingDisc(uint32_t StartParticleIndex, uint32_t Count,
    const ParticleSimulatorConfig& Config, const float DeltaTime)
{
    // Same thing with cylinder, but now the RMin is provided by the user
    // And we have no height
    constexpr float AbsoluteRMinSquare = 0.1f * 0.1f;
    const float RMinSquare = std::max(AbsoluteRMinSquare, Config.RingDimensions.x * Config.RingDimensions.x);
    const float RMaxSquare = Config.RingDimensions.y * Config.RingDimensions.y;
    const float Difference = RMaxSquare - RMinSquare;
    for (uint32_t i = StartParticleIndex; i < StartParticleIndex + Count; i++)
    {
        const float Radius = glm::sqrt(Utility::RandomFloat_01() * Difference + RMinSquare);
        const float Theta = Utility::RandomFloat(-glm::pi<float>(), glm::pi<float>());
        const float Y = Utility::RandomFloat(0.f, Config.RingDimensions.z);
        // Convert to Cartesian
        const float X = Radius * glm::cos(Theta);
        const float Z = Radius * glm::sin(Theta);
        m_ParticleStates.Px[i] = X;
        m_ParticleStates.Py[i] = Y;
        m_ParticleStates.Pz[i] = Z;
        // Velocity, color, size, lifetime — shared across all shapes
        const glm::vec3 Velocity = SpawnParticles_Speed(X, 0.f, Z, Config);
        m_ParticleStates.Vx[i] = Velocity.x;
        m_ParticleStates.Vy[i] = Velocity.y;
        m_ParticleStates.Vz[i] = Velocity.z;
        const glm::vec3 Color = SpawnParticles_Color(Config);
        m_ParticleStates.R[i] = Color.r;
        m_ParticleStates.G[i] = Color.g;
        m_ParticleStates.B[i] = Color.b;
        m_ParticleStates.Size[i] = SpawnParticles_Size(Config);
        const float Life = SpawnParticles_LifeTime(Config);
        m_ParticleStates.LifeTime[i] = Life;
        m_ParticleStates.MaxLifeTime[i] = Life;
    }
}

void ParticleManager::SpawnParticles_Cylinder(uint32_t StartParticleIndex, uint32_t Count,
    const ParticleSimulatorConfig& Config, const float DeltaTime)
{
    /*
     * Again we use polar coordinates, but same issue with sphere
     * So cannot use the radius naively since larger radius associates with larger volume
     * So we do sqrt(random_01 * (Rmax^2 - Rmin^2) + Rmin^2)
     * This gives uniform area distribution across the disc cross-section
     */
    constexpr float RMinSquare = 0.1f * 0.1f;
    const float RMaxSquare = Config.CylinderDimensions.x * Config.CylinderDimensions.x;
    const float Difference = RMaxSquare - RMinSquare;
    for (uint32_t i = StartParticleIndex; i < StartParticleIndex + Count; i++)
    {
        const float Radius = glm::sqrt(Utility::RandomFloat_01() * Difference + RMinSquare);
        const float Theta = Utility::RandomFloat(-glm::pi<float>(), glm::pi<float>());
        // Height
        const float Y = Utility::RandomFloat(0.f, Config.CylinderDimensions.y);
        // Convert to Cartesian
        const float X = Radius * glm::cos(Theta);
        const float Z = Radius * glm::sin(Theta);
        m_ParticleStates.Px[i] = X;
        m_ParticleStates.Py[i] = Y;
        m_ParticleStates.Pz[i] = Z;
        // Velocity, color, size, lifetime — shared across all shapes
        const glm::vec3 Velocity = SpawnParticles_Speed(X, Y, Z, Config);
        m_ParticleStates.Vx[i] = Velocity.x;
        m_ParticleStates.Vy[i] = Velocity.y;
        m_ParticleStates.Vz[i] = Velocity.z;
        const glm::vec3 Color = SpawnParticles_Color(Config);
        m_ParticleStates.R[i] = Color.r;
        m_ParticleStates.G[i] = Color.g;
        m_ParticleStates.B[i] = Color.b;
        m_ParticleStates.Size[i] = SpawnParticles_Size(Config);
        const float Life = SpawnParticles_LifeTime(Config);
        m_ParticleStates.LifeTime[i] = Life;
        m_ParticleStates.MaxLifeTime[i] = Life;
    }
}

void ParticleManager::SpawnParticles_Cone(uint32_t StartParticleIndex, uint32_t Count,
    const ParticleSimulatorConfig& Config, const float DeltaTime)
{
    /*
     * This one is computationally heavier since not much can be precomputed
     * We first get a random_01, cube root it because cone volumes grow proportional to
     * the cube of height
     * Then at height H, using similar triangles, we get the RMaxAtH = tan(halfAngle) * H
     * The cone opens upward (negative Y in screen space), tip at the origin
     */
    constexpr float HeightMinCube = 0.1f * 0.1f * 0.1f;
    const float HeightMaxCube = Config.ConeDimensions.x * Config.ConeDimensions.x * Config.ConeDimensions.x;
    const float Difference = HeightMaxCube - HeightMinCube;
    // Half angle is stored in degrees from the UI slider, convert to radians for tan
    const float TanHalfAngle = glm::tan(glm::radians(Config.ConeDimensions.y));
    for (uint32_t i = StartParticleIndex; i < StartParticleIndex + Count; i++)
    {
        const float H = std::cbrt(Utility::RandomFloat_01() * Difference + HeightMinCube);
        const float RMaxAtH = TanHalfAngle * H;
        const float Rho = glm::sqrt(Utility::RandomFloat_01()) * RMaxAtH;
        const float Theta = Utility::RandomFloat(-glm::pi<float>(), glm::pi<float>());
        // Convert to Cartesian, cone axis is Y (upward), disc cross-section on XZ
        const float X = Rho * glm::cos(Theta);
        const float Y = H;
        const float Z = Rho * glm::sin(Theta);
        m_ParticleStates.Px[i] = X;
        m_ParticleStates.Py[i] = Y;
        m_ParticleStates.Pz[i] = Z;
        // Velocity, color, size, lifetime — shared across all shapes
        const glm::vec3 Velocity = SpawnParticles_Speed(X, Y, Z, Config);
        m_ParticleStates.Vx[i] = Velocity.x;
        m_ParticleStates.Vy[i] = Velocity.y;
        m_ParticleStates.Vz[i] = Velocity.z;
        const glm::vec3 Color = SpawnParticles_Color(Config);
        m_ParticleStates.R[i] = Color.r;
        m_ParticleStates.G[i] = Color.g;
        m_ParticleStates.B[i] = Color.b;
        m_ParticleStates.Size[i] = SpawnParticles_Size(Config);
        const float Life = SpawnParticles_LifeTime(Config);
        m_ParticleStates.LifeTime[i] = Life;
        m_ParticleStates.MaxLifeTime[i] = Life;
    }
}

glm::vec3 ParticleManager::SpawnParticles_Speed(float X, float Y, float Z, const ParticleSimulatorConfig& Config)
{
    float Speed = 0.f;
    if (Config.IsRandomSpeed)
    {
        Speed = Utility::RandomFloat(Config.Speed.x, Config.Speed.y);
    }
    else
    {
        Speed = Config.Speed.x;
    }
    // Normalize position to get outward direction, then scale by speed
    const float DistSquare = X * X + Y * Y + Z * Z;
    if (DistSquare < Constants::CUSTOM_EPSILON)
    {
        // Position is at origin — pick an arbitrary upward direction
        return {0.f, Speed, 0.f};
    }
    const float InverseDistance = glm::inversesqrt(DistSquare);
    return {X * InverseDistance * Speed, Y * InverseDistance * Speed, Z * InverseDistance * Speed};
}

glm::vec3 ParticleManager::SpawnParticles_Color(const ParticleSimulatorConfig& Config)
{
    if (Config.IsRandomColor)
    {
        return {Utility::RandomFloat_01(),Utility::RandomFloat_01(),
            Utility::RandomFloat_01()};
    }
    return Config.StartColor;
}

float ParticleManager::SpawnParticles_Size(const ParticleSimulatorConfig& Config)
{
    if (Config.IsRandomScale)
    {
        return Utility::RandomFloat(Config.Scale.x, Config.Scale.y);
    }
    return Config.Scale.x;
}

float ParticleManager::SpawnParticles_LifeTime(const ParticleSimulatorConfig& Config)
{
    if (Config.IsRandomLifeTime)
    {
        return Utility::RandomFloat(Config.LifeTime.x, Config.LifeTime.y);
    }
    return Config.LifeTime.x;
}

// Top level force solving function that will call the rest

void ParticleManager::SolveForces(uint32_t StartParticleIndex, uint32_t Count, const ForceConfig& ForceConfigData,
    float DeltaTime, const glm::vec3* WindInfluences)
{
    // Deal with gravity first
    SolveGravity(StartParticleIndex, Count, ForceConfigData.Gravity, DeltaTime);
    for (uint32_t i = 0; i < ForceConfigData.ExtraForceCount; i++)
    {
        if (!ForceConfigData.IsForceEnabled[i])
        {
            continue;
        }
        switch (ForceConfigData.ForceTypes[i])
        {
            case ForceType::Drag:
            {
                SolveDrag(StartParticleIndex, Count, ForceConfigData.ForceDataArray[i].Strength,
                    DeltaTime);
                break;
            }
            case ForceType::Directional:
            {
                // Wind influence was pre-computed in UpdateParticles (once per frame)
                // to avoid the race condition on m_TimeSinceLastWind
                SolveWind(StartParticleIndex, Count, WindInfluences[i]);
                break;
            }
            case ForceType::Point:
            {
                /*
                 * Hardcoded to generate only SSE2 SIMD codes
                 * The reason is that AVX2 and 512 requires the -mavx and -mavx512 flgas
                 * These flags will allow the compilers to generate avx and avx512 instructions
                 * And the flags do not have any compile/link time check to verify hardware supports
                 * Which means if we compile the codes with these flags and run the app on an old CPU
                 * The program will crash because it does not support the instructions
                 * SSE2 on the other hand has much wider support, so we can use it
                 * Like I don't think any one is gonna run this on a 32-bit CPU
                 */
            #if defined(__x86_64__) || defined(_M_X64)
                SolvePointForce_Vector<SIMDLevel::SSE2>(StartParticleIndex, Count,
                    ForceConfigData.ForceDataArray[i].Direction,
                    ForceConfigData.ForceDataArray[i].Strength,
                    ForceConfigData.ForceDataArray[i].PointRadius, DeltaTime);
            #else
                SolvePointForce(StartParticleIndex, Count, ForceConfigData.ForceDataArray[i].Direction,
                    ForceConfigData.ForceDataArray[i].Strength,
                    ForceConfigData.ForceDataArray[i].PointRadius, DeltaTime);
            #endif
                break;
            }
            case ForceType::Vortex:
            {
            #if defined(__x86_64__) || defined(_M_X64)
                SolveVortex_Vector<SIMDLevel::SSE2>(StartParticleIndex, Count,
                    ForceConfigData.ForceDataArray[i].Strength,
                    ForceConfigData.ForceDataArray[i].VortexPull, DeltaTime, glm::vec3(0.f));
            #else
                SolveVortex(StartParticleIndex, Count, ForceConfigData.ForceDataArray[i].Strength,
                    ForceConfigData.ForceDataArray[i].VortexPull, DeltaTime, glm::vec3(0.f));
            #endif
                break;
            }
        }
    }
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

#if defined(__x86_64__) || defined(_M_X64)
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
    constexpr uint32_t SIMDWidth = SIMDStruct::SIMDWidth;
    // Precompute
    const float ScaledGravityInfluence = GravityScale * Constants::GRAVITY * DeltaTime;
    auto BroadcastGravityInfluence = SIMDStruct::VectorizedBroadcast(ScaledGravityInfluence);
    // SIMD loop, just subtract the influence from all particle velocity
    uint32_t i = 0;
    for (; i + SIMDWidth <= Count; i += SIMDWidth)
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

/*
 * I decided to add custom SIMD implementations of point and vortex force solvers
 * because after pasting our scalar versions into godbolt
 * It seems that even with __restrict__ qualifiers and O3 optimization level
 * Clang still generates many extra instructions to check whether pointers overlap at run time
 * Which is a waste
 */

template <SIMDLevel Level>
void ParticleManager::SolvePointForce_Vector(uint32_t StartParticleIndex, uint32_t Count,
    const glm::vec3& ForcePosition, const float Strength, const float Radius, const float DeltaTime)
{
    using SIMDStruct = SIMDTraits<Level>;
    constexpr uint32_t SIMDWidth = SIMDStruct::SIMDWidth;
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
        auto DeltaX = SIMDStruct::VectorizedSub(PointPositionX, SIMDStruct::VectorizedLoad(&m_ParticleStates.Px[i]));
        auto DeltaY = SIMDStruct::VectorizedSub(PointPositionY, SIMDStruct::VectorizedLoad(&m_ParticleStates.Py[i]));
        auto DeltaZ = SIMDStruct::VectorizedSub(PointPositionZ, SIMDStruct::VectorizedLoad(&m_ParticleStates.Pz[i]));

        // Distance squared
        auto DistSquare = SIMDStruct::VectorizedAdd(
            SIMDStruct::VectorizedAdd(
                SIMDStruct::VectorizedMul(DeltaX, DeltaX),
                SIMDStruct::VectorizedMul(DeltaY, DeltaY)),
            SIMDStruct::VectorizedMul(DeltaZ, DeltaZ));

        // Clamp to epsilon before rsqrt to prevent div by 0
        auto ClampedDistSquare = SIMDStruct::VectorizedMax(DistSquare, Epsilon);
        auto InvDist = SIMDStruct::VectorizedRSqrt(ClampedDistSquare);
        // Dist = 1 / InvDist, but we can also get it from DistSquare * InvDist (= sqrt(DistSquare))
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

        // Load velocities, apply force along normalized direction, store back
        auto AdjustedVx = SIMDStruct::VectorizedLoad(&m_ParticleStates.Vx[i]);
        auto AdjustedVy = SIMDStruct::VectorizedLoad(&m_ParticleStates.Vy[i]);
        auto AdjustedVz = SIMDStruct::VectorizedLoad(&m_ParticleStates.Vz[i]);

        AdjustedVx = SIMDStruct::VectorizedAdd(AdjustedVx, SIMDStruct::VectorizedMul(DeltaX, ForceInfluence));
        AdjustedVy = SIMDStruct::VectorizedAdd(AdjustedVy, SIMDStruct::VectorizedMul(DeltaY, ForceInfluence));
        AdjustedVz = SIMDStruct::VectorizedAdd(AdjustedVz, SIMDStruct::VectorizedMul(DeltaZ, ForceInfluence));

        SIMDStruct::VectorizedStore(&m_ParticleStates.Vx[i], AdjustedVx);
        SIMDStruct::VectorizedStore(&m_ParticleStates.Vy[i], AdjustedVy);
        SIMDStruct::VectorizedStore(&m_ParticleStates.Vz[i], AdjustedVz);
    }
    // Scalar cleanup for remaining particles
    for (; i < StartParticleIndex + Count; i++)
    {
        float DeltaX = ForcePosition.x - m_ParticleStates.Px[i];
        float DeltaY = ForcePosition.y - m_ParticleStates.Py[i];
        float DeltaZ = ForcePosition.z - m_ParticleStates.Pz[i];

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

        m_ParticleStates.Vx[i] += PointInfluence * DeltaX;
        m_ParticleStates.Vy[i] += PointInfluence * DeltaY;
        m_ParticleStates.Vz[i] += PointInfluence * DeltaZ;
    }
}

template <SIMDLevel Level>
void ParticleManager::SolveVortex_Vector(uint32_t StartParticleIndex, uint32_t Count, const float VortexStrength,
    const float VortexPull, const float DeltaTime, const glm::vec3& VortexCenter)
{
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
            SIMDStruct::VectorizedLoad(&m_ParticleStates.Px[i]), CenterXVec);
        auto RadialZ = SIMDStruct::VectorizedSub(
            SIMDStruct::VectorizedLoad(&m_ParticleStates.Pz[i]), CenterZVec);

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
        auto AdjustedVx = SIMDStruct::VectorizedAdd(SIMDStruct::VectorizedLoad(&m_ParticleStates.Vx[i]),
            DeltaVx);
        auto AdjustedVz = SIMDStruct::VectorizedAdd(SIMDStruct::VectorizedLoad(&m_ParticleStates.Vz[i]),
            DeltaVz);
        SIMDStruct::VectorizedStore(&m_ParticleStates.Vx[i], AdjustedVx);
        SIMDStruct::VectorizedStore(&m_ParticleStates.Vz[i], AdjustedVz);
    }

    // Scalar cleanup
    for (; i < StartParticleIndex + Count; i++)
    {
        float RadialX = m_ParticleStates.Px[i] - VortexCenter.x;
        float RadialZ = m_ParticleStates.Pz[i] - VortexCenter.z;
        float DistanceSquare = RadialX * RadialX + RadialZ * RadialZ;
        DistanceSquare = std::max(DistanceSquare, Constants::CUSTOM_EPSILON);
        const float InverseDistance = 1.f / std::sqrt(DistanceSquare);

        // Normalize first, then derive tangent from the normalized radial
        RadialX *= InverseDistance;
        RadialZ *= InverseDistance;
        const float TangentX = RadialZ;
        const float TangentZ = -RadialX;

        m_ParticleStates.Vx[i] += TangentX * DtStrength - RadialX * DtPull;
        m_ParticleStates.Vz[i] += TangentZ * DtStrength - RadialZ * DtPull;
    }
}
#endif // defined(__x86_64__) || defined(_M_X64)



AlignedArray ParticleManager::AllocateAlignedArray(size_t NumElements, size_t Alignment)
{
    void* RawBlock = nullptr;
    const size_t NumBytes = NumElements * sizeof(float);
#ifdef _WIN32
    // Similar to the free situation
    // Windows uses this function to allocate aligned memory
    RawBlock = _aligned_malloc(NumBytes, Alignment);
#else
    // POSIX aligned_alloc requires the requested size to be a multiple of the alignment.
    const size_t Remainder = NumBytes % Alignment;
    const size_t RoundedBytes = Remainder == 0 ? NumBytes : (NumBytes + Alignment - Remainder);
    RawBlock = std::aligned_alloc(Alignment, RoundedBytes);
#endif
    if (!RawBlock)
    {
        std::cerr << "Failed to allocate aligned array!" << std::endl;
        return nullptr;
    }
    return AlignedArray(static_cast<float*>(RawBlock));
}
