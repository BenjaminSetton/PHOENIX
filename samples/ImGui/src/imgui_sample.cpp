
#include <imgui.h>

#include "imgui_sample.h"

using namespace PHX;

#define CHECK_PHX_RES(phxRes) if(phxRes != PHX::STATUS_CODE::SUCCESS) { return; }

ImGuiSample::ImGuiSample()
{
	Init();
}

ImGuiSample::~ImGuiSample()
{
	Shutdown();
}

bool ImGuiSample::Update(float dt)
{
	bool shouldClose = BaseSample::Update(dt);

	m_imguiBackend.NewFrame(dt, m_swapChain.GetWidth(), m_swapChain.GetHeight());

	// Build ImGui UI
	ImGui::Begin("Hello, PHX!");
	ImGui::Text("This is ImGui running through PHX's abstract renderer!");
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::Text("Current frame: %u", m_renderGraph.GetFrameNumber());
	ImGui::End();

	ImGui::ShowDemoWindow();

	return shouldClose;
}

void ImGuiSample::Draw()
{
	ImGui::Render();

	m_renderGraph.BeginFrame(m_swapChain);

	m_imguiRenderer.RenderDrawData(m_renderGraph, m_swapChain, ImGui::GetDrawData());

	m_renderGraph.Bake(m_swapChain);

	// Viz
	{
		const u32 frameNumber = m_renderGraph.GetFrameNumber();
		const u32 nameLen = 64;
		char renderGraphVisName[nameLen];
		snprintf(renderGraphVisName, nameLen, "./ImGui_RG_%u.dot", frameNumber);
		m_renderGraph.GenerateVisualization(renderGraphVisName);
	}

	m_renderGraph.EndFrame();
	m_swapChain.Present();
}

void ImGuiSample::Init()
{
	m_window.SetWindowTitle("PHX %u.%u.%u | IMGUI", PHX::GetMajorVersion(), PHX::GetMinorVersion(), PHX::GetPatchVersion());

	if (!m_imguiBackend.Init())
	{
		return;
	}

	if (!m_imguiRenderer.Init(m_renderDevice, m_swapChain, m_renderGraph))
	{
		return;
	}
}

void ImGuiSample::Shutdown()
{
	m_imguiRenderer.Shutdown();
	m_imguiBackend.Shutdown();
}

void ImGuiSample::OnKeyDown(PHX::KeyCode keycode)
{
	BaseSample::OnKeyDown(keycode);
	m_imguiBackend.OnKeyDown(keycode);
}

void ImGuiSample::OnKeyUp(PHX::KeyCode keycode)
{
	BaseSample::OnKeyUp(keycode);
	m_imguiBackend.OnKeyUp(keycode);
}

void ImGuiSample::OnMouseButtonDown(PHX::MouseButtonCode mouseButton)
{
	BaseSample::OnMouseButtonDown(mouseButton);
	m_imguiBackend.OnMouseButtonDown(mouseButton);
}

void ImGuiSample::OnMouseButtonUp(PHX::MouseButtonCode mouseButton)
{
	BaseSample::OnMouseButtonUp(mouseButton);
	m_imguiBackend.OnMouseButtonUp(mouseButton);
}

void ImGuiSample::OnMouseMoved(float newX, float newY)
{
	BaseSample::OnMouseMoved(newX, newY);
	m_imguiBackend.OnMouseMoved(newX, newY);
}