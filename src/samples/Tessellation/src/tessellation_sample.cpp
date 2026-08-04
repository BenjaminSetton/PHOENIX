
#include <array>
#include <cmath>
#include <gtc/matrix_transform.hpp>
#include <imgui.h>

#include "tessellation_sample.h"

#include "../../common/src/camera/freefly_camera.h"
#include "../../common/src/utils/shader_utils.h"

using namespace PHX;

#define CHECK_PHX_RES(phxRes) if(phxRes != PHX::STATUS_CODE::SUCCESS) { return; }

TessellationSample::TessellationSample()
	: m_uniformCollection()
	, m_vertexBuffer()
	, m_cameraBuffer()
	, m_tessParamsBuffer()
	, m_depthBuffer()
	, m_gridWidth(64)
	, m_gridHeight(64)
	, m_cellSize(1.25f)
	, m_patchCount(0)
	, m_vertexCount(0)
	, m_gridDirty(true)
	, m_maxTessFactor(16.0f)
	, m_minDistance(5.0f)
	, m_maxDistance(80.0f)
	, m_displacementScale(2.0f)
	, m_wireframe(false)
	, m_lastFrameTime(0.0f)
{
}

TessellationSample::~TessellationSample()
{
}

void TessellationSample::UpdateSample(float dt)
{
	m_lastFrameTime = dt;

	// Update camera view matrix (transposed for row-major Slang)
	m_cameraData.view = glm::transpose(m_pCamera->GetViewMatrix());
	m_cameraData.camPos = m_pCamera->GetPosition();

	// ImGui
	m_imguiBackend.NewFrame(dt, m_swapChain.GetWidth(), m_swapChain.GetHeight());
	BuildImGuiUI();
}

