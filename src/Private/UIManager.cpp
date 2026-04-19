//External libs and STL
#include <utility>
#include "imgui.h"
//Own headers
#include "Commons.hpp"
#include "UIManager.hpp"

#include "ParticlePresets.hpp"


UIManager::UIManager(std::function<void(bool SetLoopEnable)> InToggleLoopCallback) : m_ToggleLoopCallback(std::move(InToggleLoopCallback)){
}

bool UIManager::UIFrame(const DeltaTimeData& InDeltaTimeData, ParticleSimulatorConfig& ParticleConfig,
    uint32_t ParticleCount, bool IsPaused)
{
    bool IsConfigDirty = false;
    // Settings panel
    if (!m_IsPanelOpen)
    {
        DrawPanelExpandButton();
        return IsConfigDirty;
    }
    DrawStatusBar(InDeltaTimeData, ParticleCount, IsPaused);
    IsConfigDirty = GetParticleSimulatorConfig(ParticleConfig);
    return IsConfigDirty;
}

void UIManager::DrawStatusBar(const DeltaTimeData& InDeltaTimeData, uint32_t ParticleCount, bool IsPaused) const
{
    using namespace Commons::Layout;
    // Position: full width, at the bottom of the window
    ImGui::SetNextWindowPos(ImVec2(0, VIEWPORT_HEIGHT_OPEN));
    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, STATUS_BAR_HEIGHT));

    // Window flags: no decorations, no interaction, no scrolling.
    // NoBackground is not set, we want a solid background to clearly
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
    if (IsPaused)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Simulation Paused");
    }
    else
    {
        ImGui::Text("Simulation Running");
    }
    ImGui::Text("Press Space to toggle pause. Press Tab to toggle viewport");
    ImGui::End();
}

bool UIManager::GetParticleSimulatorConfig(ParticleSimulatorConfig &Config)
{
    using namespace Commons;
    // Start drawing the entire settings panel
    ImGui::SetNextWindowPos(ImVec2(Layout::VIEWPORT_WIDTH_OPEN, 0));
    ImGui::SetNextWindowSize(ImVec2(Layout::PANEL_WIDTH, Layout::WINDOW_HEIGHT));
    // Set flags
    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
    ImGui::Begin("Particle Simulator Settings", nullptr, WindowFlags);

    bool Result = false;

    Result |= DrawSettingsPanel(Config);
    Result |= DrawParticleInit(Config);
    Result |= DrawParticleVisuals(Config);
    Result |= DrawForceSettings(Config.ForceConfigData);
    // End drawing settings panel
    ImGui::End();
    return Result;
}

