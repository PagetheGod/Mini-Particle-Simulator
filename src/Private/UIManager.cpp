//External libs and STL
#include <format>
#include <utility>
#include "SDL3/SDL_render.h"
#include "imgui_internal.h"
#include <iostream>
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_vulkan.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
//Own headers
#include "Commons.hpp"
#include "UIManager.hpp"

#include "ParticlePresets.hpp"


UIManager::UIManager(std::function<void(bool SetLoopEnable)> InToggleLoopCallback) : m_ToggleLoopCallback(std::move(InToggleLoopCallback)){
}

void UIManager::UIFrame(const DeltaTimeData& InDeltaTimeData, ParticleSimulatorConfig& ParticleConfig,
    uint32_t ParticleCount)
{
    // Settings panel
    if (!m_IsPanelOpen)
    {
        DrawPanelExpandButton();
        return;
    }
    DrawStatusBar(InDeltaTimeData, ParticleCount);
    GetParticleSimulatorConfig(ParticleConfig);
    // Draw the collapse button at last so it doesn't get drawn over by settings panel
    DrawPanelCollapseButton();
}

void UIManager::DrawStatusBar(const DeltaTimeData& InDeltaTimeData, uint32_t ParticleCount) const
{
    using namespace Commons::Layout;
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
    // Basically when we say begin(), we are telling ImGui: "I want to start drawing UI to a window"
    // We can say begin + end() multiple times in a frame to the same window
    ImGui::Begin("Status Bar", nullptr, WindowFlags);

    // Stats on separate lines for readability.
    ImGui::Text("FPS: %d", InDeltaTimeData.FPS);
    ImGui::Text("Frame Time: %.1f ms", InDeltaTimeData.FrameTime);
    // Format particle count with thousands separator
    // In an actual system this shall be handled by a while loop with division and modulo
    // But since we probably won't get over 100k particle count this logic should suffice
    if (ParticleCount >= 1000)
    {
        ImGui::Text("Particles: %u,%03u", ParticleCount / 1000, ParticleCount % 1000);
    }
    else
    {
        ImGui::Text("Particles: %u", ParticleCount);
    }
    ImGui::Text("Pressed Space to toggle pause");
    ImGui::End();
}

void UIManager::GetParticleSimulatorConfig(ParticleSimulatorConfig &Config)
{
    using namespace Commons;
    // Start drawing the entire settings panel
    ImGui::SetNextWindowPos(ImVec2(Layout::VIEWPORT_WIDTH_OPEN, 0));
    ImGui::SetNextWindowSize(ImVec2(Layout::PANEL_WIDTH, Layout::WINDOW_HEIGHT));
    // Set flags
    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
    ImGui::Begin("Particle Simulator Settings", nullptr, WindowFlags);
    DrawSettingsPanel(Config);
    DrawParticleInit(Config);
    DrawParticleVisuals(Config);
    DrawForceSettings(Config.ForceConfigData);
    // End drawing settings panel
    ImGui::End();
}

