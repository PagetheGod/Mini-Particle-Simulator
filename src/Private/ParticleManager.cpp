//
// Created by YWvin on 2026/3/23.
//

#include "ParticleManager.hpp"
#include <immintrin.h>

void ParticleManager::InitializeParticles()
{
    m_ParticleStates.Px = std::vector<float>(NUM_MAX_PARTICLES, 0.f);
    m_ParticleStates.Py = std::vector<float>(NUM_MAX_PARTICLES, 0.f);
    m_ParticleStates.Pz = std::vector<float>(NUM_MAX_PARTICLES, 0.f);
    m_ParticleStates.Vx = std::vector<float>(NUM_MAX_PARTICLES, 0.f);
    m_ParticleStates.Vy = std::vector<float>(NUM_MAX_PARTICLES, 0.f);
    m_ParticleStates.Vz = std::vector<float>(NUM_MAX_PARTICLES, 0.f);
    m_ParticleStates.R = std::vector<float>(NUM_MAX_PARTICLES, 0.f);
    m_ParticleStates.G = std::vector<float>(NUM_MAX_PARTICLES, 0.f);
    m_ParticleStates.B = std::vector<float>(NUM_MAX_PARTICLES, 0.f);
    m_ParticleStates.SizeOrNextFree.resize(NUM_MAX_PARTICLES);
    for (uint32_t i = 0; i < NUM_MAX_PARTICLES; i++)
    {
        m_ParticleStates.SizeOrNextFree[i].NextFreeSlot = i + 1;
    }
    m_ParticleStates.LifeTime = std::vector<float>(NUM_MAX_PARTICLES, 0.f);
    m_ParticleStates.FirstAvailable = 0;
}

void ParticleManager::UpdateParticles(float DeltaTime)
{

}

glm::vec3 ParticleManager::SolveForces(const ForceConfig &ForceConfigData)
{
    // Resolve gravity first using SIMD

    // Calculate the number of particles every thread will handle, rounding up
    const uint32_t NumParticlesPerThread = std::ceil(static_cast<float>(m_ParticleCount) /
        static_cast<float>(NUM_THREADS_USED));

}
