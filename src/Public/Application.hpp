#pragma once
#include "SDL3/SDL_video.h"
#include <memory>

#include "InputManager.hpp"
#include "SoftwareRenderer.hpp"
#include "UIManager.hpp"
#include "SIMD.hpp"

/*
* This class will handle the GUI application, including:
* 1. Initialization and init error handling. It will set up both ImGUI and SDL3, and Vulkan backend(optional)
* 2. Run the main loop: poll events, draw the immediate mode UI for ImGUI, and issue draw calls
* 3. Initialize thread pool, fill the particle object pool
*/

class Camera;
class ParticleManager;
class HardwareRenderer;

enum class RendererType : uint8_t
{
    Software,
    Hardware
};


class Application {
public:
    // Constructors and destructors
    Application();
    Application(const Application&) = delete; //Makes no sense to copy/move the application instance in our case
    Application(Application&&) = delete;
    Application& operator=(const Application&) = delete;
    ~Application();

    // Init and frame function
    bool Initialize();
    void Run();
    bool Frame(const DeltaTimeData& InDeltaTimeData, const InputResult& Input);

private:
    // Shows a modal dialog and returns the user's renderer choice.
    // This function creates a temporary SDL_Renderer, runs a mini event
    // loop with ImGui, and cleans up before returning.
    bool ShowStartupDialog();
    // Adjust all the fonts and paddings and such, moved from SoftwareRenderer to here
    // Because hardware renderer needs the same adjustments as well
    void SetUIandFontScale();
    // Get delta time
    float GetDeltaTime(uint64_t& LastNs);
    // Calculate delta time, FPS, and frame time each frame
    void FrameTiming(DeltaTimeData& DTData);
private:
    SDL_Window* m_Window;
    RendererType m_RendererType;
    // Declared before the renderers and the particle manager on purpose: they
    // borrow it via raw pointers, so the owner must be destroyed AFTER them.
    // Members destroy in reverse declaration order, so first-declared = last-destroyed.
    std::unique_ptr<VThreadPool> m_VThreadPool;
    std::unique_ptr<SoftwareRenderer> m_SoftwareRenderer;
    std::unique_ptr<HardwareRenderer> m_HardwareRenderer;
    std::unique_ptr<UIManager> m_UIManager;
    std::unique_ptr<ParticleManager> m_ParticleManager;
    InputManager m_InputManager; // Actual instance not a ptr because it's small
    Camera2D* m_Camera2D; // Raw ptr because we do not own it
    Camera* m_Camera;

    // States
    bool m_Paused = true;
    bool m_ShouldLoop = true;
    float m_PlaybackLeft = 0.f;
    bool m_IsConfigDirty = false;
    // This config persists through the entire lifetime of the app
    // So we can easily check
    ParticleSimulatorConfig m_ParticleConfig;
    // Constants
    // How long would we play the entire scene until we stop or loop
    static constexpr float PLAYBACK_DURATION = 15.f;
};


