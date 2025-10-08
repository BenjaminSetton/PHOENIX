#pragma once

#include "base_camera.h"

namespace Common
{
	class FreeflyCamera : public BaseCamera
	{
	public:

		explicit FreeflyCamera(float speed, float sensitivity);
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