#pragma once

#include "../../common/src/base_sample.h"
#include "../../common/src/backends/imgui_backend_phx.h"
#include "../../common/src/backends/imgui_renderer_phx.h"

class ImGuiSample : public Common::BaseSample
{
public:

	ImGuiSample();
	~ImGuiSample() override;

	bool Update(float dt) override;
	void Draw() override;

private:

	void Init() override;
	void Shutdown() override;

	void OnKeyDown(PHX::KeyCode keycode) override;
	void OnKeyUp(PHX::KeyCode keycode) override;
	void OnMouseButtonDown(PHX::MouseButtonCode mouseButton) override;
	void OnMouseButtonUp(PHX::MouseButtonCode mouseButton) override;
	void OnMouseMoved(float newX, float newY) override;

	Common::ImGuiPhxBackend m_imguiBackend;
	Common::ImGuiPhxRenderer m_imguiRenderer;
};