#include "ParticleManager.hpp"
#include "ParticlePresets.hpp"
#include "TestHarness.hpp"

#include <array>
#include <utility>

namespace {

// Shared epsilon for comparisons against simulation output.
constexpr float kEpsilon = 0.001f;

ParticleSimulatorConfig MakeDeterministicConfig()
{
    // Build a config where every "random" property is pinned to one value, so
    // the tests can assert exact particle attributes after spawning.
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
    // Build a config that spawns particles with zero initial velocity so force
    // tests measure only the force under test, not spawn momentum.
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
    // Burst presets need enough delta time to cross the burst interval once.
    if (config.Mode == EmitterMode::Burst)
    {
        return config.BurstInterval > 0.0f ? config.BurstInterval : 0.25f;
    }
    // Continuous presets only need a small positive frame to emit particles.
    return 0.25f;
}

}  // namespace

TEST_CASE(ParticleManager_ContinuousEmitterSpawnsFloorOfRateTimesDeltaTime)
{
    // Start from a fresh particle pool.
    ParticleManager particles;
    particles.InitializeParticles();

    // With 12 particles/second over 0.25 seconds, floor(rate * dt) = 3.
    ParticleSimulatorConfig config = MakeDeterministicConfig();
    config.Mode = EmitterMode::Continuous;
    config.EmissionRate = 12;

    // The first frame both resets dirty config state and performs spawning.
    particles.ParticleFrame(0.25f, config, true);

    REQUIRE(particles.GetParticleCount() == 3);
}

TEST_CASE(ParticleManager_BurstEmitterWaitsForInterval)
{
    // Burst emitters should not fire until the full interval has elapsed.
    ParticleManager particles;
    particles.InitializeParticles();

    ParticleSimulatorConfig config = MakeDeterministicConfig();
    config.Mode = EmitterMode::Burst;
    config.BurstInterval = 0.5f;
    config.EmissionRate = 4;

    // Half the interval should still produce no burst.
    particles.ParticleFrame(0.25f, config, true);
    REQUIRE(particles.GetParticleCount() == 0);

    // Crossing the interval boundary on the next frame should emit all 4.
    particles.ParticleFrame(0.25f, config, false);
    REQUIRE(particles.GetParticleCount() == 4);
}

TEST_CASE(ParticleManager_SpawnedParticlesUseFixedAttributesWhenRandomnessIsDisabled)
{
    // Deterministic config lets us assert exact color, size, and lifetime.
    ParticleManager particles;
    particles.InitializeParticles();

    ParticleSimulatorConfig config = MakeDeterministicConfig();
    config.EmissionRate = 4;

    particles.ParticleFrame(0.25f, config, true);

    // 4 particles/second over 0.25 seconds should spawn exactly one particle.
    REQUIRE(particles.GetParticleCount() == 1);

    // The spawned color should match the configured start color exactly.
    const glm::vec3 color = particles.GetParticleColor(0);
    REQUIRE_NEAR(color.r, config.StartColor.r, kEpsilon);
    REQUIRE_NEAR(color.g, config.StartColor.g, kEpsilon);
    REQUIRE_NEAR(color.b, config.StartColor.b, kEpsilon);
    // Scale should match the non-random configured scalar.
    REQUIRE_NEAR(particles.GetParticleScale(0), config.Scale.x, kEpsilon);
    // Lifetime drops immediately by the frame delta after the spawn/update pass.
    REQUIRE_NEAR(particles.GetParticleLifeTime(0), 1.75f, kEpsilon);
    // Relative lifetime should track remaining / original lifetime.
    REQUIRE_NEAR(particles.GetParticleRelLifeTime(0), 1.75f / 2.0f, kEpsilon);
}

TEST_CASE(ParticleManager_ExpiredParticlesAreRemovedOnLaterFrames)
{
    // Spawn one short-lived particle, then advance past its remaining life.
    ParticleManager particles;
    particles.InitializeParticles();

    ParticleSimulatorConfig spawn_config = MakeDeterministicConfig();
    spawn_config.EmissionRate = 2;
    spawn_config.LifeTime = glm::vec2(1.0f, 1.0f);

    // First half-second spawns one particle and reduces its lifetime to 0.5.
    particles.ParticleFrame(0.5f, spawn_config, true);
    REQUIRE(particles.GetParticleCount() == 1);
    REQUIRE_NEAR(particles.GetParticleLifeTime(0), 0.5f, kEpsilon);

    // Disable further spawning so only expiry behavior is under test.
    ParticleSimulatorConfig update_config = spawn_config;
    update_config.EmissionRate = 0;

    // One more full second should expire and cull the particle.
    particles.ParticleFrame(1.0f, update_config, false);
    REQUIRE(particles.GetParticleCount() == 0);
}

TEST_CASE(ParticleManager_ConfigDirtyResetsExistingParticlesBeforeRespawning)
{
    // Dirty config updates should clear old particles before spawning with the
    // new settings, instead of mixing old and new populations together.
    ParticleManager particles;
    particles.InitializeParticles();

    ParticleSimulatorConfig first_config = MakeDeterministicConfig();
    first_config.EmissionRate = 8;
    // 8 * 0.5 seconds = 4 initial particles.
    particles.ParticleFrame(0.5f, first_config, true);
    REQUIRE(particles.GetParticleCount() == 4);

    ParticleSimulatorConfig second_config = MakeDeterministicConfig();
    second_config.EmissionRate = 2;
    // Marking the config dirty should reset the pool before the next spawn.
    particles.ParticleFrame(0.5f, second_config, true);

    // After reset, only the new config's one spawned particle should remain.
    REQUIRE(particles.GetParticleCount() == 1);
}