bool UIManager::DrawSettingsPanel(ParticleSimulatorConfig& Config)
{
    // Preset selectors
    // Currently, we are using a system where the enum constant's values must match the indices in this array
    // Fragile and EVIL, should probably look for a more stable solution
    bool IsConfigDirty = false;
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
                IsConfigDirty = true;
                break;
            }
            case PresetType::Firework:
            {
                Config = ParticlePresets::Firework;
                IsConfigDirty = true;
                break;
            }
            case PresetType::Fountain:
            {
                Config = ParticlePresets::Fountain;
                IsConfigDirty = true;
                break;
            }
            case PresetType::Vortex:
            {
                Config = ParticlePresets::Vortex;
                IsConfigDirty = true;
                break;
            }
            case PresetType::Waterfall:
            {
                Config = ParticlePresets::Waterfall;
                IsConfigDirty = true;
                break;
            }
            case PresetType::Snow:
            {
                Config = ParticlePresets::Snow;
                IsConfigDirty = true;
                break;
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    /*
     * Note that ImGui's widget functions return true when its value get changed
     * Therefore, we just or it with our is config dirty boolean
     * so we easily check whether any of the setting was changed and informed application
     */

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
        IsConfigDirty = true;
    }
    if (Config.Mode == EmitterMode::Burst)
    {
        // How often does the emitter spawn particles in burst mode
        IsConfigDirty |= ImGui::SliderFloat("Burst Interval", &Config.BurstInterval, 0.5f, 2.5f);
        // We reuse the Emitter Rate variable for both burst and continuous emitter
        IsConfigDirty |= ImGui::SliderInt("Particle Count Per Burst", &Config.EmissionRate,
            5, 3500);
    }
    else
    {
        // These numbers are kinda arbitrary, might be better to define them in commons.hpp
        // As constants, however, since we are just using them here, it's ok for now
        IsConfigDirty |= ImGui::SliderInt("Particle Per Second", &Config.EmissionRate, 50, 5000);
    }

    // Emitter lifetime - how long will our emitter spawn particles for
    IsConfigDirty |= ImGui::SliderFloat("Emitter Lifetime", &Config.EmitterLifeTime, 0.5f, 10.f, "%.1f");

    // Spawn shape settings
    if (ImGui::CollapsingHeader("Emitter Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Spawn shapes
        const char* Shapes[] = {"Sphere", "Box/Plane", "Cone", "Ring/Disc", "Cylinder"};
        int CurrentShape = static_cast<int>(Config.Shape);
        if (ImGui::Combo("Shape", &CurrentShape, Shapes, IM_ARRAYSIZE(Shapes)))
        {
            Config.Shape = static_cast<enum SpawnShape>(CurrentShape);
            IsConfigDirty = true;
        }

        /* Shape-specific parameters
         * Only show the parameters relevant to the current shape.
         * This is a natural use of ImGui's immediate mode — just don't
         * call the widgets for shapes that aren't active.
         */
        switch (Config.Shape)//
        {
            case SpawnShape::Sphere:
            {
                IsConfigDirty |= ImGui::SliderFloat("Radius", &Config.SphereRadius,
                    0.1f, 50.f, "%.1f");
                break;
            }
            case SpawnShape::Cone:
            {
                IsConfigDirty |= ImGui::SliderFloat("Axis Length", &Config.ConeDimensions.x,
                    0.1f, 50.f, "%.1f");
                IsConfigDirty |= ImGui::SliderFloat("Half Angle", &Config.ConeDimensions.y,
                    0.5f, 45.f, "%.1f");
                break;
            }
            case SpawnShape::Box:
            {
                // Min is 0 so the user can flatten one axis to make a plane
                bool DimensionDirty = false;
                DimensionDirty |= ImGui::SliderFloat("Width", &Config.BoxDimensions.x,
                    0.f, 100.f, "%.1f");
                DimensionDirty |= ImGui::SliderFloat("Height", &Config.BoxDimensions.y,
                    0.f, 100.f, "%.1f");
                DimensionDirty |= ImGui::SliderFloat("Depth", &Config.BoxDimensions.z,
                    0.f, 100.f, "%.1f");
                if (DimensionDirty)
                {
                    // At most one axis can be zero (plane), otherwise SpawnParticles_Speed
                    // gets a zero-length vector and produces NaN velocities
                    const bool IsXZero = Config.BoxDimensions.x < 0.1f;
                    const bool IsYZero = Config.BoxDimensions.y < 0.1f;
                    const bool IsZZero = Config.BoxDimensions.z < 0.1f;
                    const int ZeroCount = IsXZero + IsYZero + IsZZero;
                    if (ZeroCount >= 2)
                    {
                        // Clamp non-zero axes to at least 0.1, keep the user-set zero one flat
                        if (IsXZero)
                        {
                            Config.BoxDimensions.x = 0.f;
                        }
                        else
                        {
                            Config.BoxDimensions.x = std::max(Config.BoxDimensions.x, 0.1f);
                        }
                        if (IsYZero)
                        {
                            Config.BoxDimensions.y = 0.f;
                        }
                        else { Config.BoxDimensions.y = std::max(Config.BoxDimensions.y, 0.1f); }
                        if (IsZZero)
                        {
                            Config.BoxDimensions.z = 0.f;
                        }
                        else
                        {
                            Config.BoxDimensions.z = std::max(Config.BoxDimensions.z, 0.1f);
                        }
                    }
                    // All three near-zero — default to XY plane
                    if (ZeroCount == 3)
                    {
                        Config.BoxDimensions.x = 0.1f;
                        Config.BoxDimensions.y = 0.1f;
                    }
                }
                IsConfigDirty |= DimensionDirty;
                break;
            }
            case SpawnShape::Ring:
            {
                IsConfigDirty |= ImGui::SliderFloat("Inner Radius", &Config.RingDimensions.x,
                    0.f, 50.f, "%.1f");
                IsConfigDirty |= ImGui::SliderFloat("Outer Radius", &Config.RingDimensions.y,
                    0.f, 50.f, "%.1f");
                IsConfigDirty |= ImGui::SliderFloat("Height", &Config.RingDimensions.z,
                    0.f, 100.f, "%.1f");
                // Clamp the min to be smaller than max
                if (Config.RingDimensions.x > Config.RingDimensions.y)
                {
                    Config.RingDimensions.x = Config.RingDimensions.y - 0.1f;
                }
                break;
            }
            case SpawnShape::Cylinder:
            {
                IsConfigDirty |= ImGui::SliderFloat("Height", &Config.CylinderDimensions.y,
                    0.f, 80.f, "%.1f");
                IsConfigDirty |= ImGui::SliderFloat("Radius", &Config.CylinderDimensions.x,
                    0.f, 80.f, "%.1f");
                break;
            }
            default:
                break;
        }
    }
    ImGui::Spacing();
    return IsConfigDirty;
}
bool UIManager::DrawParticleInit(ParticleSimulatorConfig &Config) {
    bool IsConfigDirty = false;
    if (ImGui::CollapsingHeader("Particle Initialization", ImGuiTreeNodeFlags_DefaultOpen)) {
        // First check whether the user wants random speed at spawn
        bool IsRandomSpeed = Config.IsRandomSpeed;
        IsConfigDirty |= ImGui::Checkbox("Random Speed at Spawn", &IsRandomSpeed);
        if (IsRandomSpeed)
        {
            // Min and max base speed at spawn
            IsConfigDirty |= ImGui::SliderFloat("Min Particle Velocity at Spawn", &Config.Speed.x,
                0.f, 100.f);
            IsConfigDirty |= ImGui::SliderFloat("Max Particle Velocity at Spawn", &Config.Speed.y,
                0.f, 100.f);
            // Clamp
            if (Config.Speed.x >= Config.Speed.y)
            {
                Config.Speed.x = std::max(0.f, Config.Speed.y - 0.1f);
            }
        }
        else
        {
            // No random interval, just a fix speed
            IsConfigDirty |= ImGui::SliderFloat("Particle velocity at Spawn", &Config.Speed.x,
                0.f, 100.f);
        }
        Config.IsRandomSpeed = IsRandomSpeed;

        ImGui::Spacing();

        bool IsRandomLifeTime = Config.IsRandomLifeTime;
        IsConfigDirty |= ImGui::Checkbox("Random Lifetime", &IsRandomLifeTime);
        if (IsRandomLifeTime)
        {
            // Min and max lifetime
            IsConfigDirty |= ImGui::SliderFloat("Lifetime Min", &Config.LifeTime.x,
                0.1f, 10.f, "%.1f");
            IsConfigDirty |= ImGui::SliderFloat("Lifetime Max", &Config.LifeTime.y,
                0.1f, 10.f, "%.1f");
            // Remember to clamp!
            if (Config.LifeTime.x >= Config.LifeTime.y)
            {
                Config.LifeTime.x = std::max(0.f, Config.LifeTime.y - 0.1f);
            }
        }
        else
        {
            // No random interval for lifetime, just fix lifetime
            IsConfigDirty |= ImGui::SliderFloat("Lifetime", &Config.LifeTime.x,
                0.1f, 10.f, "%.1f");
        }
        Config.IsRandomLifeTime = IsRandomLifeTime;

        ImGui::Spacing();

        bool IsRandomScale = Config.IsRandomScale;
        IsConfigDirty |= ImGui::Checkbox("Random Size at Spawn", &IsRandomScale);
        if (IsRandomScale)
        {
            // Min and max scales of the particles
            IsConfigDirty |= ImGui::SliderFloat("Size Min", &Config.Scale.x,
                0.1f, 1.5f, "%.1f");
            IsConfigDirty |= ImGui::SliderFloat("Size Max", &Config.Scale.y,
                0.1f, 1.5f, "%.1f");
            if (Config.Scale.x >= Config.Scale.y)
            {
                Config.Scale.x = std::max(0.1f, Config.Scale.y - 0.1f);
            }
        }
        else
        {
            // No random interval for spawn size, just fixed
            IsConfigDirty |= ImGui::SliderFloat("Size", &Config.Scale.x, 0.1f, 1.5f, "%.1f");
        }
        Config.IsRandomScale = IsRandomScale;

        ImGui::Spacing();
    }
    ImGui::Spacing();
    return IsConfigDirty;
}

