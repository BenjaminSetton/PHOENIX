
#include <array>
#include <cstring>
#include <vector>

#include "imgui_renderer_phx.h"

#include "../utils/shader_utils.h"



using namespace PHX;

namespace Common
{
	static constexpr u64 INITIAL_VERTEX_BUFFER_SIZE = 65536 * sizeof(ImDrawVert);
	static constexpr u64 INITIAL_INDEX_BUFFER_SIZE  = 65536 * sizeof(ImDrawIdx);

	ImGuiPhxRenderer::ImGuiPhxRenderer() : m_fontTexture(), m_vertShader(), m_fragShader(),
		m_uniformCollection(), m_transformBuffer(), m_pipelineDesc(), m_vertexBuffer(), m_indexBuffer(), m_vertexBufferSize(0),
		m_indexBufferSize(0), m_initialized(false), m_fontAtlasUploaded(false)
	{
	}

	ImGuiPhxRenderer::~ImGuiPhxRenderer()
	{
		Shutdown();
	}

	bool ImGuiPhxRenderer::Init(RenderDeviceHandle renderDevice, SwapChainHandle swapChain, RenderGraphHandle renderGraph)
	{
		CreateFontAtlas(renderDevice);
		CreateShaders(renderDevice);
		CreateUniformCollection(renderDevice);
		CreatePipelineDescription(swapChain);
		CreateBuffers(renderDevice);

		m_initialized = true;
		return true;
	}

	void ImGuiPhxRenderer::Shutdown()
	{
		m_shaders.clear();
		m_inputAttributes.clear();
		m_initialized = false;
	}

	void ImGuiPhxRenderer::CreateFontAtlas(RenderDeviceHandle renderDevice)
	{
		ImGuiIO& io = ImGui::GetIO();

		u8* pixels = nullptr;
		int width = 0;
		int height = 0;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

		TextureBaseCreateInfo baseCI{};
		baseCI.pName = "ImGuiFontAtlas";
		baseCI.width = static_cast<u32>(width);
		baseCI.height = static_cast<u32>(height);
		baseCI.arrayLayers = 1;
		baseCI.generateMips = false;
		baseCI.format = BASE_FORMAT::R8G8B8A8_SRGB;
		baseCI.usageFlags = USAGE_TYPE_FLAG_SAMPLED | USAGE_TYPE_FLAG_TRANSFER_DST;

		TextureViewCreateInfo viewCI{};
		viewCI.type = VIEW_TYPE::TYPE_2D;
		viewCI.scope = VIEW_SCOPE::ENTIRE;
		viewCI.aspectFlags = ASPECT_TYPE_FLAG_COLOR;

		TextureSamplerCreateInfo samplerCI{};
		samplerCI.addressModeUVW = SAMPLER_ADDRESS_MODE::REPEAT;
		samplerCI.enableAnisotropicFiltering = false;
		samplerCI.magnificationFilter = FILTER_MODE::LINEAR;
		samplerCI.minificationFilter = FILTER_MODE::LINEAR;
		samplerCI.samplerMipMapFilter = FILTER_MODE::LINEAR;

		STATUS_CODE res = renderDevice.AllocateTexture(baseCI, viewCI, samplerCI, m_fontTexture);
		if (res != STATUS_CODE::SUCCESS)
		{
			return;
		}

		// Store font texture ID in ImGui using the handle's index as a unique identifier
		io.Fonts->SetTexID(static_cast<ImTextureID>(static_cast<uint64_t>(m_fontTexture.GetIndex())));
	}

	void ImGuiPhxRenderer::CreateShaders(RenderDeviceHandle renderDevice)
	{
		if (!Common::AllocateShader("../src/shaders/imgui.vert", SHADER_STAGE::VERTEX, renderDevice, m_vertShader))
		{
			return;
		}

		if (!Common::AllocateShader("../src/shaders/imgui.frag", SHADER_STAGE::FRAGMENT, renderDevice, m_fragShader))
		{
			return;
		}

		m_shaders.push_back(m_vertShader);
		m_shaders.push_back(m_fragShader);
	}

