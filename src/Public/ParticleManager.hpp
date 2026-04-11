#pragma once
#include <cstdint>
#include <memory>
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>


#include "Commons.hpp"
#include "VThreadPool.hpp"
#include "SIMD.hpp"

/*
 * This class will handle creating, updating, and destroying particles
 * It's where we will run the core math and physics of our particle updates
 */


/*
 * Enum definitions: presets, emitter mode, spawn shape and force types
 */
enum class PresetType : uint8_t
{
    None,
    OmniDirectionalBurst,
    Firework,
    Fountain,
    Vortex,
    Waterfall,
    Snow
};

enum class EmitterMode : uint8_t
{
    Burst,
    Continuous
};

enum class SpawnShape: uint8_t
{
    Sphere,
    Box,
    Cone,
    Ring,
    Cylinder
};

enum class ForceType : uint8_t
{
    Drag, // Frictions, slows us down
    Point, // Attract and repel from a direction with quadratic falloff
    Vortex, // Rotational force around an axis
    Directional // In our case, this refers to the wind force(constant direction)
};

/*
 * Force struct that encapsulates data needed for a force
 * It does not contain the types of the forces because it wastes space due to padding
 */
struct ForceData
{
    glm::vec3 Direction = glm::vec3(0.f);
    float Strength = 0.f;
    union
    {
        float VortexPull = 0.f;
        float WindPeriod;
        float PointRadius;
    };

};

struct ForceConfig
{
    float Gravity = 0.f;
    // Fixed size array to store all forces, using a fixed size array because we have an upper limit
    // of how many forces we can have (other than gravity)
    ForceData ForceDataArray[Commons::Constants::MAX_NUM_FORCES]{};
    ForceType ForceTypes[Commons::Constants::MAX_NUM_FORCES]{};
    bool IsForceEnabled[Commons::Constants::MAX_NUM_FORCES]{};
    uint8_t ExtraForceCount = 0;
};

/*
 * ParticleSimulatorConfig the fat struct that encapsulate most of the info needed by the particle manager class to
 * simulate particle math and physics
 */
struct ParticleSimulatorConfig
{
    float BurstInterval = 0.f;
    int EmissionRate = 0;
    float EmitterLifeTime = 0.f;
    glm::vec3 StartColor = glm::vec3(1.f);
    glm::vec3 EndColor = glm::vec3(1.f);
    // Shape specific data... to help me gather my thoughts
    // Sphere - just a radius
    // Cone - does not need an axis(always up), needs a spread(phi), and a rotation(theta)
    // Box/Plane - needs three floats(width, height, depth), set one of the three to 0 to make it a plane
    // Ring/Disc - filled circle needing two floats: min and max radius
    // Cylinder - needs two floats, radius and height
    union
    {
        glm::vec3 BoxDimensions = glm::vec3(1.f); // Width, height, depth
        glm::vec2 ConeDimensions; // Height, spread(half-angle)
        glm::vec3 RingDimensions; // Min and max radius, height
        glm::vec2 CylinderDimensions; // Radius, height
        float SphereRadius;
    };
    glm::vec2 LifeTime = glm::vec2(1.f, 2.f); // Min and max life time
    glm::vec2 Scale = glm::vec2(1.f, 2.f); // Min and max scale
    glm::vec2 Speed = glm::vec2(1.f, 2.f); // Min and max speed at spawn
    float Gravity = 0.8f; // The factor by which we scale 9.81, not gravity itself
    ForceConfig ForceConfigData;
    // Put the single-byte members at the end so we don't waste space due to paddings
    // Probably doesn't matter in this case
    SpawnShape Shape = SpawnShape::Sphere;
    EmitterMode Mode = EmitterMode::Burst;
    bool IsRandomColor = false;
    bool IsScalingColor = false;
    bool IsRandomLifeTime = true;
    bool IsRandomScale = false;
    bool IsRandomSpeed = true;
};
/* SIMD has strict alignment requirements, namely pointers we pass got to be aligned to 16-byte
 * Otherwise it would slower, though I am not sure how much slower that would be
 * But this is a good chance to search and learn online
 * Look what I have found, std::aligned_alloc(), a function that allocates memory with specified alignment
 * Although using this means we can't just delete[] anymore, need to call std::free() on POSIX
 * And _aligned_free() on Windows
 *
 * Also, fun thing I learned, despite being called a custom "deleter", the second template argument to unique_ptr
 * is literally a struct(in popular implementations) that contains a function that free your memory
 */
