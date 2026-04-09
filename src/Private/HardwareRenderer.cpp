
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

HardwareRenderer::HardwareRenderer(ParticleManager* InParticleManager) : m_ParticleManager(InParticleManager)
{

}

HardwareRenderer::~HardwareRenderer() {
}

bool HardwareRenderer::Initialize()
{
    return true;
}


