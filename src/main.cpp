/*
 * Mini Particle Simulator project, built by Iso and Vincent
 * This is a tiny particle simulator/previewer that aims to create a simplified experience from game engine particle vfx editor
 */
#include <cstdio>
#include "SDL3/SDL.h"
#include <cstdlib>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "imgui_impl_vulkan.h"
#include <SDL3/SDL_vulkan.h>
#include <iostream>
#include <format>
#include "Application.hpp"

int main()
{
    Application App;
    if (!App.Initialize())
    {
        std::cerr << "Application failed to initialize!" << std::endl;
    }

}