#pragma once

#include "base_camera.h"

namespace Common
{
	class FreeflyCamera : public BaseCamera
	{
	public:

		FreeflyCamera();
		FreeflyCamera(float speed, float sensitivity, glm::vec3 pos, glm::vec3 rot);
		~FreeflyCamera() override;
		FreeflyCamera(const FreeflyCamera& other);
		FreeflyCamera& operator=(const FreeflyCamera& other);
		FreeflyCamera(FreeflyCamera&& other);

		void Update(float dt) override;

	private:

		float m_speed;
		float m_sensitivity;
	};
}