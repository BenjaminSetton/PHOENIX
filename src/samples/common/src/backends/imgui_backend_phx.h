#pragma once

#include <imgui.h>

#include <BSL/integral_types.h>
#include <PHX/types/key_codes.h>
#include <PHX/types/mouse_codes.h>

namespace Common
{
	class ImGuiPhxBackend
	{
	public:

		ImGuiPhxBackend();
		~ImGuiPhxBackend();

		bool Init();
		void Shutdown();

		void NewFrame(float dt, u32 displayWidth, u32 displayHeight);

		void OnKeyDown(PHX::KeyCode key);
		void OnKeyUp(PHX::KeyCode key);
		void OnKeyRepeat(PHX::KeyCode key);
		void OnMouseMoved(float x, float y);
		void OnMouseButtonDown(PHX::MouseButtonCode button);
		void OnMouseButtonUp(PHX::MouseButtonCode button);

	private:

		void InitializeKeyMap();
		ImGuiKey ConvertKeyCode(PHX::KeyCode key);
		ImGuiMouseButton ConvertMouseButtonCode(PHX::MouseButtonCode button);

		ImGuiIO* m_io;
		float m_mouseX;
		float m_mouseY;
	};
}
