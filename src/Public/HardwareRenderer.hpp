#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <vector>
#include <cstdint>
#include <string>
#include "Commons.hpp"
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <memory>
#include <glm/glm.hpp>

#include "VulkanManager.hpp"


class ParticleManager;

class HardwareRenderer
{
public:
    HardwareRenderer(ParticleManager* InParticleManager);
    HardwareRenderer(const HardwareRenderer&) = delete;
    HardwareRenderer& operator=(const HardwareRenderer&) = delete;
    HardwareRenderer(HardwareRenderer&& ) = delete;
    HardwareRenderer& operator=(HardwareRenderer&&) = delete;
    ~HardwareRenderer();

    // Actual work functions
    bool Initialize();


public:

private:
	std::unique_ptr<VulkanManager> m_VulkanManager;
	ParticleManager* m_ParticleManager;


	// Constants
	/*
	 * An array that contains the six vertices of the two triangles that make up a single particle
	 * Similar to what I did back in DX11, when we use instance rendering with something like particles
	 * You create a small "vertex buffer" like this and the GPU shaders will use the instance buffer
	 * to handle the actual transforms
	 */
	static constexpr float QuadVertices[] = {
		// First triangle
		-1.f, -1.f, // Bottom left
		1.f, 1.f, // Top right
		-1.f, 1.f, // Top left
		// Second triangle
		-1.f, -1.f, // Bottom left
		1.f, -1.f, // Bottom right
		1.f, 1.f // Top right
	};
};