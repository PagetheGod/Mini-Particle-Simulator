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
        .EmissionRate = 1500,
        .EmitterLifeTime = 5.f,
        .StartColor = glm::vec3(0.9f, 0.6f, 0.2f),
        .EndColor = glm::vec3(0.9f, 0.6f, 0.2f),
        .SphereRadius = 3.f,
        .LifeTime = glm::vec2(1.f, 3.f),
        .Scale = glm::vec2(0.4f, 0.4f),
        .Speed = glm::vec2(60.f, 120.f),
        .Gravity = 1.5f,
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
        .EmissionRate = 100,
        .EmitterLifeTime = 5.f,
        .StartColor = glm::vec3(1.f, 0.3f, 0.3f),
        .EndColor = glm::vec3(1.f, 0.3f, 0.3f),
        .ConeDimensions = glm::vec2(5.f, 20.f),
        .LifeTime = glm::vec2(1.5f, 3.5f),
        .Scale = glm::vec2(0.3f, 0.3f),
        .Speed = glm::vec2(80.f, 160.f),
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

    // Narrow upward stream that falls back like water
    static constexpr ParticleSimulatorConfig Fountain {
        .EmissionRate = 250,
        .EmitterLifeTime = 5.f,
        .StartColor = glm::vec3(0.3f, 0.6f, 1.f),
        .EndColor = glm::vec3(0.3f, 0.6f, 1.f),
        .ConeDimensions = glm::vec2(3.f, 8.f),
        .LifeTime = glm::vec2(2.f, 4.f),
        .Scale = glm::vec2(0.25f, 0.25f),
        .Speed = glm::vec2(50.f, 100.f),
        .Gravity = 2.5f,
        .ForceConfigData = { .Gravity = 2.5f },
        .Shape = SpawnShape::Cone,
        .Mode = EmitterMode::Continuous,
        .IsRandomColor = true,
        .IsScalingColor = false,
        .IsRandomLifeTime = true,
        .IsRandomScale = false,
        .IsRandomSpeed = true
    };

    // Particles spiral around a central axis — uses ring so particles have
    // vertical spread (Ring is purely XZ which is mostly invisible in 2D)
    static constexpr ParticleSimulatorConfig Vortex {
        .EmissionRate = 500,
        .EmitterLifeTime = 5.f,
        .StartColor = glm::vec3(0.6f, 0.2f, 0.9f),
        .EndColor = glm::vec3(0.6f, 0.2f, 0.9f),
        .ConeDimensions = glm::vec2(30.f, 30.f),
        .LifeTime = glm::vec2(3.f, 5.f),
        .Scale = glm::vec2(0.25f, 0.25f),
        .Speed = glm::vec2(0.f, 0.2f),
        .Gravity = 0.2f,
        .ForceConfigData = {
            .Gravity = 0.f,
            .ForceDataArray = { ForceData{
                .Direction = glm::vec3(0.f, 0.f, 0.f),
                .Strength = 10.f,
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
        .Gravity = 5.f,
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
        .EmissionRate = 350,
        .EmitterLifeTime = 7.5f,
        .StartColor = glm::vec3(0.95f, 0.95f, 1.f),
        .EndColor = glm::vec3(0.95f, 0.95f, 1.f),
        .BoxDimensions = glm::vec3(50.f, 0.f, 10.f),
        .LifeTime = glm::vec2(4.f, 8.f),
        .Scale = glm::vec2(0.1f, 0.1f),
        .Speed = glm::vec2(0.3f, 0.8f),
        .Gravity = 0.3f,
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
}

#endif //MINIPARTICLESIMULATOR_PARTICLEPRESETS_HPP
