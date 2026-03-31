#pragma once
#include <cstdint>
#include <memory>
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include "Commons.hpp"
#include "VThreadPool.hpp"
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
        float Frequency;
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
    union
    {
        int BurstCount = 0;
        float EmissionRate;
    };
    glm::vec3 StartColor = glm::vec3(1.f);
    glm::vec3 EndColor = glm::vec3(1.f);
    // Shape specific data...
    // Sphere - just a radius
    // Cone - does not need an axis(always up), needs a spread(phi), and a rotation(theta)
    // Box/Plane - needs three floats(width, height, depth), set one of the three to 0 to make it a plane
    // Ring/Disc - filled circle needing two floats: min and max radius
    // Cylinder - needs two floats, radius and height
    union
    {
        glm::vec3 BoxDimensions = glm::vec3(1.f); // Width, height, depth
        glm::vec2 ConeDimensions; // Height, spread(half-angle), rotation
        glm::vec2 RingDimensions; // Min and max radius
        glm::vec2 CylinderDimensions; // Radius, height
        float SphereRadius;
    };
    glm::vec2 LifeTime = glm::vec2(1.f, 2.f); // Min and max life time
    glm::vec2 Scale = glm::vec2(1.f, 2.f); // Min and max scale
    float BaseSpeedMin = 1.f;
    float BaseSpeedMax = 1.f;
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
 * Also, fun thing I learend, despite being called a custom "deleter", the second template argument to unique_ptr
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


class ParticleManager {
public:
    //Constructors and destructors
    ParticleManager() = default;

    ParticleManager(const ParticleManager&) = delete;// Same with application class, makes no sense to copy or move
    ParticleManager& operator=(const ParticleManager&) = delete;
    ParticleManager(ParticleManager&&) = delete;
    ParticleManager& operator=(ParticleManager&&) = delete;

    ~ParticleManager() = default;

    //Actual work functions
    void InitializeParticles();
    void SpawnParticles(const ParticleSimulatorConfig& Config);
    void UpdateParticles(float DeltaTime);
    glm::vec3 SolveForces(const ForceConfig& ForceConfigData);
    void SolveWind(const ForceConfig& ForceConfigData, uint32_t BeginIndex, uint32_t EndIndex);
    void SolveDrag(const ForceConfig& ForceConfigData, uint32_t BeginIndex, uint32_t EndIndex);
    void SolveVortex(const ForceConfig& ForceConfigData, uint32_t BeginIndex, uint32_t EndIndex);
    void SolvePointForce(const ForceConfig& ForceConfigData, uint32_t BeginIndex, uint32_t EndIndex);
    void UpdateParticlePositions(uint32_t BeginIndex, uint32_t EndIndex);
    void UpdateParticleLifeTime(uint32_t BeginIndex, uint32_t EndIndex);
    void UpdateParticleColor(uint32_t BeginIndex, uint32_t EndIndex);
    void UpdateParticleScale(uint32_t BeginIndex, uint32_t EndIndex);
    //Getters and setters
    [[nodiscard]] uint32_t GetParticleCount() const {
        return m_ParticleCount;
    };
public:

private:
    // Helper to create a float array aligned to specified boundary
    AlignedArray AllocateAlignedArray(size_t NumElements, size_t Alignment);
private:
    ParticleStates m_ParticleStates;
    std::unique_ptr<VThreadPool> m_VThreadPool;

    uint32_t m_ParticleCount = 0;
    glm::vec3 m_EmitterPosition = glm::vec3(0.f);

    // Constants
    static constexpr uint32_t NUM_MAX_PARTICLES = 65000;
    static constexpr uint32_t NUM_THREADS_USED = 16;
};