TEST_CASE(ParticleManager_GravityForceMovesStationaryParticleDownward)
{
    // Use a zero-velocity spawn so gravity is the only source of motion.
    ParticleManager particles;
    particles.InitializeParticles();

    ParticleSimulatorConfig spawn_config = MakeStationaryRingConfig();
    particles.ParticleFrame(0.25f, spawn_config, true);
    // Freshly spawned stationary particle should start on the emitter plane.
    REQUIRE(particles.GetParticleCount() == 1);
    REQUIRE_NEAR(particles.GetParticlePos(0).y, 0.0f, kEpsilon);

    // Disable new emission and turn on gravity for the update step.
    ParticleSimulatorConfig gravity_config = spawn_config;
    gravity_config.EmissionRate = 0;
    gravity_config.ForceConfigData.Gravity = 1.0f;

    // Advance one small step so the expected displacement stays easy to compute.
    particles.ParticleFrame(0.1f, gravity_config, false);

    REQUIRE(particles.GetParticleCount() == 1);
    // Expected drop matches the simulator's gravity integration for this step.
    REQUIRE_NEAR(particles.GetParticlePos(0).y, -0.0981f, 0.0005f);
}

TEST_CASE(ParticleManager_DirectionalForceMovesParticleByExpectedAmount)
{
    // Start from a stationary spawn so wind displacement is isolated.
    ParticleManager particles;
    particles.InitializeParticles();

    ParticleSimulatorConfig spawn_config = MakeStationaryRingConfig();
    particles.ParticleFrame(0.25f, spawn_config, true);
    REQUIRE(particles.GetParticleCount() == 1);
    // Record the baseline x position before the directional force is applied.
    const float x_before = particles.GetParticlePos(0).x;

    // Configure one enabled directional force that points along +X.
    ParticleSimulatorConfig wind_config = spawn_config;
    wind_config.EmissionRate = 0;
    wind_config.ForceConfigData.ExtraForceCount = 1;
    wind_config.ForceConfigData.ForceTypes[0] = ForceType::Directional;
    wind_config.ForceConfigData.IsForceEnabled[0] = true;
    wind_config.ForceConfigData.ForceDataArray[0].Direction = glm::vec3(1.0f, 0.0f, 0.0f);
    wind_config.ForceConfigData.ForceDataArray[0].Strength = 4.0f;
    wind_config.ForceConfigData.ForceDataArray[0].WindPeriod = 1.0f;

    // Step the simulation forward with only the wind force active.
    particles.ParticleFrame(0.25f, wind_config, false);

    REQUIRE(particles.GetParticleCount() == 1);
    // The branch's directional-force math should move the particle by 0.25 units.
    REQUIRE_NEAR(particles.GetParticlePos(0).x - x_before, 0.25f, 0.0005f);
}

TEST_CASE(ParticleManager_ColorScalingInterpolatesBetweenStartAndEndColors)
{
    // Spawn exactly one particle, then enable color scaling on the next frame.
    ParticleManager particles;
    particles.InitializeParticles();

    ParticleSimulatorConfig spawn_config = MakeStationaryRingConfig();
    particles.ParticleFrame(0.25f, spawn_config, true);
    REQUIRE(particles.GetParticleCount() == 1);

    ParticleSimulatorConfig scaling_config = spawn_config;
    scaling_config.EmissionRate = 0;
    scaling_config.IsScalingColor = true;

    // Advance enough time to move 12.5% through the particle's lifetime.
    particles.ParticleFrame(0.5f, scaling_config, false);

    // Read the interpolated color and compare it against the expected blend.
    const glm::vec3 color = particles.GetParticleColor(0);
    const glm::vec3 expected = spawn_config.StartColor * 0.875f + spawn_config.EndColor * 0.125f;
    REQUIRE_NEAR(color.r, expected.r, kEpsilon);
    REQUIRE_NEAR(color.g, expected.g, kEpsilon);
    REQUIRE_NEAR(color.b, expected.b, kEpsilon);
}

TEST_CASE(ParticlePresets_HaveSaneRanges)
{
    // Collect every shipped preset so one loop can sanity-check all of them.
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
        // Names should exist so UI preset selection remains readable.
        REQUIRE(name[0] != '\0');
        // Emission and lifetime ranges should be positive and ordered.
        REQUIRE(preset->EmissionRate > 0);
        REQUIRE(preset->EmitterLifeTime > 0.0f);
        REQUIRE(preset->LifeTime.x > 0.0f);
        REQUIRE(preset->LifeTime.x <= preset->LifeTime.y);
        // Scale and speed ranges should also be valid min/max pairs.
        REQUIRE(preset->Scale.x > 0.0f);
        REQUIRE(preset->Scale.x <= preset->Scale.y);
        REQUIRE(preset->Speed.x >= 0.0f);
        REQUIRE(preset->Speed.x <= preset->Speed.y);
        // Extra forces must stay within the fixed-size force arrays.
        REQUIRE(preset->ForceConfigData.ExtraForceCount <= Commons::Constants::MAX_NUM_FORCES);
    }
}

TEST_CASE(ParticlePresets_CanSpawnAtLeastOneParticle)
{
    // Each preset should be capable of producing visible output on its first
    // meaningful frame; otherwise the preset feels broken in the UI.
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
        // Use a fresh particle manager for each preset to avoid cross-contamination.
        ParticleManager particles;
        particles.InitializeParticles();

        // Advance by enough time for the preset's emission mode to fire once.
        particles.ParticleFrame(SpawnDeltaTimeForPreset(*preset), *preset, true);

        // A valid preset should leave at least one live particle behind.
        REQUIRE(particles.GetParticleCount() > 0);
    }
}