bool UIManager::DrawParticleVisuals(ParticleSimulatorConfig &Config)
{
    bool IsConfigDirty = false;
    /*
     * Color picker flags
     * These flags configure what the picker widget displays.
     * We can change these to customize the picker appearance.
     * Available picker styles:
     *   PickerHueWheel, circular hue ring with SV square inside
     *   PickerHueBar vertical hue bar with SV square beside it
     * Available display modes :
     *   DisplayRGB, show R, G, B number input boxes
     *   DisplayHSV, show H, S, V number input boxes
     *   DisplayHex, show hex input (#RRGGBB)
     * Number format:
     *   Float, show values as 0.000–1.000
     *   (default), show values as 0–255
     */
    ImGuiColorEditFlags ColorEditorFlags = ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_DisplayRGB |
        ImGuiColorEditFlags_Float;
    /*
     * Using ColorPicker3, this shows the entire picker widget, which takes up a lot of space
     * If there's too much visual clutter we will switch to ColorEdittor
     */
    float StartColor[3] = {Config.StartColor.r, Config.StartColor.g, Config.StartColor.b};
    ImGui::Text("Particle Color at Spawn: ");
    if (ImGui::ColorPicker3("StartColorPicker", StartColor, ColorEditorFlags))
    {
        Config.StartColor = {StartColor[0], StartColor[1], StartColor[2]};
        IsConfigDirty = true;
    }

    ImGui::Spacing();

    // Random per-particle color takes precedence over scaling, so grey out
    // the whole scale-color block when randomization is on.
    ImGui::BeginDisabled(Config.IsRandomColor);
    bool IsScalingColor = Config.IsScalingColor;
    IsConfigDirty |= ImGui::Checkbox("Scale color", &IsScalingColor);
    Config.IsScalingColor = IsScalingColor;
    if (IsScalingColor)
    {
        float EndColor[3] = {Config.EndColor.r, Config.EndColor.g, Config.EndColor.b};
        ImGui::Text("Particle Color at Death: ");
        if (ImGui::ColorEdit3("EndColorPicker", EndColor, ColorEditorFlags))
        {
            Config.EndColor = {EndColor[0], EndColor[1], EndColor[2]};
            IsConfigDirty = true;
        }
        /*
         * Show a preview of start-to-end color transition using ImGui's low-level draw list API.
         *
         * ImGui has two rendering paths: the widget API (SliderFloat, Checkbox, etc.) which handles
         * layout and interaction automatically, and the draw list API which lets us draw arbitrary
         * shapes directly. The draw list is what ImGui itself uses internally to render widgets.
         *
         * AddRectFilledMultiColor() draws a filled rectangle with independently colored corners
         * The GPU interpolates between the four corner colors across the rect. By setting top-left
         * and bottom-left to StartColor, and top-right and bottom-right to EndColor,
         * we get a horizontal gradient from start to end color.
         *
         * The corner colors must be packed as ImU32 (ABGR uint32), not float4. ImGui provides
         * ColorConvertFloat4ToU32() for this conversion.
         *
         * GetCursorScreenPos() returns the current layout cursor in absolute screen coordinates
         * (not window-relative). This is where the next widget would be placed. We use it as the
         * top-left corner of our gradient rect, and offset by BarSize for the bottom-right.
         *
         * Because AddRectFilledMultiColor draws directly to the draw list and bypasses ImGui's
         * layout system, ImGui doesn't know we drew anything, the cursor doesn't advance, and
         * subsequent widgets would overlap our gradient. Dummy(BarSize) fixes this by reserving
         * the equivalent space in the layout without drawing anything visible.
         */
        const ImVec2 BarSize(ImGui::GetContentRegionAvail().x, 20.f);
        const ImVec2 CursorPos = ImGui::GetCursorScreenPos();
        const ImU32 StartColorUint = ImGui::ColorConvertFloat4ToU32(ImVec4(StartColor[0], StartColor[1], StartColor[2], 1.f));
        const ImU32 EndColorUint = ImGui::ColorConvertFloat4ToU32(ImVec4(EndColor[0], EndColor[1], EndColor[2], 1.f));

        // Get the draw list for the window, this list contains what imgui is about to draw to the window
        // If we directly append to this we need to let imGui know
        ImGui::GetWindowDrawList()->AddRectFilledMultiColor(CursorPos, ImVec2(CursorPos.x + BarSize.x, CursorPos.y + BarSize.y)
        , StartColorUint, EndColorUint, StartColorUint, EndColorUint);
        // AddRectFilledColor bypasses ImGui's layout system so imGui does not know we had drawn to the preview region yet
        // Call Dummy() to reserve space for the color transition preview
        ImGui::Dummy(BarSize);
        ImGui::Text("Start to End Color Transition Preview");
        ImGui::Spacing();
    }
    ImGui::EndDisabled();
    IsConfigDirty |= ImGui::Checkbox("Randomize Colors for Individual Particle", &Config.IsRandomColor);
    if (ImGui::IsItemHovered())
    {
        // We can also use SetToolTip if the tool tip text is a single liner
        ImGui::BeginTooltip();
        ImGui::Text("If checked, each particle will have a random color at spawn.");
        ImGui::Separator();
        ImGui::Text("If enabled, the start and end color parameters will be ignored.");
        ImGui::EndTooltip();
    }
    return IsConfigDirty;
}

