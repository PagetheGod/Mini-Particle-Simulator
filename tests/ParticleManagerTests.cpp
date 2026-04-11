#include "ParticleManager.hpp"
#include "ParticlePresets.hpp"
#include "TestHarness.hpp"

#include <array>
#include <utility>

namespace {

constexpr float kEpsilon = 0.001f;

ParticleSimulatorConfig MakeDeterministicConfig()
{
    ParticleSimulatorConfig config{};
    config.Mode = EmitterMode::Continuous;
    config.Shape = SpawnShape::Sphere;
    config.SphereRadius = 1.0f;
    config.EmissionRate = 8;
    config.EmitterLifeTime = 5.0f;
    config.BurstInterval = 0.5f;
    config.StartColor = glm::vec3(0.2f, 0.4f, 0.6f);
    config.EndColor = glm::vec3(0.9f, 0.8f, 0.1f);
    config.LifeTime = glm::vec2(2.0f, 2.0f);
    config.Scale = glm::vec2(1.5f, 1.5f);
    config.Speed = glm::vec2(3.0f, 3.0f);
    config.IsRandomColor = false;
    config.IsScalingColor = false;
    config.IsRandomLifeTime = false;
    config.IsRandomScale = false;
    config.IsRandomSpeed = false;
    config.ForceConfigData.Gravity = 0.0f;
    config.ForceConfigData.ExtraForceCount = 0;
    return config;
}

ParticleSimulatorConfig MakeStationaryRingConfig()
{
    ParticleSimulatorConfig config{};
    config.Mode = EmitterMode::Continuous;
    config.Shape = SpawnShape::Ring;
    config.RingDimensions = glm::vec3(5.0f, 5.0f, 0.0f);
    config.EmissionRate = 4;
    config.EmitterLifeTime = 5.0f;
    config.BurstInterval = 0.5f;
    config.StartColor = glm::vec3(0.1f, 0.2f, 0.9f);
    config.EndColor = glm::vec3(0.9f, 0.7f, 0.1f);
    config.LifeTime = glm::vec2(2.0f, 2.0f);
    config.Scale = glm::vec2(1.0f, 1.0f);
    config.Speed = glm::vec2(0.0f, 0.0f);
    config.IsRandomColor = false;
    config.IsScalingColor = false;
    config.IsRandomLifeTime = false;
    config.IsRandomScale = false;
    config.IsRandomSpeed = false;
    config.ForceConfigData.Gravity = 0.0f;
    config.ForceConfigData.ExtraForceCount = 0;
    return config;
}

float SpawnDeltaTimeForPreset(const ParticleSimulatorConfig& config)
{
    if (config.Mode == EmitterMode::Burst)
    {
        return config.BurstInterval > 0.0f ? config.BurstInterval : 0.25f;
    }
    return 0.25f;
}

}  // namespace

TEST_CASE(ParticleManager_ContinuousEmitterSpawnsFloorOfRateTimesDeltaTime)
{
    ParticleManager particles;
    particles.InitializeParticles();

    ParticleSimulatorConfig config = MakeDeterministicConfig();
    config.Mode = EmitterMode::Continuous;
    config.EmissionRate = 12;

    particles.ParticleFrame(0.25f, config, true);

    REQUIRE(particles.GetParticleCount() == 3);
}

TEST_CASE(ParticleManager_BurstEmitterWaitsForInterval)
{
    ParticleManager particles;
    particles.InitializeParticles();

    ParticleSimulatorConfig config = MakeDeterministicConfig();
    config.Mode = EmitterMode::Burst;
    config.BurstInterval = 0.5f;
    config.EmissionRate = 4;

    particles.ParticleFrame(0.25f, config, true);
    REQUIRE(particles.GetParticleCount() == 0);

    particles.ParticleFrame(0.25f, config, false);
    REQUIRE(particles.GetParticleCount() == 4);
}