	void ImGuiPhxRenderer::CreateUniformCollection(RenderDeviceHandle renderDevice)
	{
		// SET 0: Transform uniform buffer (scale + translation)
		UniformData transformUniformData;
		transformUniformData.binding = 0;
		transformUniformData.shaderStage = SHADER_STAGE::VERTEX;
		transformUniformData.type = UNIFORM_TYPE::UNIFORM_BUFFER;

		UniformDataGroup transformGroup;
		transformGroup.set = 0;
		transformGroup.uniformArray = &transformUniformData;
		transformGroup.uniformArrayCount = 1;

		// SET 1: Font atlas combined image sampler
		UniformData fontUniformData;
		fontUniformData.binding = 0;
		fontUniformData.shaderStage = SHADER_STAGE::FRAGMENT;
		fontUniformData.type = UNIFORM_TYPE::COMBINED_IMAGE_SAMPLER;

		UniformDataGroup fontGroup;
		fontGroup.set = 1;
		fontGroup.uniformArray = &fontUniformData;
		fontGroup.uniformArrayCount = 1;

		std::array<UniformDataGroup, 2> dataGroups =
		{
			transformGroup,
			fontGroup
		};

		UniformCollectionCreateInfo uniformCI{};
		uniformCI.dataGroups = dataGroups.data();
		uniformCI.groupCount = static_cast<u32>(dataGroups.size());

		STATUS_CODE res = renderDevice.AllocateUniformCollection(uniformCI, m_uniformCollection);
		if (res != STATUS_CODE::SUCCESS)
		{
			return;
		}

		// Create transform uniform buffer (scale + translation = 2x Vec2f = 16 bytes)
		BufferCreateInfo transformBufferCI{};
		transformBufferCI.pName = "ImGuiTransformBuffer";
		transformBufferCI.bufferUsage = BUFFER_USAGE::UNIFORM_BUFFER;
		transformBufferCI.sizeBytes = sizeof(float) * 4;

		res = renderDevice.AllocateBuffer(transformBufferCI, m_transformBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			return;
		}
	}

	void ImGuiPhxRenderer::CreatePipelineDescription(SwapChainHandle swapChain)
	{
		// Input attributes for ImGui vertex format (ImDrawVert: pos, uv, col)
		m_inputAttributes =
		{
			// Position (2x float32, offset 0)
			{ 0, 0, BASE_FORMAT::R32G32_FLOAT },
			// UV (2x float32, offset 8)
			{ 1, 0, BASE_FORMAT::R32G32_FLOAT },
			// Color (4x uint8 unorm, offset 16)
			{ 2, 0, BASE_FORMAT::R8G8B8A8_UNORM },
		};

		m_pipelineDesc = {};
		m_pipelineDesc.topology = PRIMITIVE_TOPOLOGY::TRIANGLE_LIST;
		m_pipelineDesc.polygonMode = POLYGON_MODE::FILL;
		m_pipelineDesc.cullMode = CULL_MODE::NONE;
		m_pipelineDesc.enableDepthTest = false;
		m_pipelineDesc.enableDepthWrite = false;

		m_pipelineDesc.blendState.enableBlend = true;
		m_pipelineDesc.blendState.srcColorFactor = BLEND_FACTOR::SRC_ALPHA;
		m_pipelineDesc.blendState.dstColorFactor = BLEND_FACTOR::ONE_MINUS_SRC_ALPHA;
		m_pipelineDesc.blendState.colorBlendOp = BLEND_OP::ADD;
		m_pipelineDesc.blendState.srcAlphaFactor = BLEND_FACTOR::ONE;
		m_pipelineDesc.blendState.dstAlphaFactor = BLEND_FACTOR::ONE_MINUS_SRC_ALPHA;
		m_pipelineDesc.blendState.alphaBlendOp = BLEND_OP::ADD;

		m_pipelineDesc.viewportSize = { swapChain.GetWidth(), swapChain.GetHeight() };
		m_pipelineDesc.viewportPos = { 0, 0 };
		m_pipelineDesc.scissorOffset = { 0, 0 };
		m_pipelineDesc.scissorExtent = { swapChain.GetWidth(), swapChain.GetHeight() };

		m_pipelineDesc.pShaders = m_shaders.data();
		m_pipelineDesc.shaderCount = static_cast<u32>(m_shaders.size());
		m_pipelineDesc.pInputAttributes = m_inputAttributes.data();
		m_pipelineDesc.attributeCount = static_cast<u32>(m_inputAttributes.size());
		m_pipelineDesc.inputBinding = 0;
		m_pipelineDesc.inputRate = VERTEX_INPUT_RATE::PER_VERTEX;
		m_pipelineDesc.uniformCollection = m_uniformCollection;
	}

