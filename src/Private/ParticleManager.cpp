#include <iostream>
#include "glm/gtc/constants.hpp"

// Own headers
#include "ParticleManager.hpp"
#include "ParticleMath_Shared.h"

using namespace Commons;


void ParticleManager::InitializeParticles()
{
    /* Allocate the aligned arrays for all particle states
     * We are using 64-byte alignment to account for all three SIMD alignment requirements
     * Come to think of it this here alone does not guarantee alignment, since we are dispatching
     * particles to different threads by chunks. Very possible that a thread gets a chunk of 10 particles
     * and that's basically 40 bytes which does not align well
     */
    // Should be doing some null checking here
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

    // Initialize the SIMD manager and get back the SIMD level
    m_SIMDManager = SIMDManager();
    m_SIMDManager.CheckSIMDSupport();
    m_SIMDLevel = m_SIMDManager.GetSIMDLevel();
    /* We only build SSE2/AVX2/NEON kernel translation units, so map any detected level
     * we don't have a TU for onto the nearest one we do
     * This might look like a job for the SIMDManager class, and it probably is
     * However, in my opinion the class should be as generic possible, and that means supporting avx512
     * We don't support it in this object because it's not necessarily faster, but for a generic SIMD class
     * I don't think it should force clamp to AVX2
     */
#if defined(__x86_64__) || defined(_M_X64)
    if (m_SIMDLevel == SIMDLevel::AVX512)
    {
        m_SIMDLevel = SIMDLevel::AVX2;
    }
    else if (m_SIMDLevel == SIMDLevel::Scalar)
    {
        m_SIMDLevel = SIMDLevel::SSE2;
    }
#endif
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
    SpawnParticles(Config, DeltaTime);
    UpdateParticles(Config, DeltaTime);
}



