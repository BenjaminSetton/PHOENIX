
#include "input_manager.h"

namespace Common
{
	InputManager::InputManager() : m_keyValues(0), m_prevMousePosition({0.0f, 0.0f}), m_newMousePosition({0.0f, 0.0f}), 
		m_mouseDelta({0.0f, 0.0f}), m_isMousePositionInitialized(false)
	{
	}

	InputManager::~InputManager()
	{
	}

	void InputManager::Update()
	{
		m_mouseDelta.first = (m_newMousePosition.first - m_prevMousePosition.first);
		m_mouseDelta.second = (m_newMousePosition.second - m_prevMousePosition.second);

		m_prevMousePosition = m_newMousePosition;
	}

	bool InputManager::IsKeyPressed(PHX::KeyCode keycode) const
	{
		return m_keyValues.test(static_cast<size_t>(keycode));
	}

	bool InputManager::IsKeyReleased(PHX::KeyCode keycode) const
	{
		return !IsKeyPressed(keycode);
	}

	bool InputManager::IsMouseButtonPressed(PHX::MouseButtonCode mouseButton) const
	{
		return m_mouseButtonValues.test(static_cast<size_t>(mouseButton));
	}

	bool InputManager::IsMouseButtonReleased(PHX::MouseButtonCode mouseButton) const
	{
		return !IsMouseButtonPressed(mouseButton);
	}

	void InputManager::GetMousePosition(float& out_X, float& out_Y) const
	{
		out_X = m_newMousePosition.first;
		out_Y = m_newMousePosition.second;
	}

	void InputManager::GetMouseDelta(float& out_X, float& out_Y) const
	{
		out_X = m_mouseDelta.first;
		out_Y = m_mouseDelta.second;
	}

	void InputManager::SetKeyCode(PHX::KeyCode keycode, bool value)
	{
		m_keyValues.set(static_cast<size_t>(keycode), value);
	}

	void InputManager::SetMousePosition(float newX, float newY)
	{
		// Avoid setting huge delta on first frame mouse moves
		if (!m_isMousePositionInitialized)
		{
			m_prevMousePosition = { newX, newY };
		}

		m_newMousePosition = { newX, newY };
		m_isMousePositionInitialized = true;
	}

	void InputManager::SetMouseButton(PHX::MouseButtonCode mouseButton, bool value)
	{
		m_mouseButtonValues.set(static_cast<size_t>(mouseButton), value);
	}
}