	void ImGuiPhxRenderer::CreateBuffers(RenderDeviceHandle renderDevice)
	{
		BufferCreateInfo vtxBufferCI{};
		vtxBufferCI.pName = "ImGuiVertexBuffer";
		vtxBufferCI.bufferUsage = BUFFER_USAGE::VERTEX_BUFFER;
		vtxBufferCI.sizeBytes = INITIAL_VERTEX_BUFFER_SIZE;

		STATUS_CODE res = renderDevice.AllocateBuffer(vtxBufferCI, m_vertexBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			return;
		}
		m_vertexBufferSize = INITIAL_VERTEX_BUFFER_SIZE;

		BufferCreateInfo idxBufferCI{};
		idxBufferCI.pName = "ImGuiIndexBuffer";
		idxBufferCI.bufferUsage = BUFFER_USAGE::INDEX_BUFFER;
		idxBufferCI.sizeBytes = INITIAL_INDEX_BUFFER_SIZE;

		res = renderDevice.AllocateBuffer(idxBufferCI, m_indexBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			return;
		}
		m_indexBufferSize = INITIAL_INDEX_BUFFER_SIZE;
	}

	bool ImGuiPhxRenderer::EnsureBufferSize(u64 requiredVtxSize, u64 requiredIdxSize)
	{
		// TODO: Implement buffer resize if needed. For now, the initial buffers are large enough
		// for most ImGui usage. If buffers need to grow, they should be destroyed and recreated.
		(void)requiredVtxSize;
		(void)requiredIdxSize;
		return true;
	}

