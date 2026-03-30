#pragma once

#include <SDL3/SDL.h>



// This class implements the rendering logics by using SDL3's built in software renderer
class SoftwareRenderer
{
public:
    // Constructors and destructor
    SoftwareRenderer();
    SoftwareRenderer(const SoftwareRenderer&) = delete;
    SoftwareRenderer& operator=(const SoftwareRenderer&) = delete;
    SoftwareRenderer(SoftwareRenderer&&) = delete;
    SoftwareRenderer& operator=(SoftwareRenderer&&) = delete;
    ~SoftwareRenderer();

    // Actual work functions
    // Initialize the software renderer and ImGui backend.
    // Returns true on success.
    bool Initialize(SDL_Window *window);

    // Called at the start of each frame, before any ImGui calls.
    void BeginFrame();

    // Called after ImGui::Render(), draws ImGui + clears the background.
    void EndFrame();

private:
    bool CreateGaussianGlowTexture(int Diameter);
private:
    SDL_Renderer* m_Renderer;
    SDL_Window* m_Window;
    SDL_Texture* m_ParticleTexture;

    // Constants
    static constexpr int PARTICLE_TEXTURE_WIDTH = 64;
    static constexpr int PARTICLE_TEXTURE_HEIGHT = 64;
};
