#pragma once


// Class dedicated to drawing UI elements, such as settings panels, status bar
// The UIs have enough logics to it to justify a dedicated class imo
//
class UIManager
{
public:
    //Constructors and destructors
    UIManager() = default;
    UIManager(const UIManager&) = delete;
    UIManager(UIManager&&) = delete;
    UIManager& operator=(const UIManager&) = delete;
    UIManager& operator=(UIManager&&) = delete;
    ~UIManager() = default;

    //Actual work functions
    void DrawStatusBar(float Fps, float FrameTimeMs, uint32_t ParticleCount) const;
    void DrawSettingsPanel();
    void DrawPanelContents();

    //Getters and setters
    [[nodiscard]] bool IsPanelOpen() const { return m_IsPanelOpen; }
    void SetPanelOpen(const bool InPanelOpen) { m_IsPanelOpen = InPanelOpen; }

private:
    void DrawForceSection();
private:
    bool m_IsPanelOpen = true;
};
