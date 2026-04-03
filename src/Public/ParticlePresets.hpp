//
// Created by YWvin on 2026/3/29.
//

#ifndef MINIPARTICLESIMULATOR_PARTICLEPRESETS_HPP
#define MINIPARTICLESIMULATOR_PARTICLEPRESETS_HPP
#include "ParticleManager.hpp"

namespace ParticlePresets
{
    // Explosion from a point — particles fly outward in all directions
    static constexpr ParticleSimulatorConfig OmniDirectionalBurst {
        .BurstInterval = 1.f,
        .StartColor = glm::vec3(0.9f, 0.6f, 0.2f),
        .EndColor = glm::vec3(0.9f, 0.6f, 0.2f),
        .SphereRadius = 0.5f,
        .LifeTime = glm::vec2(0.5f, 2.5f),
        .Scale = glm::vec2(0.3f, 0.3f),
        .Speed = glm::vec2(10.f, 15.f),
        .Gravity = 2.f,
        .ForceConfigData = { .Gravity = 2.f },
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
        .EmissionRate = 3000,
        .StartColor = glm::vec3(1.f, 0.3f, 0.3f),
        .EndColor = glm::vec3(1.f, 0.3f, 0.3f),
        .ConeDimensions = glm::vec2(3.f, 25.f),
        .LifeTime = glm::vec2(1.f, 3.f),
        .Scale = glm::vec2(0.25f, 0.25f),
        .Speed = glm::vec2(10.f, 20.f),
        .Gravity = 5.f,
        .ForceConfigData = { .Gravity = 5.f },
        .Shape = SpawnShape::Cone,
        .Mode = EmitterMode::Continuous,
        .IsRandomColor = true,
        .IsScalingColor = false,
        .IsRandomLifeTime = true,
        .IsRandomScale = false,
        .IsRandomSpeed = true
    };

    // Narrow upward stream that falls back like water
    static constexpr ParticleSimulatorConfig Fountain {
        .EmissionRate = 500,
        .StartColor = glm::vec3(0.3f, 0.6f, 1.f),
        .EndColor = glm::vec3(0.3f, 0.6f, 1.f),
        .ConeDimensions = glm::vec2(2.f, 12.f),
        .LifeTime = glm::vec2(2.f, 4.f),
        .Scale = glm::vec2(0.2f, 0.2f),
        .Speed = glm::vec2(4.f, 15.f),
        .Gravity = 3.f,
        .ForceConfigData = { .Gravity = 3.f },
        .Shape = SpawnShape::Cone,
        .Mode = EmitterMode::Continuous,
        .IsRandomColor = true,
        .IsScalingColor = false,
        .IsRandomLifeTime = true,
        .IsRandomScale = false,
        .IsRandomSpeed = true
    };

    // Particles spiral around a central axis
    static constexpr ParticleSimulatorConfig Vortex {
        .EmissionRate = 300,
        .StartColor = glm::vec3(0.6f, 0.2f, 0.9f),
        .EndColor = glm::vec3(0.6f, 0.2f, 0.9f),
        .RingDimensions = glm::vec2(0.5f, 2.f),
        .LifeTime = glm::vec2(3.f, 6.f),
        .Scale = glm::vec2(0.2f, 0.2f),
        .Speed = glm::vec2(1.f, 3.5f),
        .Gravity = 0.3f,
        .ForceConfigData = {
            .Gravity = 0.3f,
            .ForceDataArray = { ForceData{
                .Direction = glm::vec3(0.f, 0.f, 0.f),
                .Strength = 5.f,
                .VortexPull = 0.3f
            } },
            .ForceTypes = { ForceType::Vortex },
            .IsForceEnabled = { true },
            .ExtraForceCount = 1
        },
        .Shape = SpawnShape::Ring,
        .Mode = EmitterMode::Continuous,
        .IsRandomColor = false,
        .IsScalingColor = false,
        .IsRandomLifeTime = true,
        .IsRandomScale = false,
        .IsRandomSpeed = true
    };

    // Wide sheet of particles cascading downward
    static constexpr ParticleSimulatorConfig Waterfall {
        .EmissionRate = 800,
        .StartColor = glm::vec3(0.4f, 0.7f, 1.f),
        .EndColor = glm::vec3(0.4f, 0.7f, 1.f),
        .BoxDimensions = glm::vec3(6.f, 0.1f, 1.f),
        .LifeTime = glm::vec2(2.f, 4.f),
        .Scale = glm::vec2(0.15f, 0.15f),
        .Speed = glm::vec2(0.5f, 2.5f),
        .Gravity = 6.f,
        .ForceConfigData = { .Gravity = 6.f },
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
        .EmissionRate = 150,
        .StartColor = glm::vec3(0.95f, 0.95f, 1.f),
        .EndColor = glm::vec3(0.95f, 0.95f, 1.f),
        .BoxDimensions = glm::vec3(10.f, 0.f, 10.f),
        .LifeTime = glm::vec2(4.f, 8.f),
        .Scale = glm::vec2(0.1f, 0.1f),
        .Speed = glm::vec2(0.3f, 0.8f),
        .Gravity = 0.3f,
        .ForceConfigData = {
            .Gravity = 0.3f,
            .ForceDataArray = { ForceData{
                .Direction = glm::vec3(-0.5f, 0.f, 0.2f),
                .Strength = 0.8f,
                .WindPeriod = 1.5f
            } },
            .ForceTypes = { ForceType::Directional },
            .IsForceEnabled = { true },
            .ExtraForceCount = 1
        },
        .Shape = SpawnShape::Box,
        .Mode = EmitterMode::Continuous,
        .IsRandomColor = false,
        .IsScalingColor = false,
        .IsRandomLifeTime = true,
        .IsRandomScale = false,
        .IsRandomSpeed = true
    };
}

#endif //MINIPARTICLESIMULATOR_PARTICLEPRESETS_HPP
