
#include "Camera2D.hpp"
#include <algorithm>

using namespace Commons;

glm::vec2 Camera2D::WorldToScreen(float WorldX, float WorldY, const Layout::ViewportRect& InViewportRect) const
{
    glm::vec2 ScreenPos = glm::vec2(0.f);
    // Transform to screen space, accounting for pan and zoom
    ScreenPos.x = (WorldX - m_CameraPosX) * m_Zoom + InViewportRect.Width / 2.f;
    ScreenPos.y = -(WorldY - m_CameraPosY) * m_Zoom + InViewportRect.Height / 2.f;
    return ScreenPos;
}

glm::vec2 Camera2D::ScreenToWorld(float ScreenX, float ScreenY, const Layout::ViewportRect& InViewportRect) const
{
    glm::vec2 WorldPos = glm::vec2(0.f);
    WorldPos.x = (ScreenX - InViewportRect.Width / 2.f) / m_Zoom + m_CameraPosX;
    WorldPos.y = -(ScreenY - InViewportRect.Height / 2.f) / m_Zoom + m_CameraPosY;
    return WorldPos;
}

void Camera2D::Pan(float ScreenDX, float ScreenDY)
{
    m_CameraPosX -= ScreenDX / m_Zoom;
    m_CameraPosX = std::clamp(m_CameraPosX, m_LeftmostPan, m_RightmostPan);
    m_CameraPosY -= ScreenDY / m_Zoom;
    m_CameraPosY = std::clamp(m_CameraPosY, -Layout::VIEWPORT_HEIGHT_OPEN, Layout::VIEWPORT_HEIGHT_OPEN);
}

void Camera2D::ZoomAt(float ZoomDelta, float ScreenX, float ScreenY, Layout::ViewportRect& InViewportRect)
{
    // Convert the zoom target from screen to world
    glm::vec2 PrevWorldPos = glm::vec2(0.f);
    PrevWorldPos = ScreenToWorld(ScreenX, ScreenY, InViewportRect);

    // Apply zoom change (multiplicative, so it feels even at all levels)
    float Factor = 1.f + ZoomDelta * 0.1f;
    m_Zoom = std::clamp(m_Zoom * Factor, MIN_ZOOM, MAX_ZOOM);

    // Propagate the zoom change to the point
    glm::vec2 AfterWorldPos = ScreenToWorld(ScreenX, ScreenY, InViewportRect);

    // Adjust our camera locations so we are still looking at the same point
    m_CameraPosX += PrevWorldPos.x - AfterWorldPos.x;
    m_CameraPosY += PrevWorldPos.y - AfterWorldPos.y;
}

void Camera2D::Reset()
{
    m_CameraPosX = 0.f;
    m_CameraPosY = 0.f;
    m_Zoom = 1.f;
}

void Camera2D::OnViewportResized(const Layout::ViewportRect& InViewportRect)
{
    // Pan bounds are symmetric around the world origin, sized to the new viewport width.
    // Without this, collapsing the panel widens the viewport but the camera stays clamped
    // to the old narrower range, so the user can't pan into the newly-revealed area.
    m_RightmostPan = InViewportRect.Width;
    m_LeftmostPan = -InViewportRect.Width;
    m_CameraPosX = std::clamp(m_CameraPosX, m_LeftmostPan, m_RightmostPan);
}
