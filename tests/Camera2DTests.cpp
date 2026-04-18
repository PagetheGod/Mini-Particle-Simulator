#include "Camera2D.hpp"
#include "TestHarness.hpp"

namespace {

// Shared tolerance for camera math round-trips.
constexpr float kEpsilon = 0.001f;

Commons::Layout::ViewportRect MakeViewport()
{
    // These tests target the standard "panel open" viewport because the camera
    // math should behave correctly in the default app layout.
    return Commons::Layout::GetViewportRect(true);
}

}  // namespace

TEST_CASE(Camera2D_WorldScreenRoundTrip)
{
    // Start from a default camera with no translation or zoom applied.
    Camera2D camera;
    // Use a realistic viewport so the conversion matches app behavior.
    const auto viewport = MakeViewport();

    // Convert a known world-space point into screen coordinates.
    const glm::vec2 screen = camera.WorldToScreen(12.5f, -7.25f, viewport);
    // Convert that screen-space point back into world space.
    const glm::vec2 world = camera.ScreenToWorld(screen.x, screen.y, viewport);

    // The round-trip should recover the original coordinates within tolerance.
    REQUIRE_NEAR(world.x, 12.5f, kEpsilon);
    REQUIRE_NEAR(world.y, -7.25f, kEpsilon);
}

TEST_CASE(Camera2D_ZoomAtKeepsCursorWorldPositionStable)
{
    // ZoomAt() is meant to zoom around the cursor, not around the origin.
    Camera2D camera;
    auto viewport = MakeViewport();

    // Pick an off-center cursor so the test catches accidental center-based zoom.
    const float cursor_x = viewport.Width * 0.33f;
    const float cursor_y = viewport.Height * 0.61f;
    // Record which world-space point currently sits under that cursor.
    const glm::vec2 world_before = camera.ScreenToWorld(cursor_x, cursor_y, viewport);

    // Apply a zoom-in step around the chosen cursor location.
    camera.ZoomAt(1.0f, cursor_x, cursor_y, viewport);

    // Ask again which world-space point is under the exact same cursor.
    const glm::vec2 world_after = camera.ScreenToWorld(cursor_x, cursor_y, viewport);
    // A correct zoom implementation keeps the same world point pinned in place.
    REQUIRE_NEAR(world_before.x, world_after.x, kEpsilon);
    REQUIRE_NEAR(world_before.y, world_after.y, kEpsilon);
}

TEST_CASE(Camera2D_PanRespectsBounds)
{
    // The camera should clamp extreme pans so the scene cannot drift forever.
    Camera2D camera;

    // Try to move far beyond any reasonable world extents.
    camera.Pan(-50000.0f, -50000.0f);

    // Horizontal position must remain inside the layout-defined bounds.
    REQUIRE(camera.GetCameraPosX() <= Commons::Layout::VIEWPORT_WIDTH_OPEN);
    REQUIRE(camera.GetCameraPosX() >= -Commons::Layout::VIEWPORT_WIDTH_OPEN);
    // Vertical position must remain inside the layout-defined bounds.
    REQUIRE(camera.GetCameraPosY() <= Commons::Layout::VIEWPORT_HEIGHT_OPEN);
    REQUIRE(camera.GetCameraPosY() >= -Commons::Layout::VIEWPORT_HEIGHT_OPEN);
}

TEST_CASE(Camera2D_ResetRestoresDefaults)
{
    // Move and zoom the camera away from its default state first.
    Camera2D camera;
    auto viewport = MakeViewport();

    camera.Pan(200.0f, -120.0f);
    camera.ZoomAt(2.0f, viewport.Width * 0.5f, viewport.Height * 0.5f, viewport);
    // Reset should bring every user-visible camera setting back to startup.
    camera.Reset();

    // Position should return to the centered origin.
    REQUIRE_NEAR(camera.GetCameraPosX(), 0.0f, kEpsilon);
    REQUIRE_NEAR(camera.GetCameraPosY(), 0.0f, kEpsilon);
    // Zoom should return to the neutral 1x scale.
    REQUIRE_NEAR(camera.GetZoom(), 1.0f, kEpsilon);
}
