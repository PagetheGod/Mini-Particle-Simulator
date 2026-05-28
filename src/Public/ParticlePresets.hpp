#pragma once
#include "ParticleManager.hpp"

#define ENABLE_STRESS_PRESET
namespace ParticlePresets
{
    // Explosion from a point — particles fly outward in all directions
    static constexpr ParticleSimulatorConfig OmniDirectionalBurst {
        .BurstInterval = 1.f,
        .EmissionRate = 1500,
        .EmitterLifeTime = 5.f,
        .StartColor = glm::vec3(0.9f, 0.6f, 0.2f),
        .EndColor = glm::vec3(0.9f, 0.6f, 0.2f),
        .SphereRadius = 3.f,
        .LifeTime = glm::vec2(1.f, 3.f),
        .Scale = glm::vec2(0.4f, 0.4f),
        .Speed = glm::vec2(60.f, 120.f),
        .ForceConfigData = { .Gravity = 1.5f },
        .Shape = SpawnShape::Sphere,
        .Mode = EmitterMode::Burst,
        .IsRandomColor = false,
        .IsScalingColor = false,
        .IsRandomLifeTime = true,
        .IsRandomScale = false,
        .IsRandomSpeed = true
    };

    // Particles shoot upward through a wide cone and arc back down
    static constexpr ParticleSimulatorConfig Firework {
        .EmissionRate = 250,
        .EmitterLifeTime = 5.f,
        .StartColor = glm::vec3(1.f, 0.3f, 0.3f),
        .EndColor = glm::vec3(1.f, 0.3f, 0.3f),
        .ConeDimensions = glm::vec2(5.f, 20.f),
        .LifeTime = glm::vec2(15.f, 15.f),
        .Scale = glm::vec2(1.f, 1.5f),
        .Speed = glm::vec2(80.f, 160.f),
        .ForceConfigData = { .Gravity = 3.f },
        .Shape = SpawnShape::Cone,
        .Mode = EmitterMode::Continuous,
        .IsRandomColor = true,
        .IsScalingColor = false,
        .IsRandomLifeTime = false,
        .IsRandomScale = false,
        .IsRandomSpeed = true
    };

    // Narrow upward stream that falls back like water
    static constexpr ParticleSimulatorConfig Fountain {
        .EmissionRate = 350,
        .EmitterLifeTime = 5.f,
        .StartColor = glm::vec3(0.3f, 0.6f, 1.f),
        .EndColor = glm::vec3(0.3f, 0.6f, 1.f),
        .ConeDimensions = glm::vec2(3.f, 8.f),
        .LifeTime = glm::vec2(2.f, 4.f),
        .Scale = glm::vec2(0.25f, 0.25f),
        .Speed = glm::vec2(50.f, 100.f),
        .ForceConfigData = { .Gravity = 2.5f },
        .Shape = SpawnShape::Cone,
        .Mode = EmitterMode::Continuous,
        .IsRandomColor = true,
        .IsScalingColor = false,
        .IsRandomLifeTime = true,
        .IsRandomScale = false,
        .IsRandomSpeed = true
    };

    // Particles spiral around a central axis uses vortex so particles have
    // vertical and horizontal spreads
    static constexpr ParticleSimulatorConfig Vortex {
        .EmissionRate = 500,
        .EmitterLifeTime = 5.f,
        .StartColor = glm::vec3(0.6f, 0.2f, 0.9f),
        .EndColor = glm::vec3(0.6f, 0.2f, 0.9f),
        .ConeDimensions = glm::vec2(30.f, 30.f),
        .LifeTime = glm::vec2(3.f, 5.f),
        .Scale = glm::vec2(0.25f, 0.25f),
        .Speed = glm::vec2(0.f, 0.2f),
        .ForceConfigData = {
            .Gravity = 0.f,
            .ForceDataArray = { ForceData{
                .Direction = glm::vec3(0.f, 0.f, 0.f),
                .Strength = 5.f,
                .VortexPull = 15.f
            } },
            .ForceTypes = { ForceType::Vortex },
            .IsForceEnabled = { true },
            .ExtraForceCount = 1
        },
        .Shape = SpawnShape::Cone,
        .Mode = EmitterMode::Continuous,
        .IsRandomColor = false,
        .IsScalingColor = false,
        .IsRandomLifeTime = true,
        .IsRandomScale = false,
        .IsRandomSpeed = true
    };

    // Wide sheet of particles cascading downward
    static constexpr ParticleSimulatorConfig Waterfall {
        .EmissionRate = 1000,
        .EmitterLifeTime = 5.f,
        .StartColor = glm::vec3(0.4f, 0.7f, 1.f),
        .EndColor = glm::vec3(0.4f, 0.7f, 1.f),
        .BoxDimensions = glm::vec3(50.f, 0.1f, 1.f),
        .LifeTime = glm::vec2(2.f, 4.f),
        .Scale = glm::vec2(0.15f, 0.15f),
        .Speed = glm::vec2(0.5f, 2.5f),
        .ForceConfigData = { .Gravity = 5.f },
        .Shape = SpawnShape::Box,
        .Mode = EmitterMode::Continuous,
        .IsRandomColor = false,
        .IsScalingColor = false,
        .IsRandomLifeTime = true,
        .IsRandomScale = false,
        .IsRandomSpeed = true
    };

