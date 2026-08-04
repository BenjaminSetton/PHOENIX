#pragma once

#include <bitset>
#include <utility>

#include <PHX/types/key_codes.h>
#include <PHX/types/mouse_codes.h>

namespace Common
{
	class InputManager
	{
		friend class BaseSample;

	public:

		static InputManager& GetInstance()
		{
			static InputManager s_instance;
			return s_instance;
		}

		~InputManager();
		InputManager(const InputManager& other) = delete;
		InputManager& operator=(const InputManager& other) = delete;
		InputManager(InputManager&& other) = delete;

		void Update();

		bool IsKeyPressed(PHX::KeyCode keycode) const;
		bool IsKeyReleased(PHX::KeyCode keycode) const;

		bool IsMouseButtonPressed(PHX::MouseButtonCode mouseButton) const;
		bool IsMouseButtonReleased(PHX::MouseButtonCode mouseButton) const;

		void GetMousePosition(float& out_X, float& out_Y) const;
		void GetMouseDelta(float& out_X, float& out_Y) const;

	private:

		InputManager();

		void SetKeyCode(PHX::KeyCode keycode, bool value);
		void SetMousePosition(float newX, float newY);
		void SetMouseButton(PHX::MouseButtonCode mouseButton, bool value);

	private:

		std::bitset<static_cast<size_t>(PHX::KeyCode::COUNT)> m_keyValues;
		std::bitset<static_cast<size_t>(PHX::MouseButtonCode::COUNT)> m_mouseButtonValues;

		std::pair<float, float> m_prevMousePosition;
		std::pair<float, float> m_newMousePosition;
		std::pair<float, float> m_mouseDelta;
		bool m_isMousePositionInitialized;
	};
}