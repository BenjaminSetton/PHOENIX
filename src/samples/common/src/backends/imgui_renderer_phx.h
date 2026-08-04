#pragma once

#include <imgui.h>
#include <PHX/phx.h>
#include <vector>

namespace Common
{
	class ShaderManager;

	class ImGuiPhxRenderer
	{
	public:

		ImGuiPhxRenderer();
		~ImGuiPhxRenderer();

		bool Init(PHX::RenderDeviceHandle renderDevice, PHX::SwapChainHandle swapChain, ShaderManager* pShaderManager);
		void Shutdown();

		// Re-creates shaders using the given ShaderManager. Called when hot reloading is needed.
		void RecreateShaders(PHX::RenderDeviceHandle renderDevice, ShaderManager* pShaderManager);

		// Renders data provided by ImDrawData. Returns whether the function succeeded or not
		// TODO - Get rid of clearBackbuffer hack. Required because for ImGui sample initial backbuffer state is
		//        UNDEFINED, but for other samples like InstancedAnimation the backbuffer cannot be cleared because
		//        things have already been rendered to the backbuffer
		bool RenderDrawData(PHX::RenderGraphHandle renderGraph, PHX::SwapChainHandle swapChain, ImDrawData* drawData, bool clearBackbuffer);

	private:

		void CreateFontAtlas(PHX::RenderDeviceHandle renderDevice);
		void CreateShaders(PHX::RenderDeviceHandle renderDevice, ShaderManager* pShaderManager);
		void CreateUniformCollection(PHX::RenderDeviceHandle renderDevice);
		void CreatePipelineDescription(PHX::SwapChainHandle swapChain);
		void CreateBuffers(PHX::RenderDeviceHandle renderDevice);

		bool EnsureBufferSize(u64 requiredVtxSize, u64 requiredIdxSize);

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
		u64                              m_vertexBufferSize;
		u64                              m_indexBufferSize;

		bool                             m_initialized;
		bool                             m_fontAtlasUploaded;
	};
}
