
#include <imgui.h>

#include "imgui_sample.h"

using namespace PHX;

ImGuiSample::ImGuiSample()
{
}

ImGuiSample::~ImGuiSample()
{
}

void ImGuiSample::UpdateSample(float dt)
{
	m_imguiBackend.NewFrame(dt, m_swapChain.GetWidth(), m_swapChain.GetHeight());

	// Build ImGui UI
	ImGui::Begin("Hello, PHX!");
	ImGui::Text("This is ImGui running through PHX's abstract renderer!");
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::Text("Current frame: %u", m_renderGraph.GetFrameNumber());
	ImGui::End();

	const PHX::Metrics& metrics = m_renderGraph.GetMetrics();
	ImGui::Begin("Metrics");
	ImGui::Text("Draw calls: %u", metrics.drawCalls);
	ImGui::Text("Vertex count: %u", metrics.vertices);
	ImGui::Text("Index count: %u", metrics.indices);
	ImGui::Text("Triangle count: %u", metrics.triangles);
	ImGui::Text("Pass count: %u", metrics.passCount);
	ImGui::Text("");
	ImGui::Text("Buffer count: %u", metrics.bufferCount);
	ImGui::Text("Texture count: %u", metrics.textureCount);
	ImGui::Text("Shader count: %u", metrics.shaderCount);
	ImGui::Text("Pipeline count: %u", metrics.pipelineCount);
	ImGui::Text("Uniform collection count: %u", metrics.uniformCollectionCount);
	ImGui::Text("Accel struct count: %u", metrics.accelerationStructureCount);
	ImGui::Text("");
	ImGui::Text("Allocated memory (bytes): %u", metrics.allocatedMemoryBytes);
	ImGui::Text("GPU frametime: %2.3f (milliseconds)", metrics.gpuFrameTime);
	ImGui::End();

	ImGui::ShowDemoWindow();
}

void ImGuiSample::Draw()
{
	m_renderGraph.BeginFrame(m_swapChain);

	ImGui::Render();
	m_imguiRenderer.RenderDrawData(m_renderGraph, m_swapChain, ImGui::GetDrawData(), true);

	m_renderGraph.Bake(m_swapChain);

	// Viz
	{
		const u32 frameNumber = m_renderGraph.GetFrameNumber();
		const u32 nameLen = 64;
		char renderGraphVisName[nameLen];
		snprintf(renderGraphVisName, nameLen, "./ImGui_RG_%u.dot", frameNumber);
		m_renderGraph.GenerateVisualization(renderGraphVisName);
	}

	m_renderGraph.EndFrame(m_swapChain);
}

void ImGuiSample::InitSample()
{
	m_window.SetWindowTitle("PHX %u.%u.%u | IMGUI", PHX::GetMajorVersion(), PHX::GetMinorVersion(), PHX::GetPatchVersion());
}

void ImGuiSample::ShutdownSample()
{
}