void UIManager::DrawSettingsPanel(ParticleSimulatorConfig& Config)
{
    // Preset selectors
    // Currently, we are using a system where the enum constant's values must match the indices in this array
    // Fragile and EVIL, should probably look for a more stable solution
    const char* Presets[] = {"None", "OmniDirectionalBurst", "Firework", "Fountain", "Vortex", "Waterfall", "Snow"};
    static int SelectedPreset = 0;
    if (ImGui::Combo("Particle Preset", &SelectedPreset, Presets, IM_ARRAYSIZE(Presets)))
    {
        const PresetType SelectedPresetType = static_cast<PresetType>(SelectedPreset);
        switch (SelectedPresetType)
        {
            case PresetType::None:
                break;
            case PresetType::OmniDirectionalBurst:
            {
                Config = ParticlePresets::OmniDirectionalBurst;
                break;
            }
            case PresetType::Firework:
            {
                Config = ParticlePresets::Firework;
                break;
            }
            case PresetType::Fountain:
            {
                Config = ParticlePresets::Fountain;
                break;
            }
            case PresetType::Vortex:
            {
                Config = ParticlePresets::Vortex;
                break;
            }
            case PresetType::Waterfall:
            {
                Config = ParticlePresets::Waterfall;
                break;
            }
            case PresetType::Snow:
            {
                Config = ParticlePresets::Snow;
                break;
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Toggle loop
    bool ShouldLoop = m_ShouldLoop;
    if (ImGui::Checkbox("Loop", &ShouldLoop))
    {
        m_ToggleLoopCallback(ShouldLoop);
        m_ShouldLoop = ShouldLoop;
    }

    // Emitter mode
    const char* EmitterMode[] = {"Burst", "Continuous"};
    int CurrentMode = static_cast<int>(Config.Mode);
    if (ImGui::Combo("Emitter Mode", &CurrentMode, EmitterMode, IM_ARRAYSIZE(EmitterMode)))
    {
        Config.Mode = static_cast<enum EmitterMode>(CurrentMode);
    }
    if (Config.Mode == EmitterMode::Burst)
    {
        // How often does the emitter spawn particles in burst mode
        ImGui::SliderFloat("Burst Interval", &Config.BurstInterval, 0.5f, 3.f);
        // We reuse the Emitter Rate variable for both burst and continuous emitter
        ImGui::SliderInt("Particle Count Per Burst", &Config.EmissionRate,
            5, 5000);
    }
    else
    {
        // These numbers are kinda arbitrary, might be better to define them in commons.hpp
        // As constants, however, since we are just using them here, it's ok for now
        ImGui::SliderInt("Particle Per Second", &Config.EmissionRate, 50, 7500);
    }
    // Spawn shape settings
    if (ImGui::CollapsingHeader("Emitter Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Spawn shapes
        const char* Shapes[] = {"Sphere", "Cone", "Box/Plane", "Ring/Disc", "Cylinder"};
        int CurrentShape = static_cast<int>(Config.Shape);
        if (ImGui::Combo("Shape", &CurrentShape, Shapes, IM_ARRAYSIZE(Shapes)))
        {
            Config.Shape = static_cast<enum SpawnShape>(CurrentShape);
        }

        // Shape-specific parameters
        // Only show the parameters relevant to the current shape.
        // This is a natural use of ImGui's immediate mode — just don't
        // call the widgets for shapes that aren't active.
        switch (Config.Shape)//
        {
            case SpawnShape::Sphere:
            {
                ImGui::SliderFloat("Radius", &Config.SphereRadius, 0.1f, 10.f, "%.1f");
                break;
            }
            case SpawnShape::Cone:
            {
                ImGui::SliderFloat("Axis Length", &Config.ConeDimensions.x, 0.1f, 10.f, "%.1f");
                ImGui::SliderFloat("Half Angle", &Config.ConeDimensions.y, 0.1f, 90.f, "%.1f");
                break;
            }
            case SpawnShape::Box:
            {
                ImGui::SliderFloat("Width", &Config.BoxDimensions.x, 0.1f, 100.f, "%.1f");
                ImGui::SliderFloat("Height", &Config.BoxDimensions.y, 0.1f, 100.f, "%.1f");
                ImGui::SliderFloat("Depth", &Config.BoxDimensions.z, 0.1f, 100.f, "%.1f");
                break;
            }
            case SpawnShape::Ring:
            {
                ImGui::SliderFloat("Inner Radius", &Config.RingDimensions.x, 0.f, 50.f, "%.1f");
                ImGui::SliderFloat("Outer Radius", &Config.RingDimensions.y, 0.f, 50.f, "%.1f");
                // Clamp the min to be smaller than max
                if (Config.RingDimensions.x > Config.RingDimensions.y)
                {
                    Config.RingDimensions.x = Config.RingDimensions.y - 0.1f;
                }
                break;
            }
            case SpawnShape::Cylinder:
            {
                ImGui::SliderFloat("Height", &Config.CylinderDimensions.y, 0.f, 50.f, "%.1f");
                ImGui::SliderFloat("Radius", &Config.CylinderDimensions.x, 0.f, 50.f, "%.1f");
                break;
            }
            default:
                break;
        }
    }
    ImGui::Spacing();
}
void UIManager::DrawParticleInit(ParticleSimulatorConfig &Config) {
    if (ImGui::CollapsingHeader("Particle Initialization", ImGuiTreeNodeFlags_DefaultOpen)) {
        // First check whether the user wants random speed at spawn
        bool IsRandomSpeed = Config.IsRandomSpeed;
        ImGui::Checkbox("Random Speed at Spawn", &IsRandomSpeed);
        if (IsRandomSpeed)
        {
            // Min and max base speed at spawn
            ImGui::SliderFloat("Min Base Speed at Spawn", &Config.BaseSpeedMin,0.1f, 100.f);
            ImGui::SliderFloat("Max Base Speed at Spawn", &Config.BaseSpeedMax,0.1f, 100.f);
            // Clamp
            if (Config.BaseSpeedMin >= Config.BaseSpeedMax)
            {
                Config.BaseSpeedMin = Config.BaseSpeedMax - 0.1f;
            }
        }
        else
        {
            // No random interval, just a fix speed
            ImGui::SliderFloat("Base Speed at Spawn", &Config.BaseSpeedMin,0.1f, 100.f);
        }
        Config.IsRandomSpeed = IsRandomSpeed;

        ImGui::Spacing();

        bool IsRandomLifeTime = Config.IsRandomLifeTime;
        ImGui::Checkbox("Random Lifetime", &IsRandomLifeTime);
        if (IsRandomLifeTime)
        {
            // Min and max lifetime
            ImGui::SliderFloat("Lifetime Min", &Config.LifeTime.x, 0.1f, 10.f, "%.1f");
            ImGui::SliderFloat("Lifetime Max", &Config.LifeTime.y, 0.1f, 10.f, "%.1f");
            // Remember to clamp!
            if (Config.LifeTime.x >= Config.LifeTime.y)
            {
                Config.LifeTime.x = Config.LifeTime.y - 0.1f;
            }
        }
        else
        {
            // No random interval for lifetime, just fix lifetime
            ImGui::SliderFloat("Lifetime", &Config.LifeTime.x, 0.1f, 10.f, "%.1f");
        }
        Config.IsRandomLifeTime = IsRandomLifeTime;

        ImGui::Spacing();

        bool IsRandomScale = Config.IsRandomScale;
        ImGui::Checkbox("Random Size at Spawn", &IsRandomScale);
        if (IsRandomScale)
        {
            // Min and max scales of the particles
            ImGui::SliderFloat("Size Min", &Config.Scale.x, 0.1f, 10.f, "%.1f");
            ImGui::SliderFloat("Size Max", &Config.Scale.y, 0.1f, 10.f, "%.1f");
            if (Config.Scale.x >= Config.Scale.y)
            {
                Config.Scale.x = Config.Scale.y - 0.1f;
            }
        }
        else
        {
            // No random interval for spawn size, just fixed
            ImGui::SliderFloat("Size", &Config.Scale.x, 0.1f, 10.f, "%.1f");
        }
        Config.IsRandomScale = IsRandomScale;

        ImGui::Spacing();
    }

    ImGui::Spacing();
}

void UIManager::DrawParticleVisuals(ParticleSimulatorConfig &Config)
{
    /* ── Color picker flags ──
     * These flags configure what the picker widget displays.
     * You can change these to customize the picker appearance.

     * Available picker styles:
     *   PickerHueWheel — circular hue ring with SV square inside
     *   PickerHueBar   — vertical hue bar with SV square beside it
    //
    // Available display modes (combine with |):
    //   DisplayRGB — show R, G, B number input boxes
    //   DisplayHSV — show H, S, V number input boxes
    //   DisplayHex — show hex input (#RRGGBB)
    //
    // Number format:
    //   Float — show values as 0.000–1.000 (useful for shaders)
    //   (default) — show values as 0–255 (more familiar to artists)
    //
    // Other useful flags:
    //   NoAlpha      — hide alpha even on ColorPicker4
    //   NoSidePreview — hide the old/new color comparison swatch
    //   NoSmallPreview — hide the small color preview before the label
    //   NoInputs     — hide all number inputs (wheel/square only)
    //   NoLabel      — hide the text label */
    ImGuiColorEditFlags ColorEditorFlags = ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_DisplayRGB |
        ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_Float;
    /*
     * Using ColorPicker3, this shows the entire picker widget, which takes up a lot of space
     * If there's too much visual clutter we will switch to ColorEdittor
     */
    float StartColor[3] = {Config.StartColor.r, Config.StartColor.g, Config.StartColor.b};
    ImGui::Text("Particle Color at Spawn: ");
    if (ImGui::ColorPicker3("StartColorPicker", StartColor, ColorEditorFlags))
    {
        Config.StartColor = {StartColor[0], StartColor[1], StartColor[2]};
    }

    ImGui::Spacing();

    bool IsScalingColor = Config.IsScalingColor;
    ImGui::Checkbox("Scale color", &IsScalingColor);
    Config.IsScalingColor = IsScalingColor;
    if (IsScalingColor)
    {
        float EndColor[3] = {Config.EndColor.r, Config.EndColor.g, Config.EndColor.b};
        ImGui::Text("Particle Color at Death: ");
        if (ImGui::ColorEdit3("EndColorPicker", EndColor, ColorEditorFlags))
        {
            Config.EndColor = {EndColor[0], EndColor[1], EndColor[2]};
        }
        /*
         * Show a preview of start-to-end color transition
         */
        const ImVec2 BarSize(ImGui::GetContentRegionAvail().x, 20.f);
        const ImVec2 CursorPos = ImGui::GetCursorScreenPos();
        const ImU32 StartColorUint = ImGui::ColorConvertFloat4ToU32(ImVec4(StartColor[0], StartColor[1], StartColor[2], 1.f));
        const ImU32 EndColorUint = ImGui::ColorConvertFloat4ToU32(ImVec4(EndColor[0], EndColor[1], EndColor[2], 1.f));

        ImGui::GetWindowDrawList()->AddRectFilledMultiColor(CursorPos, ImVec2(CursorPos.x + BarSize.x, CursorPos.y + BarSize.y)
        , StartColorUint, EndColorUint, StartColorUint, EndColorUint);
        // AddRectFilledColor bypasses ImGui's layout system so imGui does not know we had drawn to the preview region yet
        // Call Dummy() to reserve space for the color transition preview
        ImGui::Dummy(BarSize);
        ImGui::Text("Start to End Color Transition Preview");
        ImGui::Spacing();
    }
    ImGui::Checkbox("Randomize Colors for Individual Particle", &Config.IsRandomColor);
    if (ImGui::IsItemHovered())
    {
        // We can also use SetToolTip if the tool tip text is a single liner
        ImGui::BeginTooltip();
        ImGui::Text("If checked, each particle will have a random color at spawn.");
        ImGui::Separator();
        ImGui::Text("If enabled, the start and end color parameters will be ignored.");
        ImGui::EndTooltip();
    }

}

void UIManager::DrawForceSettings(ForceConfig& ForceConfig)
{
    using namespace Commons;
    if (ImGui::CollapsingHeader("Forces Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Gravity is always applied, no need to add
        ImGui::SliderFloat("Gravity Scale", &ForceConfig.Gravity, 0.f, 10.f, "%.1f");

        ImGui::Separator();
        // Let the user knows that we only allow 9 extra forces at max
        // And tell them how many they have right now
        ImGui::Text("Forces (maximum 9 allowed, currently %u / %u)",
            ForceConfig.ExtraForceCount, Constants::MAX_NUM_FORCES);
        const bool IsAtForceLimit = ForceConfig.ExtraForceCount >= Constants::MAX_NUM_FORCES;
        if (IsAtForceLimit)
        {
            // Disable the add force button when the user reached the limit
            ImGui::BeginDisabled();
        }
        if (ImGui::Button(" + Add Force"))
        {
            ImGui::OpenPopup("Add Force Popup");
        }
        if (ImGui::BeginPopup("Add Force Popup"))
        {
            // This ordering is crucial since our enum constants map to it directly
            // Fragile and EVIL, probably should find a better way to do this
            const char* ForceNames[] = {"Drag", "Point", "Vortex", "Wind"};
            for (int i = 0; i < IM_ARRAYSIZE(ForceNames); i++)
            {
                if (ImGui::Selectable(ForceNames[i]))
                {
                    const auto NewForceType = static_cast<enum ForceType>(i);
                    const uint8_t NewForceIndex = ForceConfig.ExtraForceCount;

                    ForceConfig.ForceTypes[NewForceIndex] = NewForceType;
                    ForceConfig.IsForceEnabled[NewForceIndex] = true;
                    ForceConfig.ForceDataArray[NewForceIndex] = {};

                    ForceData& NewForceDataRef = ForceConfig.ForceDataArray[NewForceIndex];
                    switch (ForceConfig.ForceTypes[NewForceIndex])
                    {
                        case ForceType::Drag:
                        {
                            NewForceDataRef.Strength = 0.3f;
                            break;
                        }
                        case ForceType::Point:
                        {
                            NewForceDataRef.Strength = 10.f;
                            NewForceDataRef.Direction = {0.f, 1.f, 0.f};
                            break;
                        }

                        case ForceType::Vortex:
                        {
                            NewForceDataRef.Strength = 1.f;
                            NewForceDataRef.VortexPull = 0.5f;
                            break;
                        }
                        case ForceType::Directional:
                        {
                            NewForceDataRef.Strength = 5.f;
                            NewForceDataRef.Direction = {-1.f, 0.f, 0.f};
                            break;
                        }
                        default:
                            break;
                    }
                    ForceConfig.ExtraForceCount++;

                }
            }
            ImGui::EndPopup();
        }
        if (IsAtForceLimit)
        {
            // Don't forget to end disable! Otherwise, it would disable other UIs
            ImGui::EndDisabled();
        }

        ImGui::Spacing();

        // Index to track whether, and which force we remove
        uint8_t RemovalIndex = UINT_FAST8_MAX;
        for (uint8_t i = 0; i < ForceConfig.ExtraForceCount; i++)
        {
            // Use unique PushID() to avoid conflicts between two forces of the same type
            ImGui::PushID(i);

            // Get references to all the info relevant to current force
            const int ForceNameIndex = static_cast<int>(ForceConfig.ForceTypes[i]);
            bool& IsForceEnabledRef = ForceConfig.IsForceEnabled[i];
            ForceData& ForceDataRef = ForceConfig.ForceDataArray[i];

            // Names for display
            const char* ForceNames[] = {"Drag", "Point", "Vortex", "Wind"};

            // Header with enable/disable checkbox
            ImGui::Checkbox("##Enabled", &IsForceEnabledRef);
            ImGui::SameLine();

            const bool IsHeadOpen = ImGui::TreeNode("##ForceHeader", "%s", ForceNames[ForceNameIndex]);

            // Right align the remove button
            // Can't really use the panel width constant, because that includes window padding
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 60.f);
            if (ImGui::SmallButton("Remove"))
            {
                RemovalIndex = i;
            }
            if (IsHeadOpen)
            {
                //Force type-specific params
                switch (ForceConfig.ForceTypes[i])
                {
                    case ForceType::Drag:
                    {
                        ImGui::SliderFloat("Drag Coefficient", &ForceDataRef.Strength,
                            0.f, 10.f, "%.1f");
                        break;
                    }
                    case ForceType::Point:
                    {
                        ImGui::SliderFloat("Point Force Strength", &ForceDataRef.Strength,
                            -20.f, 20.f, "%.1f");
                        ImGui::Text("Positive Strength = attraction, Negative Strength = repulsion");
                        ImGui::DragFloat("Position X", &ForceDataRef.Direction.x, 0.1f, -10.f, 10.f);
                        ImGui::DragFloat("Position Y", &ForceDataRef.Direction.y, 0.1f, -5.f, 15.f);
                        ImGui::DragFloat("Position Z", &ForceDataRef.Direction.z, 0.1f, 0.f, 20.f);
                        break;
                    }
                    case ForceType::Vortex:
                    {
                        ImGui::SliderFloat("Vortex Tangential Strength", &ForceDataRef.Strength,
                            0.f, 20.f, "%.1f");
                        ImGui::SliderFloat("Vortex Radial Strength", &ForceDataRef.VortexPull,
                            0.f, 1.f, "%.1f");
                        // For now, we limit the vortex to always be at y = 0
                        ImGui::DragFloat("Vertex Center X", &ForceDataRef.Direction.x, 0.1f, -5.f, 5.f);
                        ImGui::DragFloat("Vertex Center Z", &ForceDataRef.Direction.z, 0.1f, 0.f, 5.f);
                        break;
                    }
                    case ForceType::Directional:
                    {
                        ImGui::SliderFloat("Wind Strength", &ForceDataRef.Strength,
                            0.f, 10.f, "%.1f");
                        ImGui::SliderFloat("Wind Period", &ForceDataRef.WindPeriod,
                             0.5f, 5.f, "%.1f Seconds");
                        ImGui::SliderFloat("Wind Direction X", &ForceDataRef.Direction.x,
                             -1.f, 1.f, "%.1f");
                        ImGui::SliderFloat("Wind Direction Y", &ForceDataRef.Direction.y,
                            -1.f, 1.f, "%.1f");
                        ImGui::SliderFloat("Wind Direction Z", &ForceDataRef.Direction.z,
                            -1.f, 1.f, "%.1f");
                        break;
                    }
                }
                ImGui::TreePop();
            } // End individual force header if
            ImGui::PopID();
            ImGui::Separator();
        } // End force-specifics display and setting loop

        // Remove the removed force outside the loop so we don't invalidate the index half way through
        if (RemovalIndex != UINT_FAST8_MAX)
        {
            for (uint8_t i = RemovalIndex; i < ForceConfig.ExtraForceCount - 1; i++)
            {
                ForceConfig.ForceTypes[i] = ForceConfig.ForceTypes[i + 1];
                ForceConfig.IsForceEnabled[i] = ForceConfig.IsForceEnabled[i + 1];
                ForceConfig.ForceDataArray[i] = ForceConfig.ForceDataArray[i + 1];
            }
            ForceConfig.ExtraForceCount--;
        }
    }
}

void UIManager::DrawPanelExpandButton() {
    using namespace Commons;
    // Draw the "expand panel" button at the top-right corner
    constexpr float ButtonWidth = 45.f;
    constexpr float ButtonHeight = 35.f;
    constexpr float ButtonMargin = 8.5f;

    // Set where we will draw the button, it's Set Next Window pos
    // Because the NextWindow is our button
    ImGui::SetNextWindowPos(
        ImVec2(static_cast<float>(Layout::WINDOW_WIDTH) -
               ButtonWidth - ButtonMargin, ButtonMargin));
    ImGui::SetNextWindowSize(ImVec2(ButtonWidth, ButtonHeight));

    // Just like WIN32, when we create any window, we need to set flags
    ImGuiWindowFlags Flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    // Begin ImGui Frame again
    ImGui::Begin("Panel Toggle Button", nullptr, Flags);
    if (ImGui::Button("<< Expand Panel"))
    {
        m_IsPanelOpen = true;
    }
    if (ImGui::IsItemHovered())
    {
        // Tool tip is short, just use SetToolTip
        ImGui::SetTooltip("Open the settings panel and status bar (hotkey Tab)");
    }
    ImGui::End();

    /* ANOTHER Tab hint (bottom-right corner) in case the expand button got ignored
     * Small semi-transparent text reminding the user they can press Tab.
     * This does obstruct user's view and disappears when the UI is opened.
     *
     */
    constexpr float TabHintLength = 200.f;
    constexpr float TabHintHeight = 25.f;
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(Layout::WINDOW_WIDTH) - TabHintLength,
               static_cast<float>(Layout::WINDOW_HEIGHT) - TabHintHeight));
    ImGui::SetNextWindowSize(ImVec2(200.0f, 25.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
                          ImVec4(0.0f, 0.0f, 0.0f, 0.3f));

    ImGui::Begin("Tab Hint", nullptr, Flags);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.5f),
                       "Press Tab to restore UI");
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void UIManager::DrawPanelCollapseButton()
{
    // This function is very similar to the DrawPanelExpandButton()
    // Difference is this is drawed a collapse instead of expand button
    using namespace Commons;
    // Draw the "collapse panel" button at the top-right corner
    constexpr float ButtonWidth = 45.f;
    constexpr float ButtonHeight = 35.f;
    constexpr float ButtonMargin = 8.5f;

    // Set where we will draw the button, it's Set Next Window pos
    // Because the NextWindow is our button
    ImGui::SetNextWindowPos(
        ImVec2(static_cast<float>(Layout::WINDOW_WIDTH) -
               ButtonWidth - ButtonMargin, ButtonMargin));
    ImGui::SetNextWindowSize(ImVec2(ButtonWidth, ButtonHeight));

    // Just like WIN32, when we create any window, we need to set flags
    ImGuiWindowFlags Flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    // Begin ImGui Frame again
    ImGui::Begin("Panel Toggle Button", nullptr, Flags);
    if (ImGui::Button("Collapse Panel >>"))
    {
        m_IsPanelOpen = false;
    }
    if (ImGui::IsItemHovered())
    {
        // Tool tip is short, just use SetToolTip
        ImGui::SetTooltip("Collapse the settings panel and status bar (hotkey Tab)");
    }
    ImGui::End();
}
