

#include "Camera.hpp"

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"

Camera::Camera() : m_Position(glm::vec3(0.0f, 0.0f, 0.0f)), m_Up(glm::vec3(0.0f, 0.0f, 0.0f)), m_Right(glm::vec3(0.0f, 0.0f, 0.0f)),
                   m_Forward(glm::vec3(0.0f, 0.0f, 0.0f)), m_OriginLookAt(glm::vec3(0.f)), m_CurrentLookAt(glm::vec3(0.f)),
                   m_HorizontalAxis(1.f, 0.f, 0.f), m_VerticalAxis(0.f, 1.f, 0.f)
{

}

void Camera::OrbitYaw(const float InDirection)
{
    m_Yaw += m_RotationSpeed * InDirection;
    m_Yaw = glm::mod(m_Yaw, 360.f);
    const float YawRadians = glm::radians(m_Yaw);
    const float NewX = m_OrbitRadius * glm::cos(YawRadians);
    const float NewZ = m_OrbitRadius * glm::sin(YawRadians);
    m_Position = glm::vec3(NewX, m_Position.y, NewZ);
    m_Forward = glm::normalize(glm::vec3(-NewX, 0.f, -NewZ));
    m_Right = glm::normalize(glm::cross(m_Up, m_Forward));
}

void Camera::OrbitPitch(const float InDirection)
{
    m_Pitch += m_RotationSpeed * InDirection;
    m_Pitch = glm::clamp(m_Pitch, -88.f, 88.f); // Gimbal lock!
    const float PitchRadians = glm::radians(m_Pitch);
    const float NewY = m_OrbitRadius * glm::sin(PitchRadians);
    const float NewZ = m_OrbitRadius * glm::cos(PitchRadians);
    m_Position = glm::vec3(m_Position.x, NewY, NewZ);
    m_Forward = glm::normalize(glm::vec3(-m_Position.x, 0.f, -m_Position.z));
    m_Up = glm::normalize(glm::cross(m_Forward, m_Right));
}

void Camera::AdjustCameraForResize(const Commons::Layout::ViewportRect &OldViewport,
    const Commons::Layout::ViewportRect &NewViewport)
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
    const float Distance = glm::length(m_Position - m_OriginLookAt);
    const float WorldHeight = 2.f * Distance * HalfFovTan;
    const float AspectRatio = static_cast<float>(OldViewport.Width) / static_cast<float>(OldViewport.Height);
    const float WorldWidth = WorldHeight * AspectRatio;

    // Calculate the world-space offset along camera's up and right directions
    const glm::vec3 Offset = m_Right * Dx * WorldWidth + m_Up * Dy * WorldHeight;
    m_OriginLookAt = m_CurrentLookAt;
    m_CurrentLookAt += Offset;
}

void Camera::GetViewMatrix(glm::mat4 &OutViewMatrix) const
{
    constexpr glm::vec3 WorldUp = glm::vec3(0.f, 1.f, 0.f);
    OutViewMatrix = glm::lookAtLH(m_Position, m_OriginLookAt, WorldUp);
}

void Camera::GetProjectionMatrix(glm::mat4 &OutProjectionMatrix, const float AspectRatio)
{
    OutProjectionMatrix = glm::perspectiveLH_ZO(glm::radians(Camera::FIELD_OF_VIEW), AspectRatio, Camera::NEAR_PLANE, Camera::FAR_PLANE);
}

