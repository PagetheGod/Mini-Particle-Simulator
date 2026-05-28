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
    void Zoom(const InputResult& Input);
    void AdjustCameraForResize(const Commons::Layout::ViewportRect& OldViewport,
        const Commons::Layout::ViewportRect& NewViewport);

    // Setters and getters
    void SetPosition(const glm::vec3& InPosition)
    {
        m_Position = InPosition;
    }
    void SetOrbitRadius(const float InOrbitRadius);
    void SetRotationSpeed(const float InRotationSpeed) { m_RotationSpeedFactor = InRotationSpeed; }

    [[nodiscard]] glm::vec3 GetPosition() const { return m_Position; }
    [[nodiscard]] glm::vec3 GetUp() const { return m_Up; }
    [[nodiscard]] glm::vec3 GetRight() const { return m_Right; }
    // These two are not simple getters, they require a bit of calculations using current states
    void GetViewMatrix(glm::mat4& OutViewMatrix) const;
    void GetProjectionMatrix(glm::mat4& OutProjectionMatrix, float AspectRatio);
private:
    // Helper to recompute positions and local basis when needed
    void RecomputePosAndBasis();

private:
    float m_OrbitRadius = 350.f;
    glm::vec3 m_Position;
    // All three are normalized
    glm::vec3 m_Up;
    glm::vec3 m_Right;
    glm::vec3 m_Forward;
    // This is where our camera is POINTING AT, it's not the origin of the world
    // In fact it's shifted a bit upwards so we can view all of the particles easily
    glm::vec3 m_LookAt;
    float m_RotationSpeedFactor = 0.5f;
    float m_ZoomSpeedFactor = 0.25f;
    float m_Yaw = 180.f;
    float m_Pitch = 0.f;
    float m_ZoomY = 1.f;
    float m_ZoomX = 1.f;
    // Constants, we have different zoom limits for software and hardware renderer due to how they
    // render the actual sprites
    static constexpr float FIELD_OF_VIEW = 120.f;
    static constexpr float NEAR_PLANE = 0.1f;
    static constexpr float FAR_PLANE = 1000.f;
    static constexpr float MIN_ZOOM_Y = 0.8f;
    static constexpr float MAX_ZOOM_Y = 2.5f;
    static constexpr float MIN_ZOOM_X = MIN_ZOOM_Y / Commons::Layout::ASPECT_RATIO;
    static constexpr float MAX_ZOOM_X = MAX_ZOOM_Y / Commons::Layout::ASPECT_RATIO;
};

