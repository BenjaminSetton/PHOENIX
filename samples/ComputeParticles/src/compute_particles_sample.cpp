
#include <array>
#include <vector>
#include <gtc/matrix_transform.hpp>

#include "../../common/src/utils/shader_utils.h"
#include "../../common/src/camera/freefly_camera.h"

#include "compute_particles_sample.h"

using namespace PHX;

#define CHECK_PHX_RES(phxRes) if(phxRes != PHX::STATUS_CODE::SUCCESS) { return; }

ComputeParticlesSample::ComputeParticlesSample() : m_simData(), m_volumeMinBound(-30), m_volumeMaxBound(30)
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
	drawPass.SetTextureOutput(m_swapChain.GetCurrentImage(), ATTACHMENT_LOAD_OP::CLEAR, ATTACHMENT_STORE_OP::STORE, clearColor);
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

	// Outline pass - renders the 12 edges of the [-5, 5]^3 spawn cube as white lines
	// Registered after ParticleDrawPass; the render graph detects the WAW hazard on the
	// swapchain and automatically orders this pass after the draw pass.
	RenderPassHandle outlinePass;
	phxRes = m_renderGraph.RegisterPass("CubeOutlinePass", BIND_POINT::GRAPHICS, outlinePass);
	CHECK_PHX_RES(phxRes);
	outlinePass.SetBufferInput(m_outlineVertexBuffer);
	outlinePass.SetBufferInput(m_outlineIndexBuffer);
	outlinePass.SetTextureOutput(m_swapChain.GetCurrentImage(), ATTACHMENT_LOAD_OP::LOAD, ATTACHMENT_STORE_OP::STORE, {});

	outlinePass.SetPipelineDescription(m_outlinePipelineDesc);
	outlinePass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
	{
		m_outlineUniformCollection.QueueBufferUpdate(m_cameraBuffer, 0, 0, 0);
		m_outlineUniformCollection.FlushUpdateQueue();

		deviceContext.BindUniformCollection(m_outlineUniformCollection);
		deviceContext.SetScissor({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
		deviceContext.SetViewport({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
		deviceContext.BindMesh(m_outlineVertexBuffer, m_outlineIndexBuffer);
		deviceContext.DrawIndexed(24);
	});

	// Viz
	//{
	//	const u32 frameNumber = m_renderGraph.GetFrameNumber();
	//	std::string renderGraphVisName = "./ComputeParticles_RG_";
	//	renderGraphVisName.append(std::to_string(frameNumber).c_str());
	//	renderGraphVisName.append(".dot");

	//	m_renderGraph.GenerateVisualization(renderGraphVisName.c_str(), false);
	//}

	m_renderGraph.Bake(m_swapChain);
	m_renderGraph.EndFrame();

	m_swapChain.Present();
}

void ComputeParticlesSample::Init()
{
	STATUS_CODE phxRes;

	m_pWindow->SetWindowTitle("PHX %u.%u.%u | COMPUTE PARTICLES", PHX::GetMajorVersion(), PHX::GetMinorVersion(), PHX::GetPatchVersion());

	// Set particle count cap (must be set before allocating the particle buffer)
	// ONE MILLI!
	m_simData.totalParticles = 1000000;

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

	// OUTLINE SHADERS
	ShaderHandle outlineVertShader;
	if (!Common::AllocateShader("../src/shaders/outline.vert", SHADER_STAGE::VERTEX, m_renderDevice, outlineVertShader))
	{
		return;
	}
	ShaderHandle outlineFragShader;
	if (!Common::AllocateShader("../src/shaders/outline.frag", SHADER_STAGE::FRAGMENT, m_renderDevice, outlineFragShader))
	{
		return;
	}
	m_outlineShaders.push_back(outlineVertShader);
	m_outlineShaders.push_back(outlineFragShader);

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

	// OUTLINE VERTEX BUFFER
	BufferCreateInfo outlineVBCI{};
	outlineVBCI.pName = "OutlineVertexBuffer";
	outlineVBCI.bufferUsage = BUFFER_USAGE::VERTEX_BUFFER;
	outlineVBCI.sizeBytes = 8 * sizeof(glm::vec3);
	phxRes = m_renderDevice.AllocateBuffer(outlineVBCI, m_outlineVertexBuffer);
	CHECK_PHX_RES(phxRes);

	// OUTLINE INDEX BUFFER
	BufferCreateInfo outlineIBCI{};
	outlineIBCI.pName = "OutlineIndexBuffer";
	outlineIBCI.bufferUsage = BUFFER_USAGE::INDEX_BUFFER;
	outlineIBCI.sizeBytes = 24 * sizeof(uint32_t);
	phxRes = m_renderDevice.AllocateBuffer(outlineIBCI, m_outlineIndexBuffer);
	CHECK_PHX_RES(phxRes);

	// CAMERA
	const float cameraSpeed = 10.0f;
	const float cameraSensitivity = 0.15f;
	m_pCamera = new Common::FreeflyCamera(cameraSpeed, cameraSensitivity, glm::vec3(0.0f, 5.0f, 70.0f), glm::vec3(0.0f, 5.0f, 0.0f));

	const float fov = glm::radians(45.0f);
	const float aspectRatio = static_cast<float>(m_swapChain.GetWidth()) / m_swapChain.GetHeight();
	m_cameraData.view = m_pCamera->GetViewMatrix();
	m_cameraData.proj = glm::perspective(fov, aspectRatio, 0.01f, 1000.0f);

	// UNIFORM COLLECTIONS
	CreateUniformCollection();
	CreateDrawUniformCollection();
	CreateOutlineUniformCollection();

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

	// OUTLINE PIPELINE (LINE_LIST, no depth test/write, explicit vec3 vertex input)
	m_outlineInputAttribute.location = 0;
	m_outlineInputAttribute.binding  = 0;
	m_outlineInputAttribute.format   = BASE_FORMAT::R32G32B32_FLOAT;

	m_outlinePipelineDesc.topology          = PRIMITIVE_TOPOLOGY::LINE_LIST;
	m_outlinePipelineDesc.viewportSize      = { m_swapChain.GetWidth(), m_swapChain.GetHeight() };
	m_outlinePipelineDesc.viewportPos       = { 0, 0 };
	m_outlinePipelineDesc.polygonMode       = POLYGON_MODE::FILL;
	m_outlinePipelineDesc.cullMode          = CULL_MODE::NONE;
	m_outlinePipelineDesc.pInputAttributes  = &m_outlineInputAttribute;
	m_outlinePipelineDesc.attributeCount    = 1;
	m_outlinePipelineDesc.pShaders          = m_outlineShaders.data();
	m_outlinePipelineDesc.shaderCount       = static_cast<u32>(m_outlineShaders.size());
	m_outlinePipelineDesc.uniformCollection = m_outlineUniformCollection;
	m_outlinePipelineDesc.enableDepthTest   = false;
	m_outlinePipelineDesc.enableDepthWrite  = false;

	// Initialize buffers
	InitializeParticleBuffer();
	InitializeOutlineBuffers();
}

void ComputeParticlesSample::Shutdown()
{
	m_outlineShaders.clear();
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

void ComputeParticlesSample::CreateOutlineUniformCollection()
{
	// SET 0, BINDING 0 - camera view / projection matrices, read by the vertex shader
	UniformData cameraData;
	cameraData.binding = 0;
	cameraData.shaderStage = SHADER_STAGE::VERTEX;
	cameraData.type = UNIFORM_TYPE::UNIFORM_BUFFER;

	std::array<UniformData, 1> uniformData =
	{
		cameraData
	};

	UniformDataGroup outlineDataGroup;
	outlineDataGroup.set = 0;
	outlineDataGroup.uniformArray = uniformData.data();
	outlineDataGroup.uniformArrayCount = static_cast<u32>(uniformData.size());

	std::array<UniformDataGroup, 1> dataGroups =
	{
		outlineDataGroup
	};

	UniformCollectionCreateInfo uniformCollectionCI{};
	uniformCollectionCI.dataGroups = dataGroups.data();
	uniformCollectionCI.groupCount = static_cast<u32>(dataGroups.size());

	STATUS_CODE phxRes = m_renderDevice.AllocateUniformCollection(uniformCollectionCI, m_outlineUniformCollection);
	CHECK_PHX_RES(phxRes);
}

void ComputeParticlesSample::InitializeParticleBuffer()
{
	RenderPassHandle initPass;
	STATUS_CODE phxRes = m_renderGraph.RegisterPass("InitParticleBufferPass", BIND_POINT::TRANSFER, initPass);
	CHECK_PHX_RES(phxRes);

	initPass.SetBufferOutput(m_particlesBuffer);
	initPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
		{
			std::vector<ParticleData> initialParticles(m_simData.totalParticles);
			for (u32 i = 0; i < m_simData.totalParticles; i++)
			{
				float fx = static_cast<float>((i * 73u) % 1000u) / 1000.0f;
				float fy = static_cast<float>((i * 179u) % 1000u) / 1000.0f;
				float fz = static_cast<float>((i * 283u) % 1000u) / 1000.0f;

				// Spawn at the top plane (world max = bottom on screen due to Vulkan Y-flip).
				glm::vec3 position(
					(fx - 0.5f) * 20.0f,
					static_cast<float>(m_volumeMaxBound) - 1.0f,
					(fz - 0.5f) * 20.0f
				);

				// Random downward velocity (world space = upward on screen).
				glm::vec3 velocity(
					(fy - 0.5f) * 4.0f,
					-(1.0f + fx * 5.0f + fy * 2.0f),
					(fz - 0.5f) * 4.0f
				);

				// Stagger initial lifetimes so particles don't all respawn simultaneously.
				float maxLifetime = 1.5f + fy * 3.0f;
				float lifetime = maxLifetime * fx;
				float scale = 0.1f + fz * 0.25f;

				glm::mat4 transform(1.0f);
				transform[0] = glm::vec4(velocity, 0.0f);                    // velocity
				transform[1] = glm::vec4(lifetime, scale, maxLifetime, 0.0f); // (lifetime, scale, maxLifetime)
				transform[3] = glm::vec4(position, 1.0f);                   // position

				initialParticles[i].transform = transform;
				initialParticles[i].color = glm::vec4(1.0f, 0.6f, 0.2f, 1.0f); // hot orange
			}

			deviceContext.CopyDataToBuffer(m_particlesBuffer, initialParticles.data(),
				sizeof(ParticleData) * m_simData.totalParticles);
		});
}

void ComputeParticlesSample::InitializeOutlineBuffers()
{
	RenderPassHandle initPass;
	STATUS_CODE phxRes = m_renderGraph.RegisterPass("InitOutlineBufferPass", BIND_POINT::TRANSFER, initPass);
	CHECK_PHX_RES(phxRes);

	initPass.SetBufferOutput(m_outlineVertexBuffer);
	initPass.SetBufferOutput(m_outlineIndexBuffer);
	initPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
	{
		const glm::vec3 cubeVertices[8] =
		{
			{m_volumeMinBound, m_volumeMinBound, m_volumeMinBound}, {m_volumeMaxBound, m_volumeMinBound, m_volumeMinBound},
			{m_volumeMaxBound, m_volumeMaxBound, m_volumeMinBound}, {m_volumeMinBound, m_volumeMaxBound, m_volumeMinBound},
			{m_volumeMinBound, m_volumeMinBound, m_volumeMaxBound}, {m_volumeMaxBound, m_volumeMinBound, m_volumeMaxBound},
			{m_volumeMaxBound, m_volumeMaxBound, m_volumeMaxBound}, {m_volumeMinBound, m_volumeMaxBound, m_volumeMaxBound}
		};
		const uint32_t cubeIndices[24] =
		{
			0, 1,  1, 2,  2, 3,  3, 0,  // bottom face (-Z)
			4, 5,  5, 6,  6, 7,  7, 4,  // top face (+Z)
			0, 4,  1, 5,  2, 6,  3, 7   // vertical pillars
		};
		deviceContext.CopyDataToBuffer(m_outlineVertexBuffer, cubeVertices, sizeof(cubeVertices));
		deviceContext.CopyDataToBuffer(m_outlineIndexBuffer, cubeIndices, sizeof(cubeIndices));
	});
}