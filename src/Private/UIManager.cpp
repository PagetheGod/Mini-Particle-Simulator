#include <iostream>
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_vulkan.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

//External libs and STL
#include <format>
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_timer.h"

//Own headers
#include "Commons.hpp"
#include "UIManager.hpp"

void UIManager::DrawStatusBar(float Fps, float FrameTimeMs, uint32_t ParticleCount) const
{
    using namespace Commons::Layout;
    // Status bar is hidden when the panels are collapsed
    if (!m_IsPanelOpen)
    {
        return;
    }

    // Position: full width, at the bottom of the window
    ImGui::SetNextWindowPos(ImVec2(0, WINDOW_HEIGHT - STATUS_BAR_HEIGHT));
    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, STATUS_BAR_HEIGHT));

    // Window flags: no decorations, no interaction, no scrolling.
    // NoBackground is NOT set — we want a solid background to clearly
    // separate the status bar from the particle viewport above.
    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
    ImGuiWindowFlags_NoBringToFrontOnFocus;
    //
    ImGui::Begin("##Status Bar", nullptr, WindowFlags);

    // Stats on separate lines for readability.
    ImGui::Text("FPS: %.1f", Fps);
    ImGui::Text("Frame Time: %.1f ms", FrameTimeMs);

    // Format particle count with thousands separator
    // In an actual system this shall be handled by a while loop with division and modulo
    // But since we probably won't even get a million particle count this stupid loop would work
    if (ParticleCount >= 1'000'000)
    {
        ImGui::Text("Particles: %u,%03u,%03u",
                    ParticleCount / 1'000'000, (ParticleCount / 1000) % 1000,
                    ParticleCount % 1000);
    }
    else if (ParticleCount >= 1000)
    {
        ImGui::Text("Particles: %u,%03u", ParticleCount / 1000, ParticleCount % 1000);
    }
    else
    {
        ImGui::Text("Particles: %u", ParticleCount);
    }

    ImGui::End();
}

void UIManager::DrawSettingsPanel() {
    if (!m_IsPanelOpen)
    {
        return;
    }
    // Preset selectors
    const char* Presets[] = {"Burst", "Firework", "Fountain", "Vortex", "Waterfall", "Snow"};
    static int SelectedPreset = 0;
    if (ImGui::Combo("Particle Preset", &SelectedPreset, Presets, IM_ARRAYSIZE(Presets)))
    {
        //TODO:
        // Load the selected preset's configuration
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Emitter settings
    if (ImGui::CollapsingHeader("Emitter Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Spwan shapes
        const char* Shapes[] = {"Sphere", "Cone", "Box", "Ring/Disc", "Cylinder"};
        int CurrentShape = 0;
        if (ImGui::Combo("Shape", &CurrentShape, Shapes, IM_ARRAYSIZE(Shapes)))
        {
            //TODO: Set the shapes in the particle config struct
        }
    }

    // Shape-specific parameters
    // Only show the parameters relevant to the current shape.
    // This is a natural use of ImGui's immediate mode — just don't
    // call the widgets for shapes that aren't active.
    switch (1)//TODO: Replace placeholder with the shape in the config
    {
        default://TODO: Change this default
            ImGui::Text("No shape-specific parameters");
            break;
    }

    ImGui::Spacing();

    // Emitter mode
    const char* EmitterMode[] = {"Burst", "Continuous"};
    int CurrentMode = 0;
    //TODO: Set the emitter mode in the config
}

void UIManager::DrawPanelContents() {
}
