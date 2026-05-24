

#include "Camera.hpp"

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
using namespace Commons;
Camera::Camera() : m_Position(glm::vec3(0.0f, 5.0f, -m_OrbitRadius)), m_Up(glm::vec3(0.0f, 1.0f, 0.0f)), m_Right(glm::vec3(1.0f, 0.0f, 0.0f)),
                   m_Forward(glm::vec3(0.0f, 0.0f, 1.0f)), m_LookAt(glm::vec3(0.f, 75.f, 0.f))
{

}

void Camera::Orbit(const InputResult& Input)
{
    // This function just takes an InputResult struct like how camera 2D does its Pan()
    // First calculate the new yaw and pitch. We have no roll here
    m_Yaw += m_RotationSpeedFactor * Input.MouseDelta.x;
    m_Yaw = glm::mod(m_Yaw, 360.f);
    m_Pitch += m_RotationSpeedFactor * (-Input.MouseDelta.y);
    m_Pitch = glm::clamp(m_Pitch, -45.f, 0.f); // No point looking from below the plane
    RecomputePosAndBasis();
}

void Camera::Zoom(const InputResult& Input)
{
    // First calculate how much we zoom
    const float ZoomDelta = Input.ScrollDelta * m_ZoomSpeedFactor;
    // Apply the zoom delta to zoom y, zoom x can then be calculated on the fly
    m_ZoomY += ZoomDelta;
    m_ZoomY = glm::clamp(m_ZoomY, MIN_ZOOM_Y, MAX_ZOOM_Y);
    m_ZoomX = m_ZoomY / Layout::ASPECT_RATIO;
    m_ZoomX = glm::clamp(m_ZoomX, MIN_ZOOM_X, MAX_ZOOM_X);
}
// This function is NOT NEEDED for the GPU path, since our vulkan manager already takes care of it
// However, I am keeping it here for future references
void Camera::AdjustCameraForResize(const Layout::ViewportRect &OldViewport,
                                   const Layout::ViewportRect &NewViewport)
{
    /*
     * When our viewport changes, it expands toward the bottom and the right
     * More importantly, the particle emitter's position shifts to the center of the entire app GUI
     * We need to adjust the camera's look at target
     */
    // Horizontal adjustment - while the viewport expands by the difference
    // The center of our look at only moved half that distance(viewport expands on both sides)
    float Dx = (NewViewport.Width - OldViewport.Width) * 0.5f;
    // Vertical adjustment
    float Dy = (NewViewport.Height - OldViewport.Height) * 0.5f;

    // Convert both adjustments to NDC
    Dx /= OldViewport.Width;
    Dy /= OldViewport.Height;

    // Scale by how much world space the camera sees at the look-at distance
    // For a perspective camera: visible width = 2 * distance * tan(fov/2) * aspect
    /* I do not fully understand the math, but basically,
     * With a perspective camera, the amount of world-space distance that fits ON SCREEN depends on:
     * the camera’s field of view
     * the distance from the camera to the plane you care about
     * Now our distance to the viewing plane is fixed. So we only consider the FOV's influences, the way to reason about this:
     * "I would like to move this many pixels on the screen, how much distance in the 3D world do I have to cross"
     * And this is decided by the FOV, because FOV basically decides how much world space is visible given ALL PIXELS on the screen
     * So if our FOV is large, meaning we can see more world space with the same amount of pixels, so:
     * - We have to move a larger distance in the 3D world to move the same amount of pixels on the screen
     * If our FOV is small, meaning we can less world space with the same number of pixels, so:
     * - We have to move a smaller distance in the 3D world to move the same amount of pixels on the screen
     *  a small world shift when our FOV is large
     *  a large world shift when far away
     */
    /* Image a cross-section of the view frustum, looking like this:
     * ----------- Depth
     * \         /  |
     *  \       /   |
     *   \     /    |
     *    \   /     |
     *     \ /      |
     *   Camera with a 90 FOV
     *   Notice how we can get the HALF of the side at Depth by doing Distance(Depth) * tan(Half Fov Angle)
     *
     *
     */
    const float HalfFovTan = glm::tan(glm::radians(Camera::FIELD_OF_VIEW * 0.5f));
    const float Distance = glm::length(m_Position - m_LookAt);
    const float WorldHeight = 2.f * Distance * HalfFovTan;
    const float AspectRatio = static_cast<float>(OldViewport.Width) / static_cast<float>(OldViewport.Height);
    const float WorldWidth = WorldHeight * AspectRatio;

    // Calculate the world-space offset along camera's up and right directions
    const glm::vec3 Offset = m_Right * Dx * WorldWidth + m_Up * Dy * WorldHeight;
    m_LookAt += Offset;
}

void Camera::SetOrbitRadius(const float InOrbitRadius)
{
    m_OrbitRadius = InOrbitRadius;
    RecomputePosAndBasis();
}

void Camera::GetViewMatrix(glm::mat4 &OutViewMatrix) const
{
    constexpr glm::vec3 WorldUp = glm::vec3(0.f, 1.f, 0.f);
    OutViewMatrix = glm::lookAtLH(m_Position, m_LookAt, WorldUp);
}

void Camera::GetProjectionMatrix(glm::mat4 &OutProjectionMatrix, const float AspectRatio)
{
    OutProjectionMatrix = glm::perspectiveLH_ZO(glm::radians(FIELD_OF_VIEW), AspectRatio, NEAR_PLANE, FAR_PLANE);
    // According to math book, the projection matrix _01 should contain ZoomX and matrix _11 is zoom y
    OutProjectionMatrix[0][0] *= m_ZoomX;
    OutProjectionMatrix[1][1] *= m_ZoomY;
}

void Camera::RecomputePosAndBasis()
{
    // Now update the camera positions and its local basis
    const float YawRad = glm::radians(m_Yaw);
    const float PitchRad = glm::radians(m_Pitch);
    const float CosPitch = glm::cos(PitchRad);
    // X = cos(pitch) * sin(yaw), Y = -sin(pitch), Z = cos(pitch) * cos(yaw)
    m_Position = m_LookAt + m_OrbitRadius * glm::vec3(CosPitch * glm::sin(YawRad), -glm::sin(PitchRad),
        CosPitch * glm::cos(YawRad));
    constexpr glm::vec3 WorldUp = glm::vec3(0.f, 1.f, 0.f);
    m_Forward = glm::normalize(m_LookAt - m_Position);
    // World up is "co-planar" with the actual up (off by some translations, which does not affect vectors technically)
    // So we can just use it to get the right vector using cross product
    m_Right = glm::normalize(glm::cross(WorldUp, m_Forward));
    m_Up = glm::normalize(glm::cross(m_Forward, m_Right));
}

