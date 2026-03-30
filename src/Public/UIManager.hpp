#pragma once

#include "ParticleManager.hpp"
#include <functional>

// Class dedicated to drawing UI elements, such as settings panels, status bar
// The UIs have enough logics to it to justify a dedicated class imo



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
    void UIFrame(float Fps, float FrameTimeMs, uint32_t ParticleCount, ParticleSimulatorConfig& ParticleConfig);
    void DrawStatusBar(float Fps, float FrameTimeMs, uint32_t ParticleCount) const;
    void GetParticleSimulatorConfig(ParticleSimulatorConfig& Config);
    void DrawSettingsPanel(ParticleSimulatorConfig& Config);
    void DrawPanelContents(ParticleSimulatorConfig& Config);
    void DrawParticleInit(ParticleSimulatorConfig& Config);
    void DrawParticleVisuals(ParticleSimulatorConfig& Config);
    void DrawForceSettings(ForceConfig& ForceConfig);
    void DrawPanelToggleButton();
    //Getters and setters
    [[nodiscard]] bool IsPanelOpen() const { return m_IsPanelOpen; }
    void TogglePanelOpen() { m_IsPanelOpen = !m_IsPanelOpen; }

private:

private:
    bool m_IsPanelOpen = true;
    std::function<void(bool)> m_ToggleLoopCallback;
};