TEST_CASE(ParticleManager_SpawnedParticlesUseFixedAttributesWhenRandomnessIsDisabled)
{
    ParticleManager particles;
    particles.InitializeParticles();

    ParticleSimulatorConfig config = MakeDeterministicConfig();
    config.EmissionRate = 4;

    particles.ParticleFrame(0.25f, config, true);

    REQUIRE(particles.GetParticleCount() == 1);

    const glm::vec3 color = particles.GetParticleColor(0);
    REQUIRE_NEAR(color.r, config.StartColor.r, kEpsilon);
    REQUIRE_NEAR(color.g, config.StartColor.g, kEpsilon);
    REQUIRE_NEAR(color.b, config.StartColor.b, kEpsilon);
    REQUIRE_NEAR(particles.GetParticleScale(0), config.Scale.x, kEpsilon);
    REQUIRE_NEAR(particles.GetParticleLifeTime(0), 1.75f, kEpsilon);
    REQUIRE_NEAR(particles.GetParticleRelLifeTime(0), 1.75f / 2.0f, kEpsilon);
}

TEST_CASE(ParticleManager_ExpiredParticlesAreRemovedOnLaterFrames)
{
    ParticleManager particles;
    particles.InitializeParticles();

    ParticleSimulatorConfig spawn_config = MakeDeterministicConfig();
    spawn_config.EmissionRate = 2;
    spawn_config.LifeTime = glm::vec2(1.0f, 1.0f);

    particles.ParticleFrame(0.5f, spawn_config, true);
    REQUIRE(particles.GetParticleCount() == 1);
    REQUIRE_NEAR(particles.GetParticleLifeTime(0), 0.5f, kEpsilon);

    ParticleSimulatorConfig update_config = spawn_config;
    update_config.EmissionRate = 0;

    particles.ParticleFrame(1.0f, update_config, false);
    REQUIRE(particles.GetParticleCount() == 0);
}

TEST_CASE(ParticleManager_ConfigDirtyResetsExistingParticlesBeforeRespawning)
{
    ParticleManager particles;
    particles.InitializeParticles();

    ParticleSimulatorConfig first_config = MakeDeterministicConfig();
    first_config.EmissionRate = 8;
    particles.ParticleFrame(0.5f, first_config, true);
    REQUIRE(particles.GetParticleCount() == 4);

    ParticleSimulatorConfig second_config = MakeDeterministicConfig();
    second_config.EmissionRate = 2;
    particles.ParticleFrame(0.5f, second_config, true);

    REQUIRE(particles.GetParticleCount() == 1);
}

TEST_CASE(ParticleManager_GravityForceMovesStationaryParticleDownward)
{
    ParticleManager particles;
    particles.InitializeParticles();

    ParticleSimulatorConfig spawn_config = MakeStationaryRingConfig();
    particles.ParticleFrame(0.25f, spawn_config, true);
    REQUIRE(particles.GetParticleCount() == 1);
    REQUIRE_NEAR(particles.GetParticlePos(0).y, 0.0f, kEpsilon);

    ParticleSimulatorConfig gravity_config = spawn_config;
    gravity_config.EmissionRate = 0;
    gravity_config.ForceConfigData.Gravity = 1.0f;

    particles.ParticleFrame(0.1f, gravity_config, false);

    REQUIRE(particles.GetParticleCount() == 1);
    REQUIRE_NEAR(particles.GetParticlePos(0).y, -0.0981f, 0.0005f);
}

