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
        .BurstCount = 1000,
        .StartColor = glm::vec3(0.9f, 0.6f, 0.2f),
        .EndColor = glm::vec3(0.9f, 0.6f, 0.2f),
        .SphereRadius = 0.5f,
        .LifeTime = glm::vec2(0.5f, 2.5f),
        .Scale = glm::vec2(0.3f, 0.3f),
        .BaseSpeedMin = 5.f,
        .BaseSpeedMax = 15.f,
        .Gravity = 2.f,
        .ForceConfigData = { .Gravity = 2.f },
        .Preset = PresetType::OmniDirectionalBurst,
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
        .EmissionRate = 3000.f,
        .StartColor = glm::vec3(1.f, 0.3f, 0.3f),
        .EndColor = glm::vec3(1.f, 0.3f, 0.3f),
        .ConeDimensions = glm::vec2(3.f, 25.f),
        .LifeTime = glm::vec2(1.f, 3.f),
        .Scale = glm::vec2(0.25f, 0.25f),
        .BaseSpeedMin = 10.f,
        .BaseSpeedMax = 20.f,
        .Gravity = 5.f,
        .ForceConfigData = { .Gravity = 5.f },
        .Preset = PresetType::Firework,
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
        .EmissionRate = 500.f,
        .StartColor = glm::vec3(0.3f, 0.6f, 1.f),
        .EndColor = glm::vec3(0.3f, 0.6f, 1.f),
        .ConeDimensions = glm::vec2(2.f, 12.f),
        .LifeTime = glm::vec2(2.f, 4.f),
        .Scale = glm::vec2(0.2f, 0.2f),
        .BaseSpeedMin = 4.f,
        .BaseSpeedMax = 10.f,
        .Gravity = 3.f,
        .ForceConfigData = { .Gravity = 3.f },
        .Preset = PresetType::Fountain,
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
        .EmissionRate = 300.f,
        .StartColor = glm::vec3(0.6f, 0.2f, 0.9f),
        .EndColor = glm::vec3(0.6f, 0.2f, 0.9f),
        .RingDimensions = glm::vec2(0.5f, 2.f),
        .LifeTime = glm::vec2(3.f, 6.f),
        .Scale = glm::vec2(0.2f, 0.2f),
        .BaseSpeedMin = 1.f,
        .BaseSpeedMax = 3.f,
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
        .Preset = PresetType::Vortex,
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
        .EmissionRate = 800.f,
        .StartColor = glm::vec3(0.4f, 0.7f, 1.f),
        .EndColor = glm::vec3(0.4f, 0.7f, 1.f),
        .BoxDimensions = glm::vec3(6.f, 0.1f, 1.f),
        .LifeTime = glm::vec2(2.f, 4.f),
        .Scale = glm::vec2(0.15f, 0.15f),
        .BaseSpeedMin = 0.5f,
        .BaseSpeedMax = 2.f,
        .Gravity = 6.f,
        .ForceConfigData = { .Gravity = 6.f },
        .Preset = PresetType::Waterfall,
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
        .EmissionRate = 150.f,
        .StartColor = glm::vec3(0.95f, 0.95f, 1.f),
        .EndColor = glm::vec3(0.95f, 0.95f, 1.f),
        .BoxDimensions = glm::vec3(10.f, 0.f, 10.f),
        .LifeTime = glm::vec2(4.f, 8.f),
        .Scale = glm::vec2(0.1f, 0.1f),
        .BaseSpeedMin = 0.3f,
        .BaseSpeedMax = 0.8f,
        .Gravity = 0.3f,
        .ForceConfigData = {
            .Gravity = 0.3f,
            .ForceDataArray = { ForceData{
                .Direction = glm::vec3(-0.5f, 0.f, 0.2f),
                .Strength = 0.8f,
                .Frequency = 0.4f
            } },
            .ForceTypes = { ForceType::Directional },
            .IsForceEnabled = { true },
            .ExtraForceCount = 1
        },
        .Preset = PresetType::Snow,
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
