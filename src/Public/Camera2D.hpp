#pragma once
#include "glm/glm.hpp"
#include "Commons.hpp"

/*
 * Lightweight 2D camera for the software rendering path
 * Handles pan (drag) and zoom (scroll wheel) with a simple scale + translate transform
 * No projection matrices, no depth, no billboards — just three floats
 */
class Camera2D
{
public:
    Camera2D() = default;
    ~Camera2D() = default;

    // Convert a world-space position to screen-space position.
    // The viewport center corresponds to the camera's pan position in world space.
    glm::vec2 WorldToScreen(float WorldX, float WorldY, const Commons::Layout::ViewportRect& InViewportRect) const;

    // Convert screen-space position back to world-space.
    // Useful for spawning particles at the cursor position.
    glm::vec2 ScreenToWorld(float ScreenX, float ScreenY, const Commons::Layout::ViewportRect& InViewportRect) const;

    // Pan the camera by a screen-space delta (e.g., from mouse drag).
    // Dividing by Zoom ensures panning feels consistent at all zoom levels:
    // when zoomed in 2x, dragging 100px moves the world 50 units.
    void Pan(float ScreenDX, float ScreenDY);

    // Zoom toward a screen-space point (e.g., the cursor position).
    // This makes the point under the cursor stay under the cursor after zooming.
    void ZoomAt(float ZoomDelta, float ScreenX, float ScreenY,
        Commons::Layout::ViewportRect& InViewportRect);

    // Reset to default view (centered, no zoom)
    void Reset();

    // Getters
    [[nodiscard]] float GetZoom() const { return m_Zoom; }
    [[nodiscard]] float GetCameraPosX() const { return m_CameraPosX; }
    [[nodiscard]] float GetCameraPosY() const { return m_CameraPosY; }

private:
    // Camera position in world space (the world point at the center of the viewport)
    // Since we do not need to rotate and orbit the camera in 2D, we just simplify to always look ahead
    // Hence we only need these two locations
    float m_CameraPosX = 0.f;
    float m_CameraPosY = 0.f;
    // Zoom level. 1.0 = no zoom. 2.0 = 2x magnification. 0.5 = zoomed out.
    float m_Zoom = 1.f;
    float m_RightmostPan = Commons::Layout::VIEWPORT_WIDTH_OPEN;

    // Constants
    static constexpr float MIN_ZOOM = 0.1f;
    static constexpr float MAX_ZOOM = 2.5f;
    static constexpr float LEFTMOST_PAN = 0.f;
};
