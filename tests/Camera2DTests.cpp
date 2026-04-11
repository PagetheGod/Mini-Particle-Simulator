#include "Camera2D.hpp"
#include "TestHarness.hpp"

namespace {

constexpr float kEpsilon = 0.001f;

Commons::Layout::ViewportRect MakeViewport()
{
    return Commons::Layout::GetViewportRect(true);
}

}  // namespace

TEST_CASE(Camera2D_WorldScreenRoundTrip)
{
    Camera2D camera;
    const auto viewport = MakeViewport();

    const glm::vec2 screen = camera.WorldToScreen(12.5f, -7.25f, viewport);
    const glm::vec2 world = camera.ScreenToWorld(screen.x, screen.y, viewport);

    REQUIRE_NEAR(world.x, 12.5f, kEpsilon);
    REQUIRE_NEAR(world.y, -7.25f, kEpsilon);
}

TEST_CASE(Camera2D_ZoomAtKeepsCursorWorldPositionStable)
{
    Camera2D camera;
    auto viewport = MakeViewport();

    const float cursor_x = viewport.Width * 0.33f;
    const float cursor_y = viewport.Height * 0.61f;
    const glm::vec2 world_before = camera.ScreenToWorld(cursor_x, cursor_y, viewport);

    camera.ZoomAt(1.0f, cursor_x, cursor_y, viewport);

    const glm::vec2 world_after = camera.ScreenToWorld(cursor_x, cursor_y, viewport);
    REQUIRE_NEAR(world_before.x, world_after.x, kEpsilon);
    REQUIRE_NEAR(world_before.y, world_after.y, kEpsilon);
}

TEST_CASE(Camera2D_PanRespectsBounds)
{
    Camera2D camera;

    camera.Pan(-50000.0f, -50000.0f);

    REQUIRE(camera.GetCameraPosX() <= Commons::Layout::VIEWPORT_WIDTH_OPEN);
    REQUIRE(camera.GetCameraPosX() >= -Commons::Layout::VIEWPORT_WIDTH_OPEN);
    REQUIRE(camera.GetCameraPosY() <= Commons::Layout::VIEWPORT_HEIGHT_OPEN);
    REQUIRE(camera.GetCameraPosY() >= -Commons::Layout::VIEWPORT_HEIGHT_OPEN);
}

TEST_CASE(Camera2D_ResetRestoresDefaults)
{
    Camera2D camera;
    auto viewport = MakeViewport();

    camera.Pan(200.0f, -120.0f);
    camera.ZoomAt(2.0f, viewport.Width * 0.5f, viewport.Height * 0.5f, viewport);
    camera.Reset();

    REQUIRE_NEAR(camera.GetCameraPosX(), 0.0f, kEpsilon);
    REQUIRE_NEAR(camera.GetCameraPosY(), 0.0f, kEpsilon);
    REQUIRE_NEAR(camera.GetZoom(), 1.0f, kEpsilon);
}
