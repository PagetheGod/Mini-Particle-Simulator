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

#include "ParticleManager.hpp"
#include "VulkanManager.hpp"

class Camera;

class HardwareRenderer
{
public:
	explicit HardwareRenderer(ParticleManager* InParticleManager,  VThreadPool* InVThreadPool);
	HardwareRenderer(const HardwareRenderer&) = delete;
	HardwareRenderer& operator=(const HardwareRenderer&) = delete;
	HardwareRenderer(HardwareRenderer&& ) = delete;
	HardwareRenderer& operator=(HardwareRenderer&&) = delete;
	~HardwareRenderer();

	// Actual work functions
	bool Initialize(SDL_Window* InWindow);
	void BeginFrame();
	void EndFrame(const bool IsPanelOpen);

	// Setters and getters
	Camera* GetCamera()
	{
		return m_Camera.get();
	}
public:

private:
	/* Upload per frame particle state data, this would be called after vkWaitFences for this frame's fence
	 * And after the CPU's physics calculations. The fence finishing guarantees that the GPU is done
	 * reading the particle data buffers from the previous use of this slot
	 * CurrentFrame is the index mod max frame in flight, same index used for command buffer reuse
	 */
	void UploadParticleData(uint32_t CurrentFrame, const ParticleStates& InParticleStates,
		uint32_t ParticleCount);

private:
	std::unique_ptr<VulkanManager> m_VulkanManager;
	std::unique_ptr<Camera> m_Camera;
	ParticleManager* m_ParticleManager;
	VulkanContext* m_VulkanContextPtr;
	SDL_Window* m_Window;
	VThreadPool* m_VThreadPool;
	// The vertex buffer for the particles is a persistent buffer in our case, since it's just a quad
	AllocatedVkBuffer m_VertexBuffer;
	// Constants
	/*
	 * An array that contains the six vertices of the two triangles that make up a single particle
	 * Similar to what I did back in DX11, when we use instance rendering with something like particles
	 * We create a small "vertex buffer" like this and the GPU shaders will use the instance buffer
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
