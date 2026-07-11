
#include <array>

#include "ray_tracing_sample.h"

#include "../../common/src/utils/shader_utils.h"

using namespace PHX;

#define CHECK_PHX_RES(phxRes) if(phxRes != PHX::STATUS_CODE::SUCCESS) { return; }

static const BlitVertex s_blitTriangle[3] =
{
	{ glm::vec2(-1.0f, -1.0f), glm::vec2(0.0f, 0.0f) },
	{ glm::vec2( 3.0f, -1.0f), glm::vec2(2.0f, 0.0f) },
	{ glm::vec2(-1.0f,  3.0f), glm::vec2(0.0f, 2.0f) },
};

RayTracingSample::RayTracingSample()
{
	Init();
}

RayTracingSample::~RayTracingSample()
{
	Shutdown();
}

bool RayTracingSample::Update(float dt)
{
	return BaseSample::Update(dt);
}

void RayTracingSample::Draw()
{
	STATUS_CODE phxRes;

	m_renderGraph.BeginFrame(m_swapChain);

	if (m_rayTracingSupported)
	{
		// Ray tracing pass - writes the output image
		RenderPassHandle rayTracingPass;
		phxRes = m_renderGraph.RegisterPass("RayTracingPass", BIND_POINT::RAY_TRACING, rayTracingPass);
		CHECK_PHX_RES(phxRes);

		rayTracingPass.SetTextureOutput(m_rayTracingOutput, ATTACHMENT_LOAD_OP::IGNORE, ATTACHMENT_STORE_OP::STORE, {});
		rayTracingPass.SetPipelineDescription(m_rayTracingPipelineDesc);
		rayTracingPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
		{
			m_rayTracingUniformCollection.QueueImageUpdate(m_rayTracingOutput, 0, 0, 0);
			m_rayTracingUniformCollection.FlushUpdateQueue();

			deviceContext.BindUniformCollection(m_rayTracingUniformCollection);
			deviceContext.TraceRays({ m_swapChain.GetWidth(), m_swapChain.GetHeight(), 1 });
		});

		// Blit pass - samples the ray tracing output and draws to the swapchain
		RenderPassHandle blitPass;
		phxRes = m_renderGraph.RegisterPass("BlitPass", BIND_POINT::GRAPHICS, blitPass);
		CHECK_PHX_RES(phxRes);

		blitPass.SetBufferInput(m_blitVertexBuffer);
		blitPass.SetTextureInput(m_rayTracingOutput);
		blitPass.SetColorOutput(m_swapChain.GetCurrentImage());
		blitPass.SetPipelineDescription(m_blitPipelineDesc);
		blitPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
		{
			m_blitUniformCollection.QueueImageUpdate(m_rayTracingOutput, 0, 0, 0);
			m_blitUniformCollection.FlushUpdateQueue();

			deviceContext.BindUniformCollection(m_blitUniformCollection);
			deviceContext.BindVertexBuffer(m_blitVertexBuffer);
			deviceContext.SetViewport({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
			deviceContext.SetScissor({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
			deviceContext.Draw(3);
		});
	}
	else
	{
		// Fallback clear-screen pass
		RenderPassHandle clearPass;
		phxRes = m_renderGraph.RegisterPass("ClearPass", BIND_POINT::GRAPHICS, clearPass);
		CHECK_PHX_RES(phxRes);

		ClearValues clearVals{};
		clearVals.color.color = { 0.2f, 0.25f, 0.35f, 1.0f };
		clearPass.SetTextureOutput(m_swapChain.GetCurrentImage(), ATTACHMENT_LOAD_OP::CLEAR, ATTACHMENT_STORE_OP::STORE, clearVals);
	}

	phxRes = m_renderGraph.Bake(m_swapChain);
	CHECK_PHX_RES(phxRes);

	// Viz
	{
		const u32 frameNumber = m_renderGraph.GetFrameNumber();
		const u32 nameLen = 64;
		char renderGraphVisName[nameLen];
		snprintf(renderGraphVisName, nameLen, "./RayTracing_RG_%u.dot", frameNumber);
		m_renderGraph.GenerateVisualization(renderGraphVisName);
	}

	m_renderGraph.EndFrame();

	m_swapChain.Present();
}

void RayTracingSample::Init()
{
	STATUS_CODE phxRes;

	m_window.SetWindowTitle("PHX %u.%u.%u | RAY TRACING", PHX::GetMajorVersion(), PHX::GetMinorVersion(), PHX::GetPatchVersion());

	m_rayTracingSupported = m_renderDevice.IsRayTracingSupported();

	// RAY TRACING OUTPUT IMAGE
	TextureBaseCreateInfo rtOutputBaseCI{};
	rtOutputBaseCI.pName = "RayTracingOutput";
	rtOutputBaseCI.width = m_swapChain.GetWidth();
	rtOutputBaseCI.height = m_swapChain.GetHeight();
	rtOutputBaseCI.mipLevels = 1;
	rtOutputBaseCI.generateMips = false;
	rtOutputBaseCI.format = BASE_FORMAT::R8G8B8A8_UNORM;
	rtOutputBaseCI.usageFlags = USAGE_TYPE_FLAG_STORAGE | USAGE_TYPE_FLAG_SAMPLED;
	rtOutputBaseCI.sampleFlags = SAMPLE_COUNT::COUNT_1;

	TextureViewCreateInfo rtOutputViewCI{};
	rtOutputViewCI.type = VIEW_TYPE::TYPE_2D;
	rtOutputViewCI.scope = VIEW_SCOPE::ENTIRE;
	rtOutputViewCI.aspectFlags = ASPECT_TYPE_FLAG_COLOR;

	TextureSamplerCreateInfo rtOutputSamplerCI{};
	rtOutputSamplerCI.addressModeUVW = SAMPLER_ADDRESS_MODE::CLAMP_TO_EDGE;
	rtOutputSamplerCI.enableAnisotropicFiltering = false;
	rtOutputSamplerCI.magnificationFilter = FILTER_MODE::NEAREST;
	rtOutputSamplerCI.minificationFilter = FILTER_MODE::NEAREST;
	rtOutputSamplerCI.samplerMipMapFilter = FILTER_MODE::NEAREST;

	phxRes = m_renderDevice.AllocateTexture(rtOutputBaseCI, rtOutputViewCI, rtOutputSamplerCI, m_rayTracingOutput);
	CHECK_PHX_RES(phxRes);

	// UNIFORM COLLECTIONS
	CreateUniformCollections();

	// BLIT INPUT ATTRIBUTES
	m_blitInputAttributes =
	{
		{
			0,
			0,
			BASE_FORMAT::R32G32_FLOAT
		},
		{
			1,
			0,
			BASE_FORMAT::R32G32_FLOAT
		}
	};

	// BLIT SHADERS
	ShaderHandle blitVertShader;
	if (!Common::AllocateShader("../src/shaders/blit.vert", SHADER_STAGE::VERTEX, m_renderDevice, blitVertShader))
	{
		return;
	}
	m_blitPipelineShaders.push_back(blitVertShader);

	ShaderHandle blitFragShader;
	if (!Common::AllocateShader("../src/shaders/blit.frag", SHADER_STAGE::FRAGMENT, m_renderDevice, blitFragShader))
	{
		return;
	}
	m_blitPipelineShaders.push_back(blitFragShader);

	// BLIT PIPELINE
	m_blitPipelineDesc.topology = PRIMITIVE_TOPOLOGY::TRIANGLE_LIST;
	m_blitPipelineDesc.pInputAttributes = m_blitInputAttributes.data();
	m_blitPipelineDesc.attributeCount = static_cast<u32>(m_blitInputAttributes.size());
	m_blitPipelineDesc.pShaders = m_blitPipelineShaders.data();
	m_blitPipelineDesc.shaderCount = static_cast<u32>(m_blitPipelineShaders.size());
	m_blitPipelineDesc.viewportSize = { m_swapChain.GetWidth(), m_swapChain.GetHeight() };
	m_blitPipelineDesc.viewportPos = { 0, 0 };
	m_blitPipelineDesc.cullMode = CULL_MODE::NONE;
	m_blitPipelineDesc.uniformCollection = m_blitUniformCollection;
	m_blitPipelineDesc.enableDepthTest = false;
	m_blitPipelineDesc.enableDepthWrite = false;

	// RAY TRACING SHADER + PIPELINE
	if (m_rayTracingSupported)
	{
		ShaderHandle rayGenShader;
		if (!Common::AllocateShader("../src/shaders/raygen.rgen", SHADER_STAGE::RAYGEN, m_renderDevice, rayGenShader))
		{
			return;
		}
		m_rayTracingPipelineShaders.push_back(rayGenShader);

		m_rayTracingPipelineDesc.pShaders = m_rayTracingPipelineShaders.data();
		m_rayTracingPipelineDesc.shaderCount = static_cast<u32>(m_rayTracingPipelineShaders.size());
		m_rayTracingPipelineDesc.uniformCollection = m_rayTracingUniformCollection;
	}

	// BLIT VERTEX BUFFER
	BufferCreateInfo blitVBufferCI{};
	blitVBufferCI.pName = "BlitVertexBuffer";
	blitVBufferCI.bufferUsage = BUFFER_USAGE::VERTEX_BUFFER;
	blitVBufferCI.sizeBytes = sizeof(BlitVertex) * 3;
	phxRes = m_renderDevice.AllocateBuffer(blitVBufferCI, m_blitVertexBuffer);
	CHECK_PHX_RES(phxRes);

	UploadBlitVertices();
}

void RayTracingSample::Shutdown()
{
}

void RayTracingSample::OverrideSettings(Settings& settings)
{
	// Override version to allow ray tracing (>=1.2 in Vulkan)
	settings.backendAPIMajorVersion = 1;
	settings.backendAPIMinorVersion = 2;
}

void RayTracingSample::CreateUniformCollections()
{
	// Ray tracing pipeline: storage image for the ray tracing output
	UniformData rtOutputData{};
	rtOutputData.binding = 0;
	rtOutputData.shaderStage = SHADER_STAGE::RAYGEN;
	rtOutputData.type = UNIFORM_TYPE::STORAGE_IMAGE;

	UniformDataGroup rtDataGroup{};
	rtDataGroup.set = 0;
	rtDataGroup.uniformArray = &rtOutputData;
	rtDataGroup.uniformArrayCount = 1;

	UniformCollectionCreateInfo rtUniformCollectionCI{};
	rtUniformCollectionCI.dataGroups = &rtDataGroup;
	rtUniformCollectionCI.groupCount = 1;

	STATUS_CODE phxRes = m_renderDevice.AllocateUniformCollection(rtUniformCollectionCI, m_rayTracingUniformCollection);
	CHECK_PHX_RES(phxRes);

	// Blit pipeline: combined image sampler for the ray tracing output
	UniformData blitImageData{};
	blitImageData.binding = 0;
	blitImageData.shaderStage = SHADER_STAGE::FRAGMENT;
	blitImageData.type = UNIFORM_TYPE::COMBINED_IMAGE_SAMPLER;

	UniformDataGroup blitDataGroup{};
	blitDataGroup.set = 0;
	blitDataGroup.uniformArray = &blitImageData;
	blitDataGroup.uniformArrayCount = 1;

	UniformCollectionCreateInfo blitUniformCollectionCI{};
	blitUniformCollectionCI.dataGroups = &blitDataGroup;
	blitUniformCollectionCI.groupCount = 1;

	phxRes = m_renderDevice.AllocateUniformCollection(blitUniformCollectionCI, m_blitUniformCollection);
	CHECK_PHX_RES(phxRes);
}

void RayTracingSample::UploadBlitVertices()
{
	RenderPassHandle uploadPass;
	STATUS_CODE phxRes = m_renderGraph.RegisterPass("UploadBlitVertices", BIND_POINT::TRANSFER, uploadPass);
	CHECK_PHX_RES(phxRes);

	uploadPass.SetBufferOutput(m_blitVertexBuffer);
	uploadPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
	{
		deviceContext.CopyDataToBuffer(m_blitVertexBuffer, s_blitTriangle, sizeof(BlitVertex) * 3);
	});
}