void TessellationSample::Draw()
{
	STATUS_CODE phxRes;

	ClearValues clearColor{};
	clearColor.color.color = BSL::Vec4f(0.1f, 0.1f, 0.15f, 1.0f);
	clearColor.useClearColor = true;

	m_renderGraph.BeginFrame(m_swapChain);

	// Upload grid vertex data (only when grid params changed)
	const bool needGridUpload = m_gridDirty;
	if (needGridUpload)
	{
		RenderPassHandle transferPass;
		phxRes = m_renderGraph.RegisterPass("GridDataUpload", PASS_TYPE::TRANSFER, transferPass);
		CHECK_PHX_RES(phxRes);

		transferPass.SetBufferOutput(m_vertexBuffer);
		transferPass.SetExecuteCallback([this](DeviceContextHandle deviceContext)
		{
			deviceContext.CopyDataToBuffer(m_vertexBuffer, m_gridVertices.data(), m_vertexCount * sizeof(GridVertex));
		});

		m_gridDirty = false;
	}

	// Graphics pass — render tessellated grid
	{
		RenderPassHandle graphicsPass;
		phxRes = m_renderGraph.RegisterPass("TessellatedGrid", PASS_TYPE::GRAPHICS, graphicsPass);
		CHECK_PHX_RES(phxRes);

		graphicsPass.SetTextureOutput(m_swapChain.GetCurrentImage(), ATTACHMENT_LOAD_OP::CLEAR, ATTACHMENT_STORE_OP::STORE, clearColor);
		graphicsPass.SetDepthOutput(m_depthBuffer);
		graphicsPass.SetBufferInput(m_vertexBuffer);

		graphicsPass.SetPipelineDescription(m_wireframe ? m_wireframePipelineDesc : m_solidPipelineDesc);

		const u32 vertexCount = m_vertexCount;

		graphicsPass.SetExecuteCallback([&, vertexCount](DeviceContextHandle deviceContext)
		{
			// Update camera UBO
			deviceContext.CopyDataToBuffer(m_cameraBuffer, &m_cameraData, sizeof(CameraUBO));

			// Update tess params UBO
			TessParams params{};
			params.maxTessFactor = m_maxTessFactor;
			params.minDistance = m_minDistance;
			params.maxDistance = m_maxDistance;
			params.displacementScale = m_displacementScale;
			deviceContext.CopyDataToBuffer(m_tessParamsBuffer, &params, sizeof(TessParams));

			m_uniformCollection.QueueBufferUpdate(m_cameraBuffer, 0, 0, 0);
			m_uniformCollection.QueueBufferUpdate(m_tessParamsBuffer, 0, 1, 0);
			deviceContext.FlushUniformUpdates(m_uniformCollection);

			deviceContext.BindUniformCollection(m_uniformCollection);
			deviceContext.BindVertexBuffer(m_vertexBuffer);
			deviceContext.SetScissor({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
			deviceContext.SetViewport({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
			deviceContext.Draw(vertexCount);
		});
	}

	// ImGui pass
	ImGui::Render();
	m_imguiRenderer.RenderDrawData(m_renderGraph, m_swapChain, ImGui::GetDrawData(), false);

	m_renderGraph.Bake(m_swapChain);

	// Viz
	GenerateRenderGraphVisualization("Tessellation");

	m_renderGraph.EndFrame(m_swapChain);
}

void TessellationSample::InitSample()
{
	STATUS_CODE phxRes;

	m_window.SetWindowTitle("PHX %u.%u.%u | TESSELLATION", PHX::GetMajorVersion(), PHX::GetMinorVersion(), PHX::GetPatchVersion());

	// CAMERA
	const float cameraSpeed = 20.0f;
	const float cameraSensitivity = 0.2f;
	// Position camera above and behind the grid (at +Z), looking toward -Z and down at the grid
	m_pCamera = new Common::FreeflyCamera(cameraSpeed, cameraSensitivity, glm::vec3(0.0f, 25.0f, 40.0f), glm::vec3(-25.0f, 0.0f, 0.0f));

	const float fov = 45.0f;
	const float aspectRatio = static_cast<float>(m_window.GetCurrentWidth()) / m_window.GetCurrentHeight();
	m_cameraData.view = glm::transpose(m_pCamera->GetViewMatrix());
	m_cameraData.proj = glm::transpose(glm::perspective(glm::radians(fov), aspectRatio, 0.1f, 1000.0f));
	m_cameraData.proj[1][1] *= -1.0f; // Vulkan Y-flip
	m_cameraData.camPos = m_pCamera->GetPosition();

	// SHADERS
	ShaderHandle vertShader = m_pShaderManager->RegisterShader("../src/shaders/grid.vert.slang", SHADER_STAGE::VERTEX, m_renderDevice);
	if (!vertShader.IsValid()) return;
	ShaderHandle tescShader = m_pShaderManager->RegisterShader("../src/shaders/grid.tesc.slang", SHADER_STAGE::TESSELLATION_CONTROL, m_renderDevice);
	if (!tescShader.IsValid()) return;
	ShaderHandle teseShader = m_pShaderManager->RegisterShader("../src/shaders/grid.tese.slang", SHADER_STAGE::TESSELLATION_EVALUATION, m_renderDevice);
	if (!teseShader.IsValid()) return;
	ShaderHandle fragShader = m_pShaderManager->RegisterShader("../src/shaders/grid.frag.slang", SHADER_STAGE::FRAGMENT, m_renderDevice);
	if (!fragShader.IsValid()) return;

	m_shaders = { vertShader, tescShader, teseShader, fragShader };

	// INPUT ATTRIBUTES
	m_inputAttributes =
	{
		{ 0, 0, BASE_FORMAT::R32G32B32_FLOAT },  // position (vec3)
		{ 1, 0, BASE_FORMAT::R32G32_FLOAT },     // uv (vec2)
	};

	// BUILD GRID VERTEX DATA
	RegenerateGrid();

	// DEPTH BUFFER
	TextureBaseCreateInfo depthBufferBaseCI{};
	depthBufferBaseCI.pName = "DepthBuffer";
	depthBufferBaseCI.width = m_swapChain.GetWidth();
	depthBufferBaseCI.height = m_swapChain.GetHeight();
	depthBufferBaseCI.arrayLayers = 1;
	depthBufferBaseCI.generateMips = false;
	depthBufferBaseCI.format = BASE_FORMAT::D32_FLOAT;
	depthBufferBaseCI.usageFlags = USAGE_TYPE_FLAG_DEPTH_STENCIL_ATTACHMENT | USAGE_TYPE_FLAG_SAMPLED;

	TextureViewCreateInfo depthBufferViewCI{};
	depthBufferViewCI.type = VIEW_TYPE::TYPE_2D;
	depthBufferViewCI.scope = VIEW_SCOPE::ENTIRE;
	depthBufferViewCI.aspectFlags = ASPECT_TYPE_FLAG_DEPTH;

	TextureSamplerCreateInfo depthBufferSamplerCI{};
	depthBufferSamplerCI.addressModeUVW = SAMPLER_ADDRESS_MODE::REPEAT;
	depthBufferSamplerCI.enableAnisotropicFiltering = false;
	depthBufferSamplerCI.magnificationFilter = FILTER_MODE::NEAREST;
	depthBufferSamplerCI.minificationFilter = FILTER_MODE::NEAREST;
	depthBufferSamplerCI.samplerMipMapFilter = FILTER_MODE::NEAREST;

	phxRes = m_renderDevice.AllocateTexture(depthBufferBaseCI, depthBufferViewCI, depthBufferSamplerCI, m_depthBuffer);
	CHECK_PHX_RES(phxRes);

	// VERTEX BUFFER — allocated to max capacity so grid size changes don't require reallocation
	{
		static constexpr uint32_t MAX_GRID_DIM = 256; // max quads per axis
		static constexpr uint32_t MAX_VERTS = MAX_GRID_DIM * MAX_GRID_DIM * 4;

		BufferCreateInfo ci{};
		ci.pName = "VertexBuffer";
		ci.bufferUsage = BUFFER_USAGE_FLAG_VERTEX_BUFFER;
		ci.sizeBytes = MAX_VERTS * sizeof(GridVertex);
		phxRes = m_renderDevice.AllocateBuffer(ci, m_vertexBuffer);
		CHECK_PHX_RES(phxRes);
	}

	// CAMERA UBO
	{
		BufferCreateInfo ci{};
		ci.pName = "CameraBuffer";
		ci.bufferUsage = BUFFER_USAGE_FLAG_UNIFORM_BUFFER;  
		ci.sizeBytes = sizeof(CameraUBO);
		phxRes = m_renderDevice.AllocateBuffer(ci, m_cameraBuffer);
		CHECK_PHX_RES(phxRes);
	}

	// TESS PARAMS UBO
	{
		BufferCreateInfo ci{};
		ci.pName = "TessParamsBuffer";
		ci.bufferUsage = BUFFER_USAGE_FLAG_UNIFORM_BUFFER; 
		ci.sizeBytes = sizeof(TessParams);
		phxRes = m_renderDevice.AllocateBuffer(ci, m_tessParamsBuffer);
		CHECK_PHX_RES(phxRes);
	}

	// UNIFORM COLLECTION
	CreateUniformCollection();

	// PIPELINE DESCRIPTIONS (solid + wireframe)
	auto configurePipeline = [](GraphicsPipelineDesc& desc)
	{
		desc.topology = PRIMITIVE_TOPOLOGY::PATCH_LIST;
		desc.patchControlPoints = 4;
		desc.viewportSize = { 0, 0 }; // Set dynamically
		desc.polygonMode = POLYGON_MODE::FILL;
		desc.cullMode = CULL_MODE::NONE;
		desc.frontFaceWinding = FRONT_FACE_WINDING::COUNTER_CLOCKWISE;
		desc.pShaders = nullptr; // Set below
		desc.shaderCount = 0;
		desc.pInputAttributes = nullptr; // Set below
		desc.attributeCount = 0;
		desc.uniformCollection = {};
		desc.enableDepthTest = true;
		desc.enableDepthWrite = true;
	};

	configurePipeline(m_solidPipelineDesc);
	m_solidPipelineDesc.polygonMode = POLYGON_MODE::FILL;
	m_solidPipelineDesc.pShaders = m_shaders.data();
	m_solidPipelineDesc.shaderCount = static_cast<u32>(m_shaders.size());
	m_solidPipelineDesc.pInputAttributes = m_inputAttributes.data();
	m_solidPipelineDesc.attributeCount = static_cast<u32>(m_inputAttributes.size());
	m_solidPipelineDesc.uniformCollection = m_uniformCollection;
	m_solidPipelineDesc.viewportSize = { m_swapChain.GetWidth(), m_swapChain.GetHeight() };

	configurePipeline(m_wireframePipelineDesc);
	m_wireframePipelineDesc.polygonMode = POLYGON_MODE::LINE;
	m_wireframePipelineDesc.pShaders = m_shaders.data();
	m_wireframePipelineDesc.shaderCount = static_cast<u32>(m_shaders.size());
	m_wireframePipelineDesc.pInputAttributes = m_inputAttributes.data();
	m_wireframePipelineDesc.attributeCount = static_cast<u32>(m_inputAttributes.size());
	m_wireframePipelineDesc.uniformCollection = m_uniformCollection;
	m_wireframePipelineDesc.viewportSize = { m_swapChain.GetWidth(), m_swapChain.GetHeight() };
}

void TessellationSample::ShutdownSample()
{
	m_shaders.clear();

	if (m_pCamera != nullptr)
	{
		delete m_pCamera;
		m_pCamera = nullptr;
	}
}

void TessellationSample::CreateUniformCollection()
{
	STATUS_CODE phxRes;

	// Set 0: camera UBO (binding 0, all stages), tess params (binding 1, tess stages)
	std::vector<UniformData> uniforms =
	{
		{ 0, UNIFORM_TYPE::UNIFORM_BUFFER,  SHADER_STAGE_FLAG_VERTEX | SHADER_STAGE_FLAG_TESSELLATION_CONTROL | SHADER_STAGE_FLAG_TESSELLATION_EVALUATION | SHADER_STAGE_FLAG_FRAGMENT },
		{ 1, UNIFORM_TYPE::UNIFORM_BUFFER,  SHADER_STAGE_FLAG_TESSELLATION_CONTROL | SHADER_STAGE_FLAG_TESSELLATION_EVALUATION },
	};

	UniformDataGroup group{};
	group.set = 0;
	group.uniformArray = uniforms.data();
	group.uniformArrayCount = static_cast<u32>(uniforms.size());

	std::array<UniformDataGroup, 1> groups = { group };

	UniformCollectionCreateInfo ci{};
	ci.dataGroups = groups.data();
	ci.groupCount = static_cast<u32>(groups.size());

	phxRes = m_renderDevice.AllocateUniformCollection(ci, m_uniformCollection);
	CHECK_PHX_RES(phxRes);

	// Queue initial buffer bindings
	m_uniformCollection.QueueBufferUpdate(m_cameraBuffer, 0, 0, 0);
	m_uniformCollection.QueueBufferUpdate(m_tessParamsBuffer, 0, 1, 0);
}

void TessellationSample::RegenerateGrid()
{
	m_patchCount = m_gridWidth * m_gridHeight;
	m_vertexCount = m_patchCount * 4;

	m_gridVertices.clear();
	m_gridVertices.reserve(m_vertexCount);

	const float gridWidthExtent  = static_cast<float>(m_gridWidth)  * m_cellSize;
	const float gridHeightExtent = static_cast<float>(m_gridHeight) * m_cellSize;
	const float originX = -gridWidthExtent * 0.5f;
	const float originZ = -gridHeightExtent * 0.5f;

	for (uint32_t z = 0; z < m_gridHeight; z++)
	{
		for (uint32_t x = 0; x < m_gridWidth; x++)
		{
			const float x0 = originX + static_cast<float>(x) * m_cellSize;
			const float x1 = x0 + m_cellSize;
			const float z0 = originZ + static_cast<float>(z) * m_cellSize;
			const float z1 = z0 + m_cellSize;

			const float u0 = static_cast<float>(x) / static_cast<float>(m_gridWidth);
			const float u1 = static_cast<float>(x + 1) / static_cast<float>(m_gridWidth);
			const float v0 = static_cast<float>(z) / static_cast<float>(m_gridHeight);
			const float v1 = static_cast<float>(z + 1) / static_cast<float>(m_gridHeight);

			// 4 control points per patch: BL, BR, TR, TL (counter-clockwise)
			m_gridVertices.push_back({ glm::vec3(x0, 0.0f, z0), glm::vec2(u0, v0) }); // BL
			m_gridVertices.push_back({ glm::vec3(x1, 0.0f, z0), glm::vec2(u1, v0) }); // BR
			m_gridVertices.push_back({ glm::vec3(x1, 0.0f, z1), glm::vec2(u1, v1) }); // TR
			m_gridVertices.push_back({ glm::vec3(x0, 0.0f, z1), glm::vec2(u0, v1) }); // TL
		}
	}

	m_gridDirty = true;
}

void TessellationSample::BuildImGuiUI()
{
	ImGui::Begin("Tessellation Controls");

	ImGui::Checkbox("Wireframe", &m_wireframe);

	ImGui::Separator();

	ImGui::SliderFloat("Max Tess Factor", &m_maxTessFactor, 1.0f, 64.0f, "%.1f");
	ImGui::SliderFloat("Min Distance", &m_minDistance, 1.0f, 50.0f, "%.1f");
	ImGui::SliderFloat("Max Distance", &m_maxDistance, 10.0f, 200.0f, "%.1f");
	ImGui::SliderFloat("Displacement", &m_displacementScale, 0.0f, 100.0f, "%.2f");

	ImGui::Separator();

	// Grid controls — regenerate when any dimension changes
	{
		int width  = static_cast<int>(m_gridWidth);
		int height = static_cast<int>(m_gridHeight);
		bool changed = false;

		if (ImGui::SliderInt("Grid Width", &width, 1, 256))
		{
			m_gridWidth = static_cast<uint32_t>(width);
			changed = true;
		}
		if (ImGui::SliderInt("Grid Height", &height, 1, 256))
		{
			m_gridHeight = static_cast<uint32_t>(height);
			changed = true;
		}
		if (ImGui::SliderFloat("Cell Size", &m_cellSize, 0.1f, 50.0f, "%.2f"))
		{
			changed = true;
		}

		if (changed)
		{
			RegenerateGrid();
		}
	}

	ImGui::Separator();

	ImGui::Text("Grid: %u x %u (%u patches, %u verts)", m_gridWidth, m_gridHeight, m_patchCount, m_vertexCount);
	ImGui::Text("FPS: %.1f", 1000.0f / (m_lastFrameTime * 1000.0f));
	ImGui::Text("Frame time: %.3f ms", m_lastFrameTime * 1000.0f);

	ImGui::Text("Camera pos: (%.1f, %.1f, %.1f)", m_cameraData.camPos.x, m_cameraData.camPos.y, m_cameraData.camPos.z);

	const PHX::Metrics& metrics = m_renderGraph.GetMetrics();
	ImGui::Separator();
	ImGui::Text("Draw calls: %u", metrics.drawCalls);
	ImGui::Text("Triangles: %u", metrics.triangles);
	ImGui::Text("Pass count: %u", metrics.passCount);
	ImGui::Text("GPU frame time: %.3f ms", metrics.gpuFrameTime);

	ImGui::End();
}
