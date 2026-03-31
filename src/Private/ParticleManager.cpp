//
// Created by YWvin on 2026/3/23.
//

#include "ParticleManager.hpp"
#include <cstdlib>
#include <iostream>

void ParticleManager::InitializeParticles()
{
    // Allocate the aligned arrays for all particle states
    // We are using 64-byte alignment to account for all three SIMD alignment requirements
    // SSE2 - 16, AVX2 - 32, AVX512 - 64 bytes. All match their vector register width
    m_ParticleStates.Px = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.Py = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.Pz = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.Vx = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.Vy = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.Vz = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.R = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.G = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.B = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.Size = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.LifeTime = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
    m_ParticleStates.MaxLifeTime = AllocateAlignedArray(NUM_MAX_PARTICLES, 64);
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

AlignedArray ParticleManager::AllocateAlignedArray(size_t NumElements, size_t Alignment)
{
    void* RawBlock = nullptr;
#ifdef _WIN32
    // Similar to the free situation
    // Windows uses this function to allocate aligned memory
    RawBlock = _aligned_malloc(NumElements * sizeof(float), Alignment);
#else
    // Window DOES NOT support this, POSIX does
    RawBlock = std::aligned_alloc(Alignment, NumElements * sizeof(float));
#endif
    if (!RawBlock)
    {
        std::cerr << "Failed to allocate aligned array!" << std::endl;
        return nullptr;
    }
    return AlignedArray(static_cast<float*>(RawBlock));
}
