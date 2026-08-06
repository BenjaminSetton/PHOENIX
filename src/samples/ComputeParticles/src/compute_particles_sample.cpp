
#include <array>
#include <vector>
#include <gtc/matrix_transform.hpp>

#include "../../common/src/utils/shader_utils.h"
#include "../../common/src/camera/freefly_camera.h"
#include "BSL/sanity.h"

#include "compute_particles_sample.h"

using namespace PHX;

#define CHECK_PHX_RES(phxRes) if(phxRes != PHX::STATUS_CODE::SUCCESS) { return; }

static constexpr u32 PARTICLE_UPDATE_WORKGROUP_COUNT = 256;
static constexpr u32 VERTICES_PER_PARTICLE = 6;

ComputeParticlesSample::ComputeParticlesSample() : m_simData(), m_volumeMinBound(-30), m_volumeMaxBound(30), 
	m_randomEngine(), m_mt(m_randomEngine())
{
}

ComputeParticlesSample::~ComputeParticlesSample()
{
}

void ComputeParticlesSample::UpdateSample(float dt)
{
	m_imguiBackend.NewFrame(dt, m_swapChain.GetWidth(), m_swapChain.GetHeight());

	std::uniform_real_distribution<float> dist(0.0f, 1.0f + FLT_EPSILON); // Gotta add FLT_EPSILON since uniform_real_distribution is [a, b)
	static bool isSimPaused = false;
	static bool enableRandomExplosions = true;
	ImGui::Checkbox("Pause sim", &isSimPaused);
	ImGui::Checkbox("Enable random explosions", &enableRandomExplosions);

	if (isSimPaused)
	{
		m_simData.dt = 0.0f;
	}
	else
	{
		m_simData.dt = dt;
		m_simData.totalTime += dt;
	}

	m_simData.shouldExplode = 0.0f;
	if (ImGui::Button("Explode!"))
	{
		// Set it to a random non-zero value since this is used to determine explosion center
		m_simData.shouldExplode = dist(m_mt) + FLT_EPSILON;
	}
	else
	{
		// Random explosions
		// Every few seconds, a random explosion epoch triggers. All particles
		// compute the same epoch ID deterministically, so they agree on the
		// explosion center. Particles near the center get a strong outward impulse
		if (enableRandomExplosions)
		{
			// Only check for explosions every second, not every frame
			bool checkExplosion = (int)m_simData.totalTime != (int)(m_simData.totalTime + dt);
			if (checkExplosion)
			{
				float explosionChance = dist(m_mt);
				m_simData.shouldExplode = explosionChance - 0.5f; // around 50% chance
			}
		}
	}

	if (m_pCamera != nullptr)
	{
		m_cameraData.view = m_pCamera->GetViewMatrix();
	}
}

