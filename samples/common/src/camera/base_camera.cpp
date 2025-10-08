
#include <gtc/matrix_transform.hpp>

#include "base_camera.h"

namespace Common
{
	BaseCamera::BaseCamera() : m_cameraMatrix(glm::identity<glm::mat4>()), m_position(glm::vec3(0.0f)), 
		m_rotation(glm::vec3(0.0f)), m_viewMatrix()
	{
		CalculateMatrix();
	}

	BaseCamera::~BaseCamera()
	{
	}

	BaseCamera::BaseCamera(const BaseCamera& other) : m_cameraMatrix(other.m_cameraMatrix), m_position(other.m_position), 
		m_rotation(other.m_rotation), m_viewMatrix(other.m_viewMatrix)
	{
	}

	BaseCamera& BaseCamera::operator=(const BaseCamera& other)
	{
		if (this == &other)
		{
			return *this;
		}

		m_cameraMatrix = other.m_cameraMatrix;
		m_position = other.m_position;
		m_rotation = other.m_rotation;
		m_viewMatrix = other.m_viewMatrix;

		return *this;
	}

	BaseCamera::BaseCamera(BaseCamera&& other)
	{
		m_cameraMatrix = other.m_cameraMatrix;
		m_position = other.m_position;
		m_rotation = other.m_rotation;
		m_viewMatrix = other.m_viewMatrix;
	}

	glm::mat4 BaseCamera::GetCameraMatrix() const
	{
		return m_cameraMatrix;
	}

	glm::mat4 BaseCamera::GetViewMatrix() const 
	{
		// Return the inverse transform, aka the view matrix
		return m_viewMatrix;
	}

	glm::vec3 BaseCamera::GetPosition() const
	{
		// Return world position
		return glm::vec3(m_cameraMatrix[3]);
	}

	glm::vec3 BaseCamera::GetViewDirection() const
	{
		// Return Z axis
		return glm::vec3(m_cameraMatrix[2]);
	}

	void BaseCamera::CalculateMatrix()
	{
		m_viewMatrix = glm::inverse(m_cameraMatrix);
	}
}