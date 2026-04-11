#include "ParticleManager.hpp"
#include "TestHarness.hpp"

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