void ComputeParticlesSample::Draw()
{
	STATUS_CODE phxRes;

	ClearValues clearColor{};
	clearColor.color.color = BSL::Vec4f(0.5f, 0.75f, 0.98f, 1.0f);
	clearColor.useClearColor = true;

	m_renderGraph.BeginFrame(m_swapChain);

	// Update pass - compute shader writes the particle buffer
	RenderPassHandle updatePass;
	phxRes = m_renderGraph.RegisterPass("ParticleUpdatePass", PASS_TYPE::COMPUTE, updatePass);
	CHECK_PHX_RES(phxRes);
	updatePass.SetBufferInput(m_particlesBuffer);	// read-modify-write: also depends on the seed pass
	updatePass.SetBufferOutput(m_particlesBuffer);

	updatePass.SetPipelineDescription(m_particlesPipelineDesc);
	updatePass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
		{
			// Uniform collection updates
			deviceContext.CopyDataToBuffer(m_simDataBuffer, &m_simData, sizeof(SimData));

			m_computeUniformCollection.QueueBufferUpdate(m_particlesBuffer, 0, 0, 0);
			m_computeUniformCollection.QueueBufferUpdate(m_simDataBuffer, 0, 1, 0);
			deviceContext.FlushUniformUpdates(m_computeUniformCollection);

			// Dispatch
			deviceContext.BindUniformCollection(m_computeUniformCollection);
			deviceContext.SetScissor({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
			deviceContext.SetViewport({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });

			TECHDEBT("Workgroup count should be pulled from shader reflection")
			const float dimX = static_cast<float>(m_simData.totalParticles) / PARTICLE_UPDATE_WORKGROUP_COUNT;
			deviceContext.Dispatch({static_cast<u32>(dimX + 0.5f), 1, 1});
		});

	// Draw pass - reads the particle buffer and expands each particle into a quad
	RenderPassHandle drawPass;
	phxRes = m_renderGraph.RegisterPass("ParticleDrawPass", PASS_TYPE::GRAPHICS, drawPass);
	CHECK_PHX_RES(phxRes);
	drawPass.SetBufferInput(m_particlesBuffer);
	drawPass.SetTextureOutput(m_swapChain.GetCurrentImage(), ATTACHMENT_LOAD_OP::CLEAR, ATTACHMENT_STORE_OP::STORE, clearColor);
	drawPass.SetDepthOutput(m_depthBuffer);

	drawPass.SetPipelineDescription(m_drawPipelineDesc);
	drawPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
		{
			// Uniform collection updates. Transposed since Slang is row-major
			CameraData camData;
			camData.view = glm::transpose(m_cameraData.view);
			camData.proj = glm::transpose(m_cameraData.proj);
			deviceContext.CopyDataToBuffer(m_cameraBuffer, &camData, sizeof(CameraData));

			m_drawUniformCollection.QueueBufferUpdate(m_particlesBuffer, 0, 0, 0);
			m_drawUniformCollection.QueueBufferUpdate(m_cameraBuffer, 0, 1, 0);
			deviceContext.FlushUniformUpdates(m_drawUniformCollection);

			// Draw commands
			deviceContext.BindUniformCollection(m_drawUniformCollection);
			deviceContext.SetScissor({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
			deviceContext.SetViewport({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
			deviceContext.Draw(VERTICES_PER_PARTICLE * m_simData.totalParticles);
		});

	// Outline pass
	RenderPassHandle outlinePass;
	phxRes = m_renderGraph.RegisterPass("CubeOutlinePass", PASS_TYPE::GRAPHICS, outlinePass);
	CHECK_PHX_RES(phxRes);
	outlinePass.SetBufferInput(m_outlineVertexBuffer);
	outlinePass.SetBufferInput(m_outlineIndexBuffer);
	outlinePass.SetTextureOutput(m_swapChain.GetCurrentImage(), ATTACHMENT_LOAD_OP::LOAD, ATTACHMENT_STORE_OP::STORE, {});

	outlinePass.SetPipelineDescription(m_outlinePipelineDesc);
	outlinePass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
	{
		m_outlineUniformCollection.QueueBufferUpdate(m_cameraBuffer, 0, 0, 0);
		deviceContext.FlushUniformUpdates(m_outlineUniformCollection);

		deviceContext.BindUniformCollection(m_outlineUniformCollection);
		deviceContext.SetScissor({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
		deviceContext.SetViewport({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
		deviceContext.BindMesh(m_outlineVertexBuffer, m_outlineIndexBuffer);
		deviceContext.DrawIndexed(24);
	});

	// ImGui pass
	ImGui::Render();
	m_imguiRenderer.RenderDrawData(m_renderGraph, m_swapChain, ImGui::GetDrawData(), false);

	m_renderGraph.Bake(m_swapChain);

	// Viz
	GenerateRenderGraphVisualization("ComputeParticles");

	m_renderGraph.EndFrame(m_swapChain);
}

void ComputeParticlesSample::InitSample()
{
	STATUS_CODE phxRes;

	m_window.SetWindowTitle("PHX %u.%u.%u | COMPUTE PARTICLES", PHX::GetMajorVersion(), PHX::GetMinorVersion(), PHX::GetPatchVersion());

	// Set particle count cap (must be set before allocating the particle buffer)
	// ONE MILLI!
	m_simData.totalParticles = 1000000;

	// SHADERS
	ShaderHandle particlesShader;
	particlesShader = m_pShaderManager->LoadShader("../src/shaders/particles.comp.slang", SHADER_STAGE::COMPUTE, m_renderDevice);
	if (!particlesShader.IsValid())
	{
		return;
	}
	m_particleComputeShader.push_back(particlesShader);

	ShaderHandle vertShader;
	vertShader = m_pShaderManager->LoadShader("../src/shaders/particles.vert.slang", SHADER_STAGE::VERTEX, m_renderDevice);
	if (!vertShader.IsValid())
	{
		return;
	}
	m_drawShaders.push_back(vertShader);

	ShaderHandle fragShader;
	fragShader = m_pShaderManager->LoadShader("../src/shaders/particles.frag.slang", SHADER_STAGE::FRAGMENT, m_renderDevice);
	if (!fragShader.IsValid())
	{
		return;
	}
	m_drawShaders.push_back(fragShader);

	// OUTLINE SHADERS
	ShaderHandle outlineVertShader;
	outlineVertShader = m_pShaderManager->LoadShader("../src/shaders/outline.vert.slang", SHADER_STAGE::VERTEX, m_renderDevice);
	if (!outlineVertShader.IsValid())
	{
		return;
	}
	m_outlineShaders.push_back(outlineVertShader);

	ShaderHandle outlineFragShader;
	outlineFragShader = m_pShaderManager->LoadShader("../src/shaders/outline.frag.slang", SHADER_STAGE::FRAGMENT, m_renderDevice);
	if (!outlineFragShader.IsValid())
	{
		return;
	}
	m_outlineShaders.push_back(outlineFragShader);

	// PARTICLE BUFFER (one entry per particle)
	BufferCreateInfo particlesBufferCI{};
	particlesBufferCI.pName = "ParticlesBuffer";
	particlesBufferCI.bufferUsage = BUFFER_USAGE_FLAG_STORAGE_BUFFER;
	particlesBufferCI.sizeBytes = sizeof(ParticleData) * m_simData.totalParticles;
	phxRes = m_renderDevice.AllocateBuffer(particlesBufferCI, m_particlesBuffer);
	CHECK_PHX_RES(phxRes);

	// SIM DATA UNIFORM BUFFER
	BufferCreateInfo simDataBufferCI{};
	simDataBufferCI.pName = "SimDataBuffer";
	simDataBufferCI.bufferUsage = BUFFER_USAGE_FLAG_UNIFORM_BUFFER;
	simDataBufferCI.sizeBytes = sizeof(SimData);
	phxRes = m_renderDevice.AllocateBuffer(simDataBufferCI, m_simDataBuffer);
	CHECK_PHX_RES(phxRes);

	// CAMERA UNIFORM BUFFER
	BufferCreateInfo cameraBufferCI{};
	cameraBufferCI.pName = "CameraBuffer";
	cameraBufferCI.bufferUsage = BUFFER_USAGE_FLAG_UNIFORM_BUFFER;
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
	outlineVBCI.bufferUsage = BUFFER_USAGE_FLAG_VERTEX_BUFFER;
	outlineVBCI.sizeBytes = 8 * sizeof(glm::vec3);
	phxRes = m_renderDevice.AllocateBuffer(outlineVBCI, m_outlineVertexBuffer);
	CHECK_PHX_RES(phxRes);

	// OUTLINE INDEX BUFFER
	BufferCreateInfo outlineIBCI{};
	outlineIBCI.pName = "OutlineIndexBuffer";
	outlineIBCI.bufferUsage = BUFFER_USAGE_FLAG_INDEX_BUFFER;
	outlineIBCI.sizeBytes = 24 * sizeof(uint32_t);
	phxRes = m_renderDevice.AllocateBuffer(outlineIBCI, m_outlineIndexBuffer);
	CHECK_PHX_RES(phxRes);

	// CAMERA
	const float cameraSpeed = 15.0f;
	const float cameraSensitivity = 0.15f;
	m_pCamera = new Common::FreeflyCamera(cameraSpeed, cameraSensitivity, glm::vec3(0.0f, 5.0f, 70.0f), glm::vec3(0.0f, 5.0f, 0.0f));

	constexpr float fov = glm::radians(45.0f);
	const float aspectRatio = static_cast<float>(m_swapChain.GetWidth()) / m_swapChain.GetHeight();
	m_cameraData.view = m_pCamera->GetViewMatrix();
	m_cameraData.proj = glm::perspective(fov, aspectRatio, 0.01f, 1000.0f);
	m_cameraData.proj[1][1] *= -1.0f;

	// UNIFORM COLLECTIONS
	CreateComputeUniformCollection();
	CreateDrawUniformCollection();
	CreateOutlineUniformCollection();

	// COMPUTE PIPELINE
	m_particlesPipelineDesc.shader = particlesShader;
	m_particlesPipelineDesc.uniformCollection = m_computeUniformCollection;

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

	// UPLOAD PASSES
	InitializeParticleBuffer();
	InitializeOutlineBuffers();
}

void ComputeParticlesSample::ShutdownSample()
{
	m_outlineShaders.clear();
	m_drawShaders.clear();
	m_particleComputeShader.clear();

	if (m_pCamera != nullptr)
	{
		delete m_pCamera;
		m_pCamera = nullptr;
	}
}

void ComputeParticlesSample::CreateComputeUniformCollection()
{
	// SET 0, BINDING 0
	UniformData particleBufferData;
	particleBufferData.binding = 0;
	particleBufferData.shaderStageFlags = SHADER_STAGE_FLAG_COMPUTE;
	particleBufferData.type = UNIFORM_TYPE::STORAGE_BUFFER;

	// SET 0, BINDING 1
	UniformData simParamsData;
	simParamsData.binding = 1;
	simParamsData.shaderStageFlags = SHADER_STAGE_FLAG_COMPUTE;
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

	STATUS_CODE phxRes = m_renderDevice.AllocateUniformCollection(uniformCollectionCI, m_computeUniformCollection);
	CHECK_PHX_RES(phxRes);
}

void ComputeParticlesSample::CreateDrawUniformCollection()
{
	// SET 0, BINDING 0 - particle storage buffer, read by the vertex shader
	UniformData particleBufferData;
	particleBufferData.binding = 0;
	particleBufferData.shaderStageFlags = SHADER_STAGE_FLAG_VERTEX;
	particleBufferData.type = UNIFORM_TYPE::STORAGE_BUFFER;

	// SET 0, BINDING 1 - camera view / projection matrices
	UniformData cameraData;
	cameraData.binding = 1;
	cameraData.shaderStageFlags = SHADER_STAGE_FLAG_VERTEX;
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
	cameraData.shaderStageFlags = SHADER_STAGE_FLAG_VERTEX;
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
	STATUS_CODE phxRes = m_renderGraph.RegisterPass("InitParticleBufferPass", PASS_TYPE::TRANSFER, initPass);
	CHECK_PHX_RES(phxRes);

	initPass.SetBufferOutput(m_particlesBuffer);
	initPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
	{
		std::vector<ParticleData> initialParticles(m_simData.totalParticles);
		for (u32 i = 0; i < m_simData.totalParticles; i++)
		{
			// Zero all data except position. Particles spawn underneath the spawn area, so they're immediately
			// killed and recycled by the compute shader. 
			glm::mat4 transform(1.0f);
			transform[0] = glm::vec4(0.0f);                     // velocity
			transform[1] = glm::vec4(0.0f);                     // (lifetime, scale, maxLifetime)
			transform[3] = glm::vec4(0.0f, -1.0f, 0.0f, 1.0f);  // position

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
	STATUS_CODE phxRes = m_renderGraph.RegisterPass("InitOutlineBufferPass", PASS_TYPE::TRANSFER, initPass);
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
		const u32 cubeIndices[24] =
		{
			0, 1,  1, 2,  2, 3,  3, 0,  // bottom face (-Z)
			4, 5,  5, 6,  6, 7,  7, 4,  // top face (+Z)
			0, 4,  1, 5,  2, 6,  3, 7   // vertical pillars
		};
		deviceContext.CopyDataToBuffer(m_outlineVertexBuffer, cubeVertices, sizeof(cubeVertices));
		deviceContext.CopyDataToBuffer(m_outlineIndexBuffer, cubeIndices, sizeof(cubeIndices));
	});
}