	bool ImGuiPhxRenderer::RenderDrawData(RenderGraphHandle renderGraph, SwapChainHandle swapChain, ImDrawData* drawData)
	{
		if (!m_initialized)
		{
			return false;
		}

		// When there is no draw data, register a clear-only pass so the swapchain image is transitioned to present layout
		if (drawData == nullptr || drawData->TotalVtxCount == 0)
		{
			RenderPassHandle clearPass;
			STATUS_CODE res = renderGraph.RegisterPass("ImGuiClearPass", BIND_POINT::GRAPHICS, clearPass);
			if (res != STATUS_CODE::SUCCESS)
			{
				return false;
			}

			ClearValues clearVals{};
			clearPass.SetTextureOutput(swapChain.GetCurrentImage(), ATTACHMENT_LOAD_OP::CLEAR, ATTACHMENT_STORE_OP::STORE, clearVals);
			return true;
		}

		u64 totalVtxSize = static_cast<u64>(drawData->TotalVtxCount) * sizeof(ImDrawVert);
		u64 totalIdxSize = static_cast<u64>(drawData->TotalIdxCount) * sizeof(ImDrawIdx);
		EnsureBufferSize(totalVtxSize, totalIdxSize);

		// Update transform UBO
		float scale[2] =
		{
			2.0f / drawData->DisplaySize.x,
			2.0f / drawData->DisplaySize.y
		};
		float translation[2] =
		{
			-1.0f - drawData->DisplayPos.x * scale[0],
			-1.0f - drawData->DisplayPos.y * scale[1]
		};
		float transformData[4] = { scale[0], scale[1], translation[0], translation[1] };

		// Register transfer pass to upload vertex/index data (vkCmdCopyBuffer cannot be called inside a render pass)
		RenderPassHandle transferPass;
		STATUS_CODE res = renderGraph.RegisterPass("ImGuiDataUpload", BIND_POINT::TRANSFER, transferPass);
		if (res != STATUS_CODE::SUCCESS)
		{
			return false;
		}

		transferPass.SetBufferOutput(m_vertexBuffer);
		transferPass.SetBufferOutput(m_indexBuffer);

		transferPass.SetExecuteCallback([&, drawData, this](DeviceContextHandle deviceContext)
		{
			// Upload font atlas on the first frame
			if (!m_fontAtlasUploaded)
			{
				ImGuiIO& io = ImGui::GetIO();
				u8* pixels = nullptr;
				int width = 0;
				int height = 0;
				io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
				u64 uploadSize = static_cast<u64>(width) * static_cast<u64>(height) * 4;
				deviceContext.CopyDataToTexture(m_fontTexture, pixels, uploadSize);
				m_fontAtlasUploaded = true;
			}

			std::vector<ImDrawVert> allVertices;
			allVertices.reserve(static_cast<size_t>(drawData->TotalVtxCount));
			std::vector<ImDrawIdx> allIndices;
			allIndices.reserve(static_cast<size_t>(drawData->TotalIdxCount));

			for (int n = 0; n < drawData->CmdListsCount; n++)
			{
				const ImDrawList* cmdList = drawData->CmdLists[n];
				allVertices.insert(allVertices.end(), cmdList->VtxBuffer.begin(), cmdList->VtxBuffer.end());
				allIndices.insert(allIndices.end(), cmdList->IdxBuffer.begin(), cmdList->IdxBuffer.end());
			}

			deviceContext.CopyDataToBuffer(m_vertexBuffer, allVertices.data(), allVertices.size() * sizeof(ImDrawVert));
			deviceContext.CopyDataToBuffer(m_indexBuffer, allIndices.data(), allIndices.size() * sizeof(ImDrawIdx));
		});

		// Register ImGui graphics pass
		RenderPassHandle renderPass;
		res = renderGraph.RegisterPass("ImGuiPass", BIND_POINT::GRAPHICS, renderPass);
		if (res != STATUS_CODE::SUCCESS)
		{
			return false;
		}

		ClearValues clearVals{};
		renderPass.SetTextureOutput(swapChain.GetCurrentImage(), ATTACHMENT_LOAD_OP::CLEAR, ATTACHMENT_STORE_OP::STORE, clearVals);

		renderPass.SetBufferInput(m_vertexBuffer);
		renderPass.SetBufferInput(m_indexBuffer);
		renderPass.SetTextureInput(m_fontTexture);

		renderPass.SetPipelineDescription(m_pipelineDesc);

		renderPass.SetExecuteCallback([&, transformData, drawData, swapChain](DeviceContextHandle deviceContext)
		{
			deviceContext.CopyDataToBuffer(m_transformBuffer, transformData, sizeof(transformData));
			m_uniformCollection.QueueBufferUpdate(m_transformBuffer, 0, 0, 0);
			m_uniformCollection.QueueImageUpdate(m_fontTexture, 1, 0, 0);
			m_uniformCollection.FlushUpdateQueue();

			deviceContext.BindUniformCollection(m_uniformCollection);
			deviceContext.SetViewport({ swapChain.GetWidth(), swapChain.GetHeight() }, { 0, 0 });

			INDEX_TYPE indexType = (sizeof(ImDrawIdx) == 2) ? INDEX_TYPE::U16 : INDEX_TYPE::U32;

			u32 globalVertexOffset = 0;
			u32 globalIndexOffset = 0;

			for (int n = 0; n < drawData->CmdListsCount; n++)
			{
				const ImDrawList* cmdList = drawData->CmdLists[n];

				deviceContext.BindMesh(m_vertexBuffer, m_indexBuffer, indexType);

				u32 indexOffset = globalIndexOffset;
				for (int cmdIdx = 0; cmdIdx < cmdList->CmdBuffer.Size; cmdIdx++)
				{
					const ImDrawCmd& cmd = cmdList->CmdBuffer[cmdIdx];

					ImVec2 clipMin = ImVec2(cmd.ClipRect.x - drawData->DisplayPos.x, cmd.ClipRect.y - drawData->DisplayPos.y);
					ImVec2 clipMax = ImVec2(cmd.ClipRect.z - drawData->DisplayPos.x, cmd.ClipRect.w - drawData->DisplayPos.y);

					if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
					{
						continue;
					}

					u32 scissorX = static_cast<u32>(clipMin.x);
					u32 scissorY = static_cast<u32>(clipMin.y);
					u32 scissorW = static_cast<u32>(clipMax.x - clipMin.x);
					u32 scissorH = static_cast<u32>(clipMax.y - clipMin.y);

					deviceContext.SetScissor({ scissorW, scissorH }, { scissorX, scissorY });

					deviceContext.DrawIndexed(static_cast<u32>(cmd.ElemCount), indexOffset, globalVertexOffset);

					indexOffset += static_cast<u32>(cmd.ElemCount);
				}

				globalVertexOffset += static_cast<u32>(cmdList->VtxBuffer.Size);
				globalIndexOffset += static_cast<u32>(cmdList->IdxBuffer.Size);
			}
		});

		return true;
	}
}