// In our case it's guaranteed to be a float, so just using float ptr here
struct AlignedDeleter
{
    // Strange, why is this const?
    void operator()(float* Block) const
    {
    #ifdef _WIN32
        // Windows uses this aligned free function
        _aligned_free(Block);
    #else
        // POSIX acts like normal human beings
        std::free(Block);
    #endif
    }
};

// I got tired of typing that chaotic blob on the right
// So here is an alias
using AlignedArray = std::unique_ptr<float[], AlignedDeleter>;


// Using vectors here is also fine. But since we initialize it to the max possible particle count
// And we don't resize, use iterator... or any of the vector utilities
// Therefore, no need for vectors
/*
 * I initially went for a SoAoS design which uses a free list
 * Then I realized that the implementation is pretty bad for SIMD
 * Due to the fact that the dead particles will be mixed with the live ones
 * Forming holes which make everything awkward
 * So I switched to this pure SoA design, and when we kill a particle,
 * we just swap it with the last live one
 */
struct ParticleStates
{
    AlignedArray Px;
    AlignedArray Py;
    AlignedArray Pz;
    AlignedArray Vx;
    AlignedArray Vy;
    AlignedArray Vz;
    AlignedArray R;
    AlignedArray G;
    AlignedArray B;
    AlignedArray Size;
    AlignedArray LifeTime;
    // Almost forgot, we need this to for color scaling
    // We do this by using LifeTime / MaxLifeTime ratio to lerp
    AlignedArray MaxLifeTime;
};


class ParticleManager
{
public:
    //Constructors and destructors
    ParticleManager() = default;

    ParticleManager(const ParticleManager&) = delete;// Same with application class, makes no sense to copy or move
    ParticleManager& operator=(const ParticleManager&) = delete;
    ParticleManager(ParticleManager&&) = delete;
    ParticleManager& operator=(ParticleManager&&) = delete;

    ~ParticleManager() = default;

    //Actual work functions
    // Allocated all the arrays holding the particle states, this does not actually initialize the particle data
    // We spawn particles according to config we get from the UI Manager
    void InitializeParticles();
    // The top-level per frame function that will get called by Application class inside its main loop
    // This will then go on to call other more granular update functions
    void ParticleFrame(const float DeltaTime, const ParticleSimulatorConfig& Config,
        const bool IsConfigDirty);


    //Getters and setters
    [[nodiscard]] uint32_t GetParticleCount() const {
        return m_ParticleCount;
    }
    [[nodiscard]] glm::vec3 GetParticlePos(uint32_t Index) const
    {
        return {m_ParticleStates.Px[Index], m_ParticleStates.Py[Index], m_ParticleStates.Pz[Index]};
    }
    [[nodiscard]] glm::vec3 GetParticleColor(uint32_t Index) const
    {
        return {m_ParticleStates.R[Index], m_ParticleStates.G[Index], m_ParticleStates.B[Index]};
    }
    [[nodiscard]] float GetParticleLifeTime(uint32_t Index) const
    {
        return m_ParticleStates.LifeTime[Index];
    }
    [[nodiscard]] float GetParticleScale(uint32_t Index) const
    {
        return m_ParticleStates.Size[Index];
    }
    [[nodiscard]] float GetParticleRelLifeTime(uint32_t Index) const
    {
        return m_ParticleStates.LifeTime[Index] / m_ParticleStates.MaxLifeTime[Index];
    }
public:

private:
    // Granular particle functions that will be called by particle frame()

    void SpawnParticles(const ParticleSimulatorConfig& Config, const bool IsConfigDirty, const float DeltaTime);
    std::future<void> SpawnParticles_Dispatch(uint32_t StartIndex, uint32_t Count,
        const ParticleSimulatorConfig& Config, const float DeltaTime);
    void UpdateParticles(const ParticleSimulatorConfig& Config, float DeltaTime);


    // This function only gets called when the user set scaled color to true
    // Meaning that we need to lerp between start and end colors based on their relative lifetimes
    void UpdateParticleColor(uint32_t StartParticleIndex, uint32_t Count,
        const glm::vec3& StartColor, const glm::vec3& EndColor);
    void UpdateParticleLifeTime(uint32_t StartParticleIndex, uint32_t Count, float DeltaTime);
    // Function to kill a single particle
    void KillParticle(const uint32_t KillIndex);

