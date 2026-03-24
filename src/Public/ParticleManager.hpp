#pragma once
#include <cstdint>


/*
 * This class will handle creating, updating, and destroying particles
 * It's where we will run the core math and physics of our particle updates
 */

struct ParticleSimulatorConfig
{

};

class ParticleManager {
public:
    //Constructors and destructors
    ParticleManager();

    ParticleManager(const ParticleManager&) = delete;// Same with application class, makes no sense to copy or move
    ParticleManager& operator=(const ParticleManager&) = delete;
    ParticleManager(ParticleManager&&) = delete;
    ParticleManager& operator=(ParticleManager&&) = delete;

    ~ParticleManager();

    //Actual work functions
    void InitializeParticles();

    //Getters and setters
    [[nodiscard]] uint64_t GetParticleCount() const {
        return 0;
    };
public:

private:

private:

};


