
#include <gtc/quaternion.hpp>

#include "freefly_camera.h"

#include "../input_manager.h"

namespace Common
{
	FreeflyCamera::FreeflyCamera(float speed, float sensitivity) : m_speed(speed), m_sensitivity(sensitivity)
	{
	}

	FreeflyCamera::~FreeflyCamera()
	{
	}

	FreeflyCamera::FreeflyCamera(const FreeflyCamera& other) : m_speed(other.m_speed), m_sensitivity(other.m_sensitivity)
	{
		m_speed = other.m_speed;
		m_sensitivity = other.m_sensitivity;
	}

	FreeflyCamera& FreeflyCamera::operator=(const FreeflyCamera& other)
	{
		if (this == &other)
		{
			return *this;
		}

		m_speed = other.m_speed;
		m_sensitivity = other.m_sensitivity;

		return *this;
	}

	FreeflyCamera::FreeflyCamera(FreeflyCamera&& other)
	{
		m_speed = other.m_speed;
		m_sensitivity = other.m_sensitivity;
	}

	void FreeflyCamera::Update(float dt)
	{
		const InputManager& inputManager = InputManager::GetInstance();
		glm::mat4 newMatrix = glm::identity<glm::mat4>();

#pragma region MOVEMENT
		glm::vec4 globalMovementVec = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		glm::vec4 localMovementVec = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

		if (inputManager.IsKeyPressed(PHX::KeyCode::KEY_W) || inputManager.IsKeyPressed(PHX::KeyCode::KEY_O))
		{
			// Move forward (local space)
			localMovementVec.z -= m_speed * dt;
		}
		if (inputManager.IsKeyPressed(PHX::KeyCode::KEY_S) || inputManager.IsKeyPressed(PHX::KeyCode::KEY_L))
		{
			// Move backwards (local space)
			localMovementVec.z += m_speed * dt;
		}
		if (inputManager.IsKeyPressed(PHX::KeyCode::KEY_A) || inputManager.IsKeyPressed(PHX::KeyCode::KEY_K))
		{
			// Move left (global space)
			localMovementVec.x -= m_speed * dt;
		}
		if (inputManager.IsKeyPressed(PHX::KeyCode::KEY_D) || inputManager.IsKeyPressed(PHX::KeyCode::KEY_SEMICOLON))
		{
			// Move right (global space)
			localMovementVec.x += m_speed * dt;
		}
		if (inputManager.IsKeyPressed(PHX::KeyCode::KEY_SPACEBAR))
		{
			// Move up (global space)
			globalMovementVec.y -= m_speed * dt;
		}
		if (inputManager.IsKeyPressed(PHX::KeyCode::KEY_LSHIFT) || inputManager.IsKeyPressed(PHX::KeyCode::KEY_RSHIFT))
		{
			// Move downwards (global space)
			globalMovementVec.y += m_speed * dt;
		}
#pragma endregion

#pragma region ROTATION

		glm::vec3 rotationVec(0.0f);
		if (inputManager.IsMouseButtonPressed(PHX::MouseButtonCode::MOUSE_LMB))
		{
			inputManager.GetMouseDelta(rotationVec.x, rotationVec.y);
		}

		m_rotation += (rotationVec * m_sensitivity);

		constexpr float minPitch = -90.0f;
		constexpr float maxPitch = 90.0f;
		m_rotation.y = glm::clamp(m_rotation.y, minPitch, maxPitch);

		// Mod the yaw to wrap around to 0 every 360 degrees
		float yawFractional = m_rotation.x - static_cast<int>(m_rotation.x);
		m_rotation.x = static_cast<float>((static_cast<int>(m_rotation.x) % 360)) + yawFractional;

		// Rotate the camera
		glm::quat cameraOrientation = glm::quat(glm::vec3(glm::radians(m_rotation.y), glm::radians(-m_rotation.x), 0.0f));
		glm::mat4 rotationMatrix = glm::mat4_cast(cameraOrientation);
#pragma endregion

		// Calculate the final camera matrix
		// First rotate from origin (wipe old position, then replace)
		newMatrix = (rotationMatrix * newMatrix);

		// Then apply the translation
		glm::vec4 newCameraLocalPos = newMatrix * localMovementVec;
		m_position += (newCameraLocalPos + globalMovementVec);
		newMatrix[3] = glm::vec4(m_position, 1.0f);

		m_cameraMatrix = newMatrix;
		CalculateMatrix(); // Doing this every frame for now
	}
}