    // This function will use the already-updated velocities to update particle positions
    // One axis at a time for parallel processing
    // Meaning it should get called AFTER forces had been solved
    void UpdateParticlePositionForAxis_Scalar(float* StartParticlePtr, uint32_t Count, const float* Velocity,
    float DeltaTime);
    void CheckParticleLifeTime();
    void CheckParticleY();
    // Particle spawn functions for each shape, these only set position
    void SpawnParticles_Sphere(uint32_t StartParticleIndex, uint32_t Count, const ParticleSimulatorConfig& Config,
        const float DeltaTime);
    void SpawnParticles_BoxPlane(uint32_t StartParticleIndex, uint32_t Count, const ParticleSimulatorConfig& Config,
        const float DeltaTime);
    void SpawnParticles_RingDisc(uint32_t StartParticleIndex, uint32_t Count, const ParticleSimulatorConfig& Config,
        const float DeltaTime);
    void SpawnParticles_Cylinder(uint32_t StartParticleIndex, uint32_t Count, const ParticleSimulatorConfig& Config,
        const float DeltaTime);
    void SpawnParticles_Cone(uint32_t StartParticleIndex, uint32_t Count, const ParticleSimulatorConfig& Config,
        float DeltaTime);
    // Shared spawn helpers, these are the same for all shapes
    glm::vec3 SpawnParticles_Speed(float X, float Y, float Z, const ParticleSimulatorConfig& Config);
    glm::vec3 SpawnParticles_Color(const ParticleSimulatorConfig& Config);
    float SpawnParticles_Size(const ParticleSimulatorConfig& Config);
    float SpawnParticles_LifeTime(const ParticleSimulatorConfig& Config);

    // Force solvers, these will get dispatched by the UpdateParticle function
    void SolveForces(uint32_t StartParticleIndex, uint32_t Count, const ForceConfig& ForceConfigData,
        float DeltaTime, const glm::vec3* WindInfluences);
    void SolveGravity(uint32_t StartParticleIndex, uint32_t Count, float GravityScale, float DeltaTime);
    void SolveWind(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& WindInfluence);
    // Computes the wind influence vector for this frame. Call once per frame before dispatching SolveWind to threads.
    // ForceIndex identifies which wind timer to use (each wind force gets its own oscillation phase).
    glm::vec3 ComputeWindInfluence(uint32_t ForceIndex, float Strength, const glm::vec3& Direction, float Period, float DeltaTime);

    void SolveDrag(uint32_t StartParticleIndex, uint32_t Count, const float DragCoefficient,
        const float DeltaTime);

    void SolveVortex(uint32_t StartParticleIndex, uint32_t Count, const float VortexStrength, const float VortexPull,
        const float DeltaTime, const glm::vec3& VortexCenter);

    void SolvePointForce(uint32_t StartParticleIndex, uint32_t Count,
        const glm::vec3& ForcePosition, const float Strength, const float Radius, const float DeltaTime);


    // These are some SIMD implementations for particle update functions I wrote at the start
    // Halfway through I realized that compiler auto-vectorizations might do a better job than I do
    // But I am keeping them here for now
    template<SIMDLevel Level>
    void UpdateParticlePositionForAxis(float* StartParticlePtr, uint32_t Count, const float* Velocity, float DeltaTime);

    template<SIMDLevel Level>
    void UpdateParticleLifeTime(float* StartParticlePtr, uint32_t Count, float DeltaTime);

    template<SIMDLevel Level>
    void SolveGravity(float* StartParticlePtr, uint32_t Count, float GravityScale, float DeltaTime);

    template<SIMDLevel Level>
    void SolvePointForce_Vector(uint32_t StartParticleIndex, uint32_t Count, const glm::vec3& ForcePosition,
        const float Strength, const float Radius, const float DeltaTime);

    template<SIMDLevel Level>
    void SolveVortex_Vector(uint32_t StartParticleIndex, uint32_t Count, const float VortexStrength, const float VortexPull,
        const float DeltaTime, const glm::vec3& VortexCenter);

    // Helper to create a float array aligned to specified boundary
    AlignedArray AllocateAlignedArray(size_t NumElements, size_t Alignment);
private:
    ParticleStates m_ParticleStates;
    std::unique_ptr<VThreadPool> m_VThreadPool;
    SIMDManager m_SIMDManager;

    // States trackers
    uint32_t m_ParticleCount = 0;
    SIMDLevel m_SIMDLevel = SIMDLevel::SSE2;
    uint32_t m_SIMDWidth = 4;
    // Per-wind oscillation timers, we use the indices we already got for each force
    // To index into this
    float m_WindTimers[Commons::Constants::MAX_NUM_FORCES] = {};
    float m_TimeSinceLastBurst = 0.f;
    float m_EmitterLifeTime = 0.f;
    // Constants
    static constexpr uint32_t NUM_MAX_PARTICLES = 55000;
    static constexpr uint32_t NUM_THREADS_USED = 16;
    static constexpr float KILL_Y = -1000.f;

};



