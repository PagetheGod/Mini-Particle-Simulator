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

union SizeOrNext
{
    float Size;
    uint32_t NextFreeSlot;
};

struct ParticleStates
{
    std::vector<float> Px;
    std::vector<float> Py;
    std::vector<float> Pz;
    std::vector<float> Vx;
    std::vector<float> Vy;
    std::vector<float> Vz;
    std::vector<float> R;
    std::vector<float> G;
    std::vector<float> B;
    std::vector<SizeOrNext> SizeOrNextFree;
    std::vector<float> LifeTime;
    uint32_t FirstAvailable;
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

private:
    ParticleStates m_ParticleStates;
    std::unique_ptr<VThreadPool> m_VThreadPool;

    uint32_t m_ParticleCount = 0;
    glm::vec3 m_EmitterPosition = glm::vec3(0.f);

    // Constants
    static constexpr uint32_t NUM_MAX_PARTICLES = 100000;
    static constexpr uint32_t NUM_THREADS_USED = 16;
};