bool UIManager::DrawForceSettings(ForceConfig& ForceConfig)
{
    bool IsConfigDirty = false;
    using namespace Commons;
    if (ImGui::CollapsingHeader("Forces Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Gravity is always applied, no need to add
        IsConfigDirty |= ImGui::SliderFloat("Gravity Scale", &ForceConfig.Gravity, 0.f, 5.f, "%.1f");

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
                    if (ForceConfig.ExtraForceCount >= Constants::MAX_NUM_FORCES)
                    {
                        ImGui::CloseCurrentPopup();
                        break;
                    }
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
                            NewForceDataRef.PointRadius = 50.f;
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
                            NewForceDataRef.WindPeriod = 1.5f;
                            break;
                        }
                        default:
                            break;
                    }
                    ForceConfig.ExtraForceCount++;
                    IsConfigDirty = true;
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
            IsConfigDirty |= ImGui::Checkbox("##Enabled", &IsForceEnabledRef);
            ImGui::SameLine();

            const bool IsHeadOpen = ImGui::TreeNode("ForceHeader", "%s", ForceNames[ForceNameIndex]);

            // Right align the remove button
            // Can't really use the panel width constant, because that includes window padding
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 60.f);
            if (ImGui::SmallButton("Remove"))
            {
                RemovalIndex = i;
            }
            if (IsHeadOpen)
            {
                // Force type-specific params
                switch (ForceConfig.ForceTypes[i])
                {
                    case ForceType::Drag:
                    {
                        IsConfigDirty |= ImGui::SliderFloat("Drag Coefficient", &ForceDataRef.Strength,
                            0.f, 10.f, "%.1f");
                        break;
                    }
                    case ForceType::Point:
                    {
                        ImGui::Text("Positive Strength = attraction, Negative Strength = repulsion");
                        ImGui::Spacing();
                        IsConfigDirty |= ImGui::SliderFloat("Point Force Strength", &ForceDataRef.Strength,
                            -75.f, 75.f, "%.1f");
                        IsConfigDirty |= ImGui::SliderFloat("Influence Radius", &ForceDataRef.PointRadius,
                            1.f, 350.f, "%.0f");
                        IsConfigDirty |= ImGui::SliderFloat("Position X", &ForceDataRef.Direction.x,-50.f, 50.f, "%.1f");
                        IsConfigDirty |= ImGui::SliderFloat("Position Y", &ForceDataRef.Direction.y, -50.f, 50.f, "%.1f");
                        IsConfigDirty |= ImGui::SliderFloat("Position Z", &ForceDataRef.Direction.z, 0.f, 20.f, "%.1f");
                        break;
                    }
                    case ForceType::Vortex:
                    {
                        IsConfigDirty |= ImGui::SliderFloat("Vortex Tangential Strength", &ForceDataRef.Strength,
                            0.f, 50.f, "%.1f");
                        IsConfigDirty |= ImGui::SliderFloat("Vortex Radial Strength", &ForceDataRef.VortexPull,
                            0.f, 50.f, "%.1f");
                        break;
                    }
                    case ForceType::Directional:
                    {
                        IsConfigDirty |= ImGui::SliderFloat("Wind Strength", &ForceDataRef.Strength,
                            0.f, 75.f, "%.1f");
                        IsConfigDirty |= ImGui::SliderFloat("Wind Period", &ForceDataRef.WindPeriod,
                             0.5f, 5.f, "%.1f Seconds");
                        IsConfigDirty |= ImGui::SliderFloat("Wind Direction X", &ForceDataRef.Direction.x,
                             -1.f, 1.f, "%.1f");
                        IsConfigDirty |= ImGui::SliderFloat("Wind Direction Y", &ForceDataRef.Direction.y,
                            -1.f, 1.f, "%.1f");
                        IsConfigDirty |= ImGui::SliderFloat("Wind Direction Z", &ForceDataRef.Direction.z,
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
            IsConfigDirty = true;
        }
    }
    return IsConfigDirty;
}

void UIManager::DrawPanelExpandButton() {
    using namespace Commons;
    // Size the window to the actual label at the current font/style scale.
    // Hardcoded sizes conflict agaisnt the ScaleAllSizes(UIScale) bump in SoftwareRenderer
    // the label overruns and only the first couple of chars end up visible.
    const ImGuiStyle& Style = ImGui::GetStyle();
    constexpr const char* Label = "<< Expand Panel";
    const ImVec2 TextSize = ImGui::CalcTextSize(Label);
    const float WindowWidth = TextSize.x + Style.FramePadding.x * 2.f + Style.WindowPadding.x * 2.f;
    const float WindowHeight = TextSize.y + Style.FramePadding.y * 2.f + Style.WindowPadding.y * 2.f;
    constexpr float ButtonMargin = 8.5f;

    // Anchor to the top right corner of the window
    ImGui::SetNextWindowPos(
        ImVec2(static_cast<float>(Layout::WINDOW_WIDTH) - WindowWidth - ButtonMargin, ButtonMargin));
    ImGui::SetNextWindowSize(ImVec2(WindowWidth, WindowHeight));

    // Just like WIN32, when we create any window, we need to set flags
    ImGuiWindowFlags Flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    // Begin ImGui Frame again
    ImGui::Begin("Panel Toggle Button", nullptr, Flags);
    if (ImGui::Button(Label))
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
     */
    constexpr const char* HintLabel = "Press Tab to restore UI";
    const ImVec2 HintTextSize = ImGui::CalcTextSize(HintLabel);
    const float HintWidth = HintTextSize.x + Style.WindowPadding.x * 2.f;
    const float HintHeight = HintTextSize.y + Style.WindowPadding.y * 2.f;
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(Layout::WINDOW_WIDTH) - HintWidth,
               static_cast<float>(Layout::WINDOW_HEIGHT) - HintHeight));
    ImGui::SetNextWindowSize(ImVec2(HintWidth, HintHeight));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
                          ImVec4(0.0f, 0.0f, 0.0f, 0.3f));

    ImGui::Begin("Tab Hint", nullptr, Flags);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.5f), "%s", HintLabel);
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}
