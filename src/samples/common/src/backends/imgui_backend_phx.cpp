#include "imgui_backend_phx.h"

#include <PHX/types/integral_types.h>

using namespace PHX;

namespace Common
{
	ImGuiPhxBackend::ImGuiPhxBackend() : m_io(nullptr), m_mouseX(0.0f), m_mouseY(0.0f)
	{
	}

	ImGuiPhxBackend::~ImGuiPhxBackend()
	{
		Shutdown();
	}

	bool ImGuiPhxBackend::Init()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		m_io = &ImGui::GetIO();

		InitializeKeyMap();

		return true;
	}

	void ImGuiPhxBackend::Shutdown()
	{
		if (m_io != nullptr)
		{
			ImGui::DestroyContext();
			m_io = nullptr;
		}
	}

	void ImGuiPhxBackend::NewFrame(float dt, u32 displayWidth, u32 displayHeight)
	{
		if (m_io == nullptr)
		{
			return;
		}

		m_io->DeltaTime = dt;
		m_io->DisplaySize = ImVec2(static_cast<float>(displayWidth), static_cast<float>(displayHeight));
		m_io->MousePos = ImVec2(m_mouseX, m_mouseY);

		ImGui::NewFrame();
	}

	void ImGuiPhxBackend::OnKeyDown(KeyCode key)
	{
		if (m_io == nullptr) return;
		ImGuiKey imguiKey = ConvertKeyCode(key);
		if (imguiKey != ImGuiKey_None)
		{
			m_io->AddKeyEvent(imguiKey, true);
		}
	}

	void ImGuiPhxBackend::OnKeyUp(KeyCode key)
	{
		if (m_io == nullptr) return;
		ImGuiKey imguiKey = ConvertKeyCode(key);
		if (imguiKey != ImGuiKey_None)
		{
			m_io->AddKeyEvent(imguiKey, false);
		}
	}

	void ImGuiPhxBackend::OnKeyRepeat(KeyCode key)
	{
		if (m_io == nullptr) return;
		ImGuiKey imguiKey = ConvertKeyCode(key);
		if (imguiKey != ImGuiKey_None)
		{
			m_io->AddKeyEvent(imguiKey, true);
		}
	}

	void ImGuiPhxBackend::OnMouseMoved(float x, float y)
	{
		m_mouseX = x;
		m_mouseY = y;
	}

	void ImGuiPhxBackend::OnMouseButtonDown(MouseButtonCode button)
	{
		if (m_io == nullptr) return;
		ImGuiMouseButton imguiButton = ConvertMouseButtonCode(button);
		if (imguiButton != ImGuiMouseButton_COUNT)
		{
			m_io->AddMouseButtonEvent(imguiButton, true);
		}
	}

	void ImGuiPhxBackend::OnMouseButtonUp(MouseButtonCode button)
	{
		if (m_io == nullptr) return;
		ImGuiMouseButton imguiButton = ConvertMouseButtonCode(button);
		if (imguiButton != ImGuiMouseButton_COUNT)
		{
			m_io->AddMouseButtonEvent(imguiButton, false);
		}
	}

	void ImGuiPhxBackend::InitializeKeyMap()
	{
		// Modern ImGui (1.87+) uses ImGuiKey enum directly via AddKeyEvent.
		// We map at event time via ConvertKeyCode, so no explicit key map setup is needed.
		// However, we must tell ImGui we support keyboard navigation and gamepad.
		m_io->BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
		m_io->BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
		m_io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	}

	ImGuiKey ImGuiPhxBackend::ConvertKeyCode(KeyCode key)
	{
		switch (key)
		{
		// Letters
		case KeyCode::KEY_A: return ImGuiKey_A;
		case KeyCode::KEY_B: return ImGuiKey_B;
		case KeyCode::KEY_C: return ImGuiKey_C;
		case KeyCode::KEY_D: return ImGuiKey_D;
		case KeyCode::KEY_E: return ImGuiKey_E;
		case KeyCode::KEY_F: return ImGuiKey_F;
		case KeyCode::KEY_G: return ImGuiKey_G;
		case KeyCode::KEY_H: return ImGuiKey_H;
		case KeyCode::KEY_I: return ImGuiKey_I;
		case KeyCode::KEY_J: return ImGuiKey_J;
		case KeyCode::KEY_K: return ImGuiKey_K;
		case KeyCode::KEY_L: return ImGuiKey_L;
		case KeyCode::KEY_M: return ImGuiKey_M;
		case KeyCode::KEY_N: return ImGuiKey_N;
		case KeyCode::KEY_O: return ImGuiKey_O;
		case KeyCode::KEY_P: return ImGuiKey_P;
		case KeyCode::KEY_Q: return ImGuiKey_Q;
		case KeyCode::KEY_R: return ImGuiKey_R;
		case KeyCode::KEY_S: return ImGuiKey_S;
		case KeyCode::KEY_T: return ImGuiKey_T;
		case KeyCode::KEY_U: return ImGuiKey_U;
		case KeyCode::KEY_V: return ImGuiKey_V;
		case KeyCode::KEY_W: return ImGuiKey_W;
		case KeyCode::KEY_X: return ImGuiKey_X;
		case KeyCode::KEY_Y: return ImGuiKey_Y;
		case KeyCode::KEY_Z: return ImGuiKey_Z;

		// Numbers
		case KeyCode::KEY_0: return ImGuiKey_0;
		case KeyCode::KEY_1: return ImGuiKey_1;
		case KeyCode::KEY_2: return ImGuiKey_2;
		case KeyCode::KEY_3: return ImGuiKey_3;
		case KeyCode::KEY_4: return ImGuiKey_4;
		case KeyCode::KEY_5: return ImGuiKey_5;
		case KeyCode::KEY_6: return ImGuiKey_6;
		case KeyCode::KEY_7: return ImGuiKey_7;
		case KeyCode::KEY_8: return ImGuiKey_8;
		case KeyCode::KEY_9: return ImGuiKey_9;

		// Utility keys
		case KeyCode::KEY_LSHIFT:   return ImGuiKey_LeftShift;
		case KeyCode::KEY_RSHIFT:   return ImGuiKey_RightShift;
		case KeyCode::KEY_LCTRL:    return ImGuiKey_LeftCtrl;
		case KeyCode::KEY_RCTRL:    return ImGuiKey_RightCtrl;
		case KeyCode::KEY_SPACEBAR: return ImGuiKey_Space;
		case KeyCode::KEY_LALT:     return ImGuiKey_LeftAlt;
		case KeyCode::KEY_RALT:     return ImGuiKey_RightAlt;
		case KeyCode::KEY_TAB:      return ImGuiKey_Tab;
		case KeyCode::KEY_CAPS:     return ImGuiKey_CapsLock;
		case KeyCode::KEY_ENTER:    return ImGuiKey_Enter;
		case KeyCode::KEY_BACKSPACE:return ImGuiKey_Backspace;
		case KeyCode::KEY_ESC:      return ImGuiKey_Escape;
		case KeyCode::KEY_HOME:     return ImGuiKey_Home;
		case KeyCode::KEY_END:      return ImGuiKey_End;
		case KeyCode::KEY_INSERT:   return ImGuiKey_Insert;
		case KeyCode::KEY_DELETE:   return ImGuiKey_Delete;
		case KeyCode::KEY_PGUP:     return ImGuiKey_PageUp;
		case KeyCode::KEY_PGDOWN:   return ImGuiKey_PageDown;
		case KeyCode::KEY_LEFT_ARROW:  return ImGuiKey_LeftArrow;
		case KeyCode::KEY_RIGHT_ARROW: return ImGuiKey_RightArrow;
		case KeyCode::KEY_UP_ARROW:    return ImGuiKey_UpArrow;
		case KeyCode::KEY_DOWN_ARROW:  return ImGuiKey_DownArrow;

		// Special characters
		case KeyCode::KEY_EQUALS:         return ImGuiKey_Equal;
		case KeyCode::KEY_LSQUAREBRACKET: return ImGuiKey_LeftBracket;
		case KeyCode::KEY_RSQUAREBRACKET: return ImGuiKey_RightBracket;
		case KeyCode::KEY_BACKSLASH:      return ImGuiKey_Backslash;
		case KeyCode::KEY_FORWARDSLASH:   return ImGuiKey_Slash;
		case KeyCode::KEY_SEMICOLON:      return ImGuiKey_Semicolon;
		case KeyCode::KEY_APOSTROPHE:     return ImGuiKey_Apostrophe;
		case KeyCode::KEY_COMMA:          return ImGuiKey_Comma;
		case KeyCode::KEY_PERIOD:         return ImGuiKey_Period;

		// Function keys
		case KeyCode::KEY_F1:  return ImGuiKey_F1;
		case KeyCode::KEY_F2:  return ImGuiKey_F2;
		case KeyCode::KEY_F3:  return ImGuiKey_F3;
		case KeyCode::KEY_F4:  return ImGuiKey_F4;
		case KeyCode::KEY_F5:  return ImGuiKey_F5;
		case KeyCode::KEY_F6:  return ImGuiKey_F6;
		case KeyCode::KEY_F7:  return ImGuiKey_F7;
		case KeyCode::KEY_F8:  return ImGuiKey_F8;
		case KeyCode::KEY_F9:  return ImGuiKey_F9;
		case KeyCode::KEY_F10: return ImGuiKey_F10;
		case KeyCode::KEY_F11: return ImGuiKey_F11;
		case KeyCode::KEY_F12: return ImGuiKey_F12;

		default: return ImGuiKey_None;
		}
	}

	ImGuiMouseButton ImGuiPhxBackend::ConvertMouseButtonCode(MouseButtonCode button)
	{
		switch (button)
		{
		case MouseButtonCode::MOUSE_LMB: return ImGuiMouseButton_Left;
		case MouseButtonCode::MOUSE_RMB: return ImGuiMouseButton_Right;
		case MouseButtonCode::MOUSE_MMB: return ImGuiMouseButton_Middle;
		default: return ImGuiMouseButton_COUNT;
		}
	}
}