TEST_CASE(ParticleManager_DirectionalForceMovesParticleByExpectedAmount)
{
    ParticleManager particles;
    particles.InitializeParticles();

    ParticleSimulatorConfig spawn_config = MakeStationaryRingConfig();
    particles.ParticleFrame(0.25f, spawn_config, true);
    REQUIRE(particles.GetParticleCount() == 1);
    const float x_before = particles.GetParticlePos(0).x;

    ParticleSimulatorConfig wind_config = spawn_config;
    wind_config.EmissionRate = 0;
    wind_config.ForceConfigData.ExtraForceCount = 1;
    wind_config.ForceConfigData.ForceTypes[0] = ForceType::Directional;
    wind_config.ForceConfigData.IsForceEnabled[0] = true;
    wind_config.ForceConfigData.ForceDataArray[0].Direction = glm::vec3(1.0f, 0.0f, 0.0f);
    wind_config.ForceConfigData.ForceDataArray[0].Strength = 4.0f;
    wind_config.ForceConfigData.ForceDataArray[0].WindPeriod = 1.0f;

    particles.ParticleFrame(0.25f, wind_config, false);

    REQUIRE(particles.GetParticleCount() == 1);
    REQUIRE_NEAR(particles.GetParticlePos(0).x - x_before, 0.25f, 0.0005f);
}

TEST_CASE(ParticleManager_ColorScalingInterpolatesBetweenStartAndEndColors)
{
    ParticleManager particles;
    particles.InitializeParticles();

    ParticleSimulatorConfig spawn_config = MakeStationaryRingConfig();
    particles.ParticleFrame(0.25f, spawn_config, true);
    REQUIRE(particles.GetParticleCount() == 1);

    ParticleSimulatorConfig scaling_config = spawn_config;
    scaling_config.EmissionRate = 0;
    scaling_config.IsScalingColor = true;

    particles.ParticleFrame(0.5f, scaling_config, false);

    const glm::vec3 color = particles.GetParticleColor(0);
    const glm::vec3 expected = spawn_config.StartColor * 0.875f + spawn_config.EndColor * 0.125f;
    REQUIRE_NEAR(color.r, expected.r, kEpsilon);
    REQUIRE_NEAR(color.g, expected.g, kEpsilon);
    REQUIRE_NEAR(color.b, expected.b, kEpsilon);
}

TEST_CASE(ParticlePresets_HaveSaneRanges)
{
    constexpr std::array<std::pair<const char*, const ParticleSimulatorConfig*>, 6> presets = {{
        {"OmniDirectionalBurst", &ParticlePresets::OmniDirectionalBurst},
        {"Firework", &ParticlePresets::Firework},
        {"Fountain", &ParticlePresets::Fountain},
        {"Vortex", &ParticlePresets::Vortex},
        {"Waterfall", &ParticlePresets::Waterfall},
        {"Snow", &ParticlePresets::Snow},
    }};

    for (const auto& [name, preset] : presets)
    {
        REQUIRE(name[0] != '\0');
        REQUIRE(preset->EmissionRate > 0);
        REQUIRE(preset->EmitterLifeTime > 0.0f);
        REQUIRE(preset->LifeTime.x > 0.0f);
        REQUIRE(preset->LifeTime.x <= preset->LifeTime.y);
        REQUIRE(preset->Scale.x > 0.0f);
        REQUIRE(preset->Scale.x <= preset->Scale.y);
        REQUIRE(preset->Speed.x >= 0.0f);
        REQUIRE(preset->Speed.x <= preset->Speed.y);
        REQUIRE(preset->ForceConfigData.ExtraForceCount <= Commons::Constants::MAX_NUM_FORCES);
    }
}

TEST_CASE(ParticlePresets_CanSpawnAtLeastOneParticle)
{
    constexpr std::array<const ParticleSimulatorConfig*, 6> presets = {{
        &ParticlePresets::OmniDirectionalBurst,
        &ParticlePresets::Firework,
        &ParticlePresets::Fountain,
        &ParticlePresets::Vortex,
        &ParticlePresets::Waterfall,
        &ParticlePresets::Snow,
    }};

    for (const ParticleSimulatorConfig* preset : presets)
    {
        ParticleManager particles;
        particles.InitializeParticles();

        particles.ParticleFrame(SpawnDeltaTimeForPreset(*preset), *preset, true);

        REQUIRE(particles.GetParticleCount() > 0);
    }
}
