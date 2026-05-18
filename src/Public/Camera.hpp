#pragma once

#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include "Commons.hpp"
#include "InputManager.hpp"

/*
 * This class encapsulates some simple camera behaviors(orbitting, etc)
 * and states, such position, up, right vectors
 * and rotation speed(in radian/sec)
 */

class Camera
{
public:
    // Constructors and destructors
    Camera();
    ~Camera() = default;

    // Actual work functions
    void Orbit(const InputResult& Input);
    void OrbitYaw(float InDirection);
    void OrbitPitch(float InDirection);
    void AdjustCameraForResize(const Commons::Layout::ViewportRect& OldViewport,
        const Commons::Layout::ViewportRect& NewViewport);

    // Setters and getters
    void SetPosition(const glm::vec3& InPosition)
    {
        m_Position = InPosition;
    }
    void SetOrbitRadius(const float InOrbitRadius)
    {
        m_OrbitRadius = InOrbitRadius;
        SetPosition(glm::vec3(0.f, 0.f, -m_OrbitRadius));
    }
    void SetRotationSpeed(const float InRotationSpeed) { m_RotationSpeedFactor = InRotationSpeed; }

    [[nodiscard]] glm::vec3 GetPosition() const { return m_Position; }
    [[nodiscard]] glm::vec3 GetUp() const { return m_Up; }
    [[nodiscard]] glm::vec3 GetRight() const { return m_Right; }
    // These two are not simple getters, they require a bit of calculations using current states
    void GetViewMatrix(glm::mat4& OutViewMatrix) const;
    void GetProjectionMatrix(glm::mat4& OutProjectionMatrix, float AspectRatio);

private:
    glm::vec3 m_Position;
    // All three are normalized
    glm::vec3 m_Up;
    glm::vec3 m_Right;
    glm::vec3 m_Forward;
    glm::vec3 m_OriginLookAt;
    glm::vec3 m_CurrentLookAt;
    glm::vec3 m_HorizontalAxis;
    glm::vec3 m_VerticalAxis;
    float m_RotationSpeedFactor = 0.f;
    float m_OrbitRadius = 0.f;
    float m_Yaw = 0.f;
    float m_Pitch = 0.25f; // Tilt the camera a bit upwards

    // Constants
    static constexpr float FIELD_OF_VIEW = 90.f;
    static constexpr float NEAR_PLANE = 0.1f;
    static constexpr float FAR_PLANE = 1000.f;
};

