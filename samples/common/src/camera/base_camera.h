#pragma once

#include <glm.hpp>

namespace Common
{
	class BaseCamera
	{
	public:

		BaseCamera();
		virtual ~BaseCamera();
		BaseCamera(const BaseCamera& other);
		BaseCamera& operator=(const BaseCamera& other);
		BaseCamera(BaseCamera&& other);

		virtual void Update(float dt) = 0;

		glm::mat4 GetCameraMatrix() const;
		glm::mat4 GetViewMatrix() const;

		glm::vec3 GetPosition() const;
		glm::vec3 GetViewDirection() const;

	protected:

		// Builds the view matrix based on the current position and rotation values
		void CalculateMatrix();

		glm::mat4 m_cameraMatrix; // Camera world matrix (non-inverse)
		glm::vec3 m_position;
		glm::vec3 m_rotation;

	private:

		glm::mat4 m_viewMatrix; // View matrix (inverse)
	};
}