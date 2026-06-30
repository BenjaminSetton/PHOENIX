
#include <array>
#include <vector>
#include <gtc/matrix_transform.hpp>

#include "../../common/src/utils/shader_utils.h"
#include "../../common/src/camera/freefly_camera.h"

#include "compute_particles_sample.h"

using namespace PHX;

#define CHECK_PHX_RES(phxRes) if(phxRes != PHX::STATUS_CODE::SUCCESS) { return; }

ComputeParticlesSample::ComputeParticlesSample() : m_simData()
{
	Init();
}

ComputeParticlesSample::~ComputeParticlesSample()
{
	Shutdown();
}

bool ComputeParticlesSample::Update(float dt)
{
	m_simData.dt = dt;
	m_simData.totalTime += dt;

	bool shouldClose = BaseSample::Update(dt);

	if (m_pCamera != nullptr)
	{
		m_cameraData.view = m_pCamera->GetViewMatrix();
	}

	return shouldClose;
}

void ComputeParticlesSample::Draw()
{
	STATUS_CODE phxRes;

	ClearValues clearColor{};
	clearColor.color.color = Vec4f(0.5f, 0.75f, 0.98f, 1.0f);
	clearColor.useClearColor = true;

	ClearValues clearDepth{};
	clearDepth.depthStencil.depthClear = 1.0f;
	clearDepth.depthStencil.stencilClear = 0;
	clearDepth.useClearColor = false;

	std::array<ClearValues, 2> clearVals =
	{
		clearColor,
		clearDepth
	};

	m_renderGraph.BeginFrame(m_swapChain);

	// Update pass - compute shader writes the particle buffer
	RenderPassHandle updatePass;
	phxRes = m_renderGraph.RegisterPass("ParticleUpdatePass", BIND_POINT::COMPUTE, updatePass);
	CHECK_PHX_RES(phxRes);
	updatePass.SetBufferInput(m_particlesBuffer);	// read-modify-write: also depends on the seed pass
	updatePass.SetBufferOutput(m_particlesBuffer);

	updatePass.SetPipelineDescription(m_particlesPipelineDesc);
	updatePass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
		{
			// Uniform collection updates
			deviceContext.CopyDataToBuffer(m_simDataBuffer, &m_simData, sizeof(SimData));

			m_uniformCollection.QueueBufferUpdate(m_particlesBuffer, 0, 0, 0);
			m_uniformCollection.QueueBufferUpdate(m_simDataBuffer, 0, 1, 0);
			m_uniformCollection.FlushUpdateQueue();

			// Dispatch
			deviceContext.BindUniformCollection(m_uniformCollection);
			deviceContext.SetScissor({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
			deviceContext.SetViewport({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });

			float dimX = m_simData.totalParticles / 256.0f;
			deviceContext.Dispatch({static_cast<u32>(dimX + 0.5f), 1, 1});
		});

	// Draw pass - reads the particle buffer and expands each particle into a quad
	RenderPassHandle drawPass;
	phxRes = m_renderGraph.RegisterPass("ParticleDrawPass", BIND_POINT::GRAPHICS, drawPass);
	CHECK_PHX_RES(phxRes);
	drawPass.SetBufferInput(m_particlesBuffer);
	drawPass.SetBackbufferOutput(m_swapChain.GetCurrentImage());
	drawPass.SetDepthOutput(m_depthBuffer);

	drawPass.SetPipelineDescription(m_drawPipelineDesc);
	drawPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
		{
			// Uniform collection updates
			deviceContext.CopyDataToBuffer(m_cameraBuffer, &m_cameraData, sizeof(CameraData));

			m_drawUniformCollection.QueueBufferUpdate(m_particlesBuffer, 0, 0, 0);
			m_drawUniformCollection.QueueBufferUpdate(m_cameraBuffer, 0, 1, 0);
			m_drawUniformCollection.FlushUpdateQueue();

			// Draw commands - 6 vertices (2 triangles) per particle, no vertex/index buffer
			deviceContext.BindUniformCollection(m_drawUniformCollection);
			deviceContext.SetScissor({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
			deviceContext.SetViewport({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
			deviceContext.Draw(6 * m_simData.totalParticles);
		});

	// Viz
	//{
	//	const u32 frameNumber = m_renderGraph.GetFrameNumber();
	//	std::string renderGraphVisName = "./ComputeParticles_RG_";
	//	renderGraphVisName.append(std::to_string(frameNumber).c_str());
	//	renderGraphVisName.append(".dot");

	//	m_renderGraph.GenerateVisualization(renderGraphVisName.c_str(), false);
	//}

	m_renderGraph.Bake(clearVals.data(), static_cast<u32>(clearVals.size()));
	m_renderGraph.EndFrame();

	m_swapChain.Present();
}

void ComputeParticlesSample::Init()
{
	STATUS_CODE phxRes;

	m_pWindow->SetWindowTitle("PHX %u.%u.%u | COMPUTE PARTICLES", PHX::GetMajorVersion(), PHX::GetMinorVersion(), PHX::GetPatchVersion());

	// Set particle count cap (must be set before allocating the particle buffer)
	m_simData.totalParticles = 1000;

	// SHADERS
	ShaderHandle particlesShader;
	if (!Common::AllocateShader("../src/shaders/particles.comp", SHADER_STAGE::COMPUTE, m_renderDevice, particlesShader))
	{
		return;
	}
	m_shaders.push_back(particlesShader);

	ShaderHandle vertShader;
	if (!Common::AllocateShader("../src/shaders/particles.vert", SHADER_STAGE::VERTEX, m_renderDevice, vertShader))
	{
		return;
	}

	ShaderHandle fragShader;
	if (!Common::AllocateShader("../src/shaders/particles.frag", SHADER_STAGE::FRAGMENT, m_renderDevice, fragShader))
	{
		return;
	}
	m_drawShaders.push_back(vertShader);
	m_drawShaders.push_back(fragShader);

	// PARTICLE BUFFER (one entry per particle)
	BufferCreateInfo particlesBufferCI{};
	particlesBufferCI.pName = "ParticlesBuffer";
	particlesBufferCI.bufferUsage = BUFFER_USAGE::STORAGE_BUFFER;
	particlesBufferCI.sizeBytes = sizeof(ParticleData) * m_simData.totalParticles;
	phxRes = m_renderDevice.AllocateBuffer(particlesBufferCI, m_particlesBuffer);
	CHECK_PHX_RES(phxRes);

	// SIM DATA UNIFORM BUFFER
	BufferCreateInfo simDataBufferCI{};
	simDataBufferCI.pName = "SimDataBuffer";
	simDataBufferCI.bufferUsage = BUFFER_USAGE::UNIFORM_BUFFER;
	simDataBufferCI.sizeBytes = sizeof(SimData);
	phxRes = m_renderDevice.AllocateBuffer(simDataBufferCI, m_simDataBuffer);
	CHECK_PHX_RES(phxRes);

	// CAMERA UNIFORM BUFFER
	BufferCreateInfo cameraBufferCI{};
	cameraBufferCI.pName = "CameraBuffer";
	cameraBufferCI.bufferUsage = BUFFER_USAGE::UNIFORM_BUFFER;
	cameraBufferCI.sizeBytes = sizeof(CameraData);
	phxRes = m_renderDevice.AllocateBuffer(cameraBufferCI, m_cameraBuffer);
	CHECK_PHX_RES(phxRes);

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

	// CAMERA
	const float cameraSpeed = 4.0f;
	const float cameraSensitivity = 0.2f;
	m_pCamera = new Common::FreeflyCamera(cameraSpeed, cameraSensitivity);

	const float fov = glm::radians(45.0f);
	const float aspectRatio = static_cast<float>(m_swapChain.GetWidth()) / m_swapChain.GetHeight();
	m_cameraData.view = m_pCamera->GetViewMatrix();
	m_cameraData.proj = glm::perspective(fov, aspectRatio, 0.01f, 1000.0f);

	// UNIFORM COLLECTIONS
	CreateUniformCollection();
	CreateDrawUniformCollection();

	// COMPUTE PIPELINE
	m_particlesPipelineDesc.shader = particlesShader;
	m_particlesPipelineDesc.uniformCollection = m_uniformCollection;

	// GRAPHICS PIPELINE (no vertex layout - particle data is pulled from the storage buffer)
	m_drawPipelineDesc.viewportSize = { m_swapChain.GetWidth(), m_swapChain.GetHeight() };
	m_drawPipelineDesc.viewportPos = { 0, 0 };
	m_drawPipelineDesc.polygonMode = POLYGON_MODE::FILL;
	m_drawPipelineDesc.topology = PRIMITIVE_TOPOLOGY::TRIANGLE_LIST;
	m_drawPipelineDesc.cullMode = CULL_MODE::NONE;
	m_drawPipelineDesc.pShaders = m_drawShaders.data();
	m_drawPipelineDesc.shaderCount = static_cast<u32>(m_drawShaders.size());
	m_drawPipelineDesc.pInputAttributes = nullptr;
	m_drawPipelineDesc.attributeCount = 0;
	m_drawPipelineDesc.uniformCollection = m_drawUniformCollection;
	m_drawPipelineDesc.enableDepthTest = true;
	m_drawPipelineDesc.enableDepthWrite = true;

	// Seed the particle buffer with initial transforms / colors
	SeedParticles();
}

void ComputeParticlesSample::Shutdown()
{
	m_drawShaders.clear();
	m_shaders.clear();

	delete m_pCamera;
	m_pCamera = nullptr;
}

void ComputeParticlesSample::CreateUniformCollection()
{
	// SET 0, BINDING 0
	UniformData particleBufferData;
	particleBufferData.binding = 0;
	particleBufferData.shaderStage = SHADER_STAGE::COMPUTE;
	particleBufferData.type = UNIFORM_TYPE::STORAGE_BUFFER;

	// SET 0, BINDING 1
	UniformData simParamsData;
	simParamsData.binding = 1;
	simParamsData.shaderStage = SHADER_STAGE::COMPUTE;
	simParamsData.type = UNIFORM_TYPE::UNIFORM_BUFFER;

	std::array<UniformData, 2> uniformData =
	{
		particleBufferData,
		simParamsData
	};

	UniformDataGroup particleDataGroup;
	particleDataGroup.set = 0;
	particleDataGroup.uniformArray = uniformData.data();
	particleDataGroup.uniformArrayCount = static_cast<u32>(uniformData.size());

	std::array<UniformDataGroup, 1> dataGroups =
	{
		particleDataGroup
	};

	UniformCollectionCreateInfo uniformCollectionCI{};
	uniformCollectionCI.dataGroups = dataGroups.data();
	uniformCollectionCI.groupCount = static_cast<u32>(dataGroups.size());

	STATUS_CODE phxRes = m_renderDevice.AllocateUniformCollection(uniformCollectionCI, m_uniformCollection);
	CHECK_PHX_RES(phxRes);
}

void ComputeParticlesSample::CreateDrawUniformCollection()
{
	// SET 0, BINDING 0 - particle storage buffer, read by the vertex shader
	UniformData particleBufferData;
	particleBufferData.binding = 0;
	particleBufferData.shaderStage = SHADER_STAGE::VERTEX;
	particleBufferData.type = UNIFORM_TYPE::STORAGE_BUFFER;

	// SET 0, BINDING 1 - camera view / projection matrices
	UniformData cameraData;
	cameraData.binding = 1;
	cameraData.shaderStage = SHADER_STAGE::VERTEX;
	cameraData.type = UNIFORM_TYPE::UNIFORM_BUFFER;

	std::array<UniformData, 2> uniformData =
	{
		particleBufferData,
		cameraData
	};

	UniformDataGroup drawDataGroup;
	drawDataGroup.set = 0;
	drawDataGroup.uniformArray = uniformData.data();
	drawDataGroup.uniformArrayCount = static_cast<u32>(uniformData.size());

	std::array<UniformDataGroup, 1> dataGroups =
	{
		drawDataGroup
	};

	UniformCollectionCreateInfo uniformCollectionCI{};
	uniformCollectionCI.dataGroups = dataGroups.data();
	uniformCollectionCI.groupCount = static_cast<u32>(dataGroups.size());

	STATUS_CODE phxRes = m_renderDevice.AllocateUniformCollection(uniformCollectionCI, m_drawUniformCollection);
	CHECK_PHX_RES(phxRes);
}

void ComputeParticlesSample::SeedParticles()
{
	RenderPassHandle seedPass;
	STATUS_CODE phxRes = m_renderGraph.RegisterPass("ParticleSeedPass", BIND_POINT::TRANSFER, seedPass);
	CHECK_PHX_RES(phxRes);

	seedPass.SetBufferOutput(m_particlesBuffer);
	seedPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
		{
			std::vector<ParticleData> initialParticles(m_simData.totalParticles);
			for (u32 i = 0; i < m_simData.totalParticles; i++)
			{
				// Scatter the initial translations across a [-5, 5] cube. The compute
				// shader takes over the per-frame motion / color animation from here.
				float fx = static_cast<float>((i * 73u) % 1000u) / 1000.0f;
				float fy = static_cast<float>((i * 179u) % 1000u) / 1000.0f;
				float fz = static_cast<float>((i * 283u) % 1000u) / 1000.0f;

				glm::vec3 pos = (glm::vec3(fx, fy, fz) - 0.5f) * 10.0f;

				initialParticles[i].transform = glm::translate(glm::mat4(1.0f), pos);
				initialParticles[i].color = glm::vec4(fx, fy, fz, 1.0f);
			}

			deviceContext.CopyDataToBuffer(m_particlesBuffer, initialParticles.data(),
				sizeof(ParticleData) * m_simData.totalParticles);
		});
}