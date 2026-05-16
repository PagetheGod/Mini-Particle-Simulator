
#include <algorithm>
#include <SDL3/SDL_vulkan.h>
#include <cstdio>
#include <cstring>  // for strcmp
#include <limits>
#include <iostream>
#include <format>
#include <set>
#include <fstream>

// Own headers
#include "HardwareRenderer.hpp"

#include "imgui_impl_vulkan.h"

HardwareRenderer::HardwareRenderer(ParticleManager* InParticleManager) : m_VulkanManager(nullptr), m_ParticleManager(InParticleManager),
m_VulkanContextPtr(nullptr), m_Window(nullptr)
{

}

HardwareRenderer::~HardwareRenderer()
{
    m_VulkanContextPtr = nullptr;
    m_ParticleManager = nullptr;
}

bool HardwareRenderer::Initialize(SDL_Window* InWindow)
{
    bool Result = false;

    m_VulkanManager = std::make_unique<VulkanManager>();
    m_Window = InWindow;
    Result = m_VulkanManager->Initialize(InWindow);
    if (!Result)
    {
        std::cerr << "Failed to initialize the Vulkan manager." << std::endl;
        return Result;
    }
    m_VulkanContextPtr = m_VulkanManager->GetVulkanContext();
    ImGui_ImplVulkan_InitInfo ImGuiVkInitInfo{};
    ImGuiVkInitInfo.Device = m_VulkanContextPtr->VulkanDevice;
    ImGuiVkInitInfo.Instance = m_VulkanContextPtr->VulkanInstance;
    ImGuiVkInitInfo.PhysicalDevice = m_VulkanContextPtr->VulkanPhysicalDevice;
    ImGuiVkInitInfo.Allocator = nullptr;
    ImGuiVkInitInfo.DescriptorPool = m_VulkanContextPtr->DescriptorPool;
    ImGuiVkInitInfo.DescriptorPoolSize = 1;
    ImGuiVkInitInfo.ApiVersion = VK_API_VERSION_1_4;
    ImGuiVkInitInfo.Queue = m_VulkanContextPtr->PresentQueue;
    ImGuiVkInitInfo.QueueFamily = m_VulkanContextPtr->PresentFamily;
    ImGuiVkInitInfo.UseDynamicRendering = false;

    return Result;
}


void HardwareRenderer::UploadParticleData(uint32_t CurrentFrame, const ParticleStates& InParticleStates,
    uint32_t ParticleCount)
{
    ParticleInstanceBuffers& BufferSet = m_VulkanContextPtr->ParticleInstanceBufferArray[CurrentFrame];

    const size_t BytesToCopy = ParticleCount * 4;

    /*
     * Source/destination pairs for each attribute. The order must match the Mapped[] indices set up in
     * CreateParticleInstanceBuffers - 0...8 = Px, Py, Pz, R, G, B, Size, LifeTime, MaxLifeTime
     */
    const float* Sources[9] = {
        InParticleStates.Px.get(),
        InParticleStates.Py.get(),
        InParticleStates.Pz.get(),
        InParticleStates.R.get(),
        InParticleStates.G.get(),
        InParticleStates.B.get(),
        InParticleStates.Size.get(),
        InParticleStates.LifeTime.get(),
        InParticleStates.MaxLifeTime.get()
    };

    // Copy all the data to VRAM through persistently mapped GPU ptr

    /*
     * We do not have to call vkFlushMappedMemoryRanges because we allocated the buffers as
     * HOST_COHERENT, which means that any change to the buffers by the CPU will make these writes visible
     * to the GPU "automatically". By the nex vkQueueSubmit
     * We also do not need memory barrier, Vulkan's host-to-device write visibility is handled implicitly by
     * queue submissions. Storage buffer reads in the vertex shader of this frame's command buffer (not recorded
     * at this step of the frame) will see the data we just wrote
     */
    for (uint32_t i = 0; i < 9; i++)
    {
        memcpy(BufferSet.MappedPtr[i], Sources[i], BytesToCopy);
    }

}