    // Gentle falling particles with slight wind
    static constexpr ParticleSimulatorConfig Snow {
        .EmissionRate = 350,
        .EmitterLifeTime = 7.5f,
        .StartColor = glm::vec3(0.95f, 0.95f, 1.f),
        .EndColor = glm::vec3(0.95f, 0.95f, 1.f),
        .BoxDimensions = glm::vec3(50.f, 0.f, 10.f),
        .LifeTime = glm::vec2(4.f, 8.f),
        .Scale = glm::vec2(0.1f, 0.1f),
        .Speed = glm::vec2(0.3f, 0.8f),
        .ForceConfigData = {
            .Gravity = 0.3f,
            .ForceDataArray = { ForceData{
                .Direction = glm::vec3(-0.5f, 0.f, 0.2f),
                .Strength = 5.f,
                .WindPeriod = 1.5f
            } , ForceData{.Strength = 0.5f}},
            .ForceTypes = { ForceType::Directional , ForceType::Drag},
            .IsForceEnabled = { true, true },
            .ExtraForceCount = 2
        },
        .Shape = SpawnShape::Box,
        .Mode = EmitterMode::Continuous,
        .IsRandomColor = false,
        .IsScalingColor = false,
        .IsRandomLifeTime = true,
        .IsRandomScale = false,
        .IsRandomSpeed = true
    };
#ifdef ENABLE_STRESS_PRESET
    // A preset that forces heavy computations for benchmarking purposes.
    // Goal: drive the particle count to the cap and hold it there so the per-frame SIMD
    // load stays maxed — continuous emission at max rate, max lifetime so nothing expires
    // mid-test, and a net-containing force field (drag + inward vortices + point attractors)
    // so particles stay clustered near the origin, far from the Y kill plane. All 9 force
    // slots are filled because the force COUNT, not their strength, is what drives compute.
    static constexpr ParticleSimulatorConfig Stress {
        .EmissionRate = 20'000,
        .EmitterLifeTime = 15.f,
        .StartColor = glm::vec3(1.f, 0.f, 1.f),
        .EndColor = glm::vec3(0.f, 1.f, 1.f),
        // Cone is the heaviest spawn shape (cbrt/tan/sin/cos per particle); keep it moderate
        .ConeDimensions = glm::vec2(20.f, 30.f), // Height, half-angle in degrees
        .LifeTime = glm::vec2(15.f, 15.f),       // Max lifetime; with no-random the spawner uses .x
        .Scale = glm::vec2(0.1f, 0.4f),          // Min != max so "random size" actually varies
        .Speed = glm::vec2(1.f, 5.f),            // Randomized but slow, so particles linger on screen
        .ForceConfigData = {
            .Gravity = 0.1f,
            // Order here must line up with ForceTypes below. Exactly 9 = MAX_NUM_FORCES
            .ForceDataArray = {
                // [0] Drag — bounds velocities so the other forces can't fling particles away
                ForceData{ .Strength = 2.f },
                // [1][2] Two winds, mostly horizontal, different periods so they don't sync up
                ForceData{ .Direction = glm::vec3(1.f, 0.2f, 0.f), .Strength = 6.f, .WindPeriod = 2.f },
                ForceData{ .Direction = glm::vec3(-0.4f, 0.f, 0.9f), .Strength = 5.f, .WindPeriod = 3.5f },
                // [3][4][5] Three vortices (all share the origin Y-axis) — radial term pulls
                // particles inward, tangential term makes them orbit. Inward = containing
                ForceData{ .Strength = 6.f, .VortexPull = 3.f },
                ForceData{ .Strength = 5.f, .VortexPull = 2.f },
                ForceData{ .Strength = 7.f, .VortexPull = 3.f },
                // [6][7][8] Three point attractors at distinct spots above the origin (Y > 0,
                // so they pull up/sideways, never toward the kill plane)
                ForceData{ .Direction = glm::vec3(20.f, 10.f, 0.f), .Strength = 12.f, .PointRadius = 60.f },
                ForceData{ .Direction = glm::vec3(-20.f, 15.f, 10.f), .Strength = 10.f, .PointRadius = 60.f },
                ForceData{ .Direction = glm::vec3(0.f, 8.f, 15.f), .Strength = 10.f, .PointRadius = 50.f }
            },
            .ForceTypes = {
                ForceType::Drag,
                ForceType::Directional, ForceType::Directional,
                ForceType::Vortex, ForceType::Vortex, ForceType::Vortex,
                ForceType::Point, ForceType::Point, ForceType::Point
            },
            .IsForceEnabled = { true, true, true, true, true, true, true, true, true },
            .ExtraForceCount = 9
        },
        .Shape = SpawnShape::Cone,
        .Mode = EmitterMode::Continuous,
        .IsRandomColor = false,
        .IsScalingColor = true,
        .IsRandomLifeTime = false,
        .IsRandomScale = true,
        .IsRandomSpeed = true
    };
#endif
}

