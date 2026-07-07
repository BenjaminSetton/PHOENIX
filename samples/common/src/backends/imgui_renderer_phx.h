#pragma once

#include <imgui.h>
#include <PHX/phx.h>
#include <vector>

namespace Common
{
	class ImGuiPhxRenderer
	{
	public:

		ImGuiPhxRenderer();
		~ImGuiPhxRenderer();

		bool Init(PHX::RenderDeviceHandle renderDevice, PHX::SwapChainHandle swapChain);
		void Shutdown();

		// Renders data provided by ImDrawData. Returns whether the function succeeded or not
		bool RenderDrawData(PHX::RenderGraphHandle renderGraph, PHX::SwapChainHandle swapChain, ImDrawData* drawData);

	private:

		void CreateFontAtlas(PHX::RenderDeviceHandle renderDevice);
		void CreateShaders(PHX::RenderDeviceHandle renderDevice);
		void CreateUniformCollection(PHX::RenderDeviceHandle renderDevice);
		void CreatePipelineDescription(PHX::SwapChainHandle swapChain);
		void CreateBuffers(PHX::RenderDeviceHandle renderDevice);

		bool EnsureBufferSize(PHX::u64 requiredVtxSize, PHX::u64 requiredIdxSize);

		PHX::TextureHandle               m_fontTexture;
		PHX::ShaderHandle                m_vertShader;
		PHX::ShaderHandle                m_fragShader;
		PHX::UniformCollectionHandle     m_uniformCollection;
		PHX::BufferHandle                m_transformBuffer;

		PHX::GraphicsPipelineDesc        m_pipelineDesc;
		std::vector<PHX::InputAttribute> m_inputAttributes;
		std::vector<PHX::ShaderHandle>   m_shaders;

		PHX::BufferHandle                m_vertexBuffer;
		PHX::BufferHandle                m_indexBuffer;
		PHX::u64                         m_vertexBufferSize;
		PHX::u64                         m_indexBufferSize;

		bool                             m_initialized;
		bool                             m_fontAtlasUploaded;
	};
}
