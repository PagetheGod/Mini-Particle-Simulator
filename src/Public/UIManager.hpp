#pragma once

#include "ParticleManager.hpp"
#include <functional>

// Class dedicated to drawing UI elements, such as settings panels, status bar
// The UIs have enough logics to it to justify a dedicated class imo

// Small helper struct to pass delta time and fps related information to the frame() function
struct DeltaTimeData
{
    float DeltaTime = 0.f;
    int FPS = 0.f;
    float FrameTime = 0.f;
};

class UIManager
{
public:
    //Constructors and destructors
    explicit UIManager(std::function<void(bool SetLoopEnable)> InToggleLoopCallback);
    UIManager(const UIManager&) = delete;
    UIManager(UIManager&&) = delete;
    UIManager& operator=(const UIManager&) = delete;
    UIManager& operator=(UIManager&&) = delete;
    ~UIManager() = default;

    //Actual work functions
    bool UIFrame(const DeltaTimeData& InDeltaTimeData, ParticleSimulatorConfig& ParticleConfig,
        uint32_t ParticleCount);

    //Getters and setters
    [[nodiscard]] bool IsPanelOpen() const { return m_IsPanelOpen; }
    void TogglePanelOpen() { m_IsPanelOpen = !m_IsPanelOpen; }

private:
    // Granular draw functions, each one of these will call other draw functions
    // Or draw some parts of the UI themselves
    void DrawStatusBar(const DeltaTimeData& InDeltaTimeData, uint32_t ParticleCount) const;
    bool GetParticleSimulatorConfig(ParticleSimulatorConfig& Config);
    bool DrawSettingsPanel(ParticleSimulatorConfig& Config);
    bool DrawParticleInit(ParticleSimulatorConfig& Config);
    bool DrawParticleVisuals(ParticleSimulatorConfig& Config);
    bool DrawForceSettings(ForceConfig& ForceConfig);
    void DrawPanelExpandButton();
    void DrawPanelCollapseButton();
private:
    bool m_IsPanelOpen = true;
    // It's a bandaid fix for the issue that we need the most up to date loop state
    bool m_ShouldLoop = false;
    std::function<void(bool)> m_ToggleLoopCallback;
};