void ParticleManager::SpawnParticles(const ParticleSimulatorConfig& Config, const float DeltaTime)
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
                static_cast<float>(Constants::NUM_THREADS_USED));
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
                static_cast<float>(Constants::NUM_THREADS_USED));
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
        SpawnFutures.push_back(SpawnParticles_Dispatch(i, ChunkCount, Config));
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
    const ParticleSimulatorConfig& Config)
{
    // Submit the spawn task to the thread pool by wrapping it in a lambda
    // This way std::bind doesn't get confused and copy our ref params
    switch (Config.Shape)
    {
        case SpawnShape::Sphere:
        {
            return m_VThreadPool->SubmitTask([this, StartIndex, Count, &Config]()
            {
                SpawnParticles_Sphere(StartIndex, Count, Config);
            });
        }
        case SpawnShape::Box:
        {
            return m_VThreadPool->SubmitTask([this, StartIndex, Count, &Config]()
            {
                SpawnParticles_BoxPlane(StartIndex, Count, Config);
            });
        }
        case SpawnShape::Cone:
        {
            return m_VThreadPool->SubmitTask([this, StartIndex, Count, &Config]()
            {
                SpawnParticles_Cone(StartIndex, Count, Config);
            });
        }
        case SpawnShape::Cylinder:
        {
            return m_VThreadPool->SubmitTask([this, StartIndex, Count, &Config]()
            {
                SpawnParticles_Cylinder(StartIndex, Count, Config);
            });
        }
        case SpawnShape::Ring:
        {
            return m_VThreadPool->SubmitTask([this, StartIndex, Count, &Config]()
            {
                SpawnParticles_RingDisc(StartIndex, Count, Config);
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
        static_cast<float>(m_ParticleCount) / static_cast<float>(Constants::NUM_THREADS_USED)));

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
                DispatchSolveForce(i, ChunkCount, Config.ForceConfigData, DeltaTime, WindPtr);
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
                DispatchUpdate(i, ChunkCount, Config, DeltaTime);
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

glm::vec3 ParticleManager::ComputeWindInfluence(const uint32_t ForceIndex, const float Strength,
    const glm::vec3& Direction, const float Period, const float DeltaTime)
{
    // Each wind force has its own timer so multiple winds oscillate independently
    m_WindTimers[ForceIndex] += DeltaTime;
    // Wrap to [0, Period) so this number doesn't grow forever
    // Period would not be 0 given our setup, UI clamped it, and the init code set it to 1.5
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

void ParticleManager::DispatchSolveForce(uint32_t StartParticleIndex, uint32_t Count,
    const ForceConfig& ForceConfigData, float DeltaTime, const glm::vec3* WindInfluences)
{
    // We choose to guard the different architectures here once instead of switch-case in every single force solver
    // Less code and easier to read
#if defined(__x86_64__) || defined(_M_X64)
    // No need for switch-case here since we only really supports two options, avx2 or sse2
    // Not supporting AVX512 because it's a lot newer, and doesn't always bring improvements
    // but it can easily added in the future
    if (m_SIMDLevel == SIMDLevel::AVX2)
    {
        SolveForces<SIMDLevel::AVX2>(StartParticleIndex, Count, ForceConfigData, DeltaTime, WindInfluences);
    }
    else
    {
        SolveForces<SIMDLevel::SSE2>(StartParticleIndex, Count, ForceConfigData, DeltaTime, WindInfluences);
    }
#elif defined(__aarch64__) || defined(_M_ARM64)
    SolveForces<SIMDLevel::NEON>(StartParticleIndex, Count, ForceConfigData, DeltaTime, WindInfluences);
#endif
}

template <SIMDLevel Level>
void ParticleManager::SolveForces(uint32_t StartParticleIndex, uint32_t Count, const ForceConfig& ForceConfigData,
    float DeltaTime, const glm::vec3* WindInfluences)
{
    // Deal with gravity first
    ParticleMath::SolveGravity<Level>(StartParticleIndex, Count, ForceConfigData.Gravity, DeltaTime, m_ParticleStates);
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
                ParticleMath::SolveDrag<Level>(StartParticleIndex, Count, ForceConfigData.ForceDataArray[i].Strength, DeltaTime,
                    m_ParticleStates.Vx.get(), m_ParticleStates.Vy.get(), m_ParticleStates.Vz.get());
                break;
            }
            case ForceType::Directional:
            {
                // Wind influence was pre-computed in UpdateParticles (once per frame)
                // to avoid the race condition on m_TimeSinceLastWind
                ParticleMath::SolveWind<Level>(StartParticleIndex, Count, WindInfluences[i], m_ParticleStates.Vx.get(),
                    m_ParticleStates.Vy.get(), m_ParticleStates.Vz.get());
                break;
            }
            case ForceType::Point:
            {
                ParticleMath::SolvePointForce<Level>(StartParticleIndex, Count, ForceConfigData.ForceDataArray[i].Direction, ForceConfigData.ForceDataArray[i].Strength,
                    ForceConfigData.ForceDataArray[i].PointRadius, DeltaTime, m_ParticleStates);
                break;
            }
            case ForceType::Vortex:
            {
                ParticleMath::SolveVortex<Level>(StartParticleIndex, Count, ForceConfigData.ForceDataArray[i].Strength, ForceConfigData.ForceDataArray[i].VortexPull,
                    DeltaTime, glm::vec3(0.f), m_ParticleStates);
                break;
            }
        }
    }
}

void ParticleManager::DispatchUpdate(uint32_t StartParticleIndex, uint32_t Count,
    const ParticleSimulatorConfig& Config, float DeltaTime)
{
    // Same architecture guard as DispatchSolveForce: pick the SIMD level once here,
    // everything below is compile-time specialized on Level
#if defined(__x86_64__) || defined(_M_X64)
    if (m_SIMDLevel == SIMDLevel::AVX2)
    {
        UpdateChunk<SIMDLevel::AVX2>(StartParticleIndex, Count, Config, DeltaTime);
    }
    else
    {
        UpdateChunk<SIMDLevel::SSE2>(StartParticleIndex, Count, Config, DeltaTime);
    }
#elif defined(__aarch64__) || defined(_M_ARM64)
    UpdateChunk<SIMDLevel::NEON>(StartParticleIndex, Count, Config, DeltaTime);
#endif
}

template <SIMDLevel Level>
void ParticleManager::UpdateChunk(uint32_t StartParticleIndex, uint32_t Count,
    const ParticleSimulatorConfig& Config, float DeltaTime)
{
    // Color scaling over lifetime only if the user asks for it. Randomized color at
    // spawn has higher priority, so if IsRandomColor is on we do not scale color
    if (Config.IsScalingColor && !Config.IsRandomColor)
    {
        ParticleMath::UpdateParticleColor<Level>(StartParticleIndex, Count, Config.StartColor, Config.EndColor,
            m_ParticleStates, m_ParticleStates.R.get(), m_ParticleStates.G.get(), m_ParticleStates.B.get());
    }
    // Positions, one call per axis
    ParticleMath::UpdateParticlePositionForAxis<Level>(&m_ParticleStates.Px[StartParticleIndex], Count,
        &m_ParticleStates.Vx[StartParticleIndex], DeltaTime);
    ParticleMath::UpdateParticlePositionForAxis<Level>(&m_ParticleStates.Py[StartParticleIndex], Count,
        &m_ParticleStates.Vy[StartParticleIndex], DeltaTime);
    ParticleMath::UpdateParticlePositionForAxis<Level>(&m_ParticleStates.Pz[StartParticleIndex], Count,
        &m_ParticleStates.Vz[StartParticleIndex], DeltaTime);
    // Lifetime
    ParticleMath::UpdateParticleLifeTime<Level>(StartParticleIndex, Count, DeltaTime, m_ParticleStates);
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
    if (m_ParticleCount == 0)
    {
        return;
    }
    for (int i = static_cast<int>(m_ParticleCount - 1); i >= 0; i--)
    {
        if (m_ParticleStates.Py[i] <= KILL_Y)
        {
            KillParticle(i);
        }
    }
}


void ParticleManager::SpawnParticles_Sphere(uint32_t StartParticleIndex, uint32_t Count,
    const ParticleSimulatorConfig& Config)
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
    const ParticleSimulatorConfig& Config)
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
    const ParticleSimulatorConfig& Config)
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
    const ParticleSimulatorConfig& Config)
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
    const ParticleSimulatorConfig& Config)
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
