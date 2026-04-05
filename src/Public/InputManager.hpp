#pragma once
#include "glm/vec2.hpp"


/*
 * This class will handle the various inputs to the app
 */

/*
 * Enum that categorizes the type of input events(they are each combos of elemental input events from SDL
 * 1. No-op: plain mouse movement, dragging and scrolling not within the viewport
 * 2. CameraOrbit: dragging while LMB is pressed and within the viewport
 * 3. CameraZoom: scrolling while within the viewport
 * 4. Pause and unpause: pressing space
 * 5. Expand/Collapse viewport hotkey: tab
 * 6. Quit: Pressing escape or click the X
 */
enum class InputEvent : uint8_t
{
    NoOp,
    CameraOrbit,
    CameraPan = CameraOrbit, // 2D alias, same underlying input (LMB drag), different semantics
    CameraZoom,
    TogglePause,
    ToggleViewport,
    Quit
};

/*
 * Struct returned by ProcessInput to the application class
 * Packs the per frame input data
 */
struct InputResult
{
    // Mouse delta since last frame, for orbiting/panning
    glm::vec2 MouseDelta = glm::vec2(0.f);
    // Current mouse position, for zoom-at-cursor,
    glm::vec2 MousePosition = glm::vec2(0.f);
    // Scroll delta, for zooming
    float ScrollDelta = 0.f;
    InputEvent Event = InputEvent::NoOp;
};


class InputManager
{
public:
    // Constructors and destructors
    InputManager() = default;
    InputManager(const InputManager&) = delete;
    InputManager(InputManager&&) = delete;
    InputManager& operator=(const InputManager&) = delete;
    InputManager& operator=(InputManager&&) = delete;
    ~InputManager() = default;

    // Actual work functions
    InputResult ProcessInput(const bool IsPanelOpen);
    // Setter and getters
private:
    // Helper to figure out whether we are inside the viewport or not
    bool IsCursorInViewport(const glm::vec2& CursorPosition, const bool IsPanelOpen);
private:
    bool m_IsLMBPressed = false;
};

