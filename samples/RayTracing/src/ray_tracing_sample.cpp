
#include <array>
#include <cfloat>
#include <iostream>
#include <vector>

#include "ray_tracing_sample.h"

#include <gtc/matrix_transform.hpp>
#include <gtc/packing.hpp>

#include "../../common/src/utils/shader_utils.h"
#include "../../common/src/utils/asset_importer.h"
#include "../../common/src/utils/tex_utils.h"

using namespace PHX;

#define CHECK_PHX_RES(phxRes) if(phxRes != PHX::STATUS_CODE::SUCCESS) { return; }

static const BlitVertex s_blitTriangle[3] =
{
	{ glm::vec2(-1.0f, -1.0f), glm::vec2(0.0f, 0.0f) },
	{ glm::vec2( 3.0f, -1.0f), glm::vec2(2.0f, 0.0f) },
	{ glm::vec2(-1.0f,  3.0f), glm::vec2(0.0f, 2.0f) },
};

// Default 1x1 texture pixel values (RGBA in memory byte order on little-endian)
static const u32 s_defaultTexturePixels[] =
{
	0xFFFFFFFF, // albedo (white)
	0xFFFF8080, // normal (flat, pointing +Z)
	0xFFFFFFFF, // specular (white)
};
static const u32 s_defaultTextureCount = sizeof(s_defaultTexturePixels) / sizeof(s_defaultTexturePixels[0]);

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
	bool shouldClose = BaseSample::Update(dt);
	UpdateCameraData(dt);
	return shouldClose;
}

void RayTracingSample::Draw()
{
	STATUS_CODE phxRes;

	if (m_renderGraph.BeginFrame(m_swapChain) != STATUS_CODE::SUCCESS)
	{
		return;
	}

	if (m_rayTracingSupported)
	{
		// Update frame counter and reset state
		m_cameraData.frameCount = m_frameCount;
		m_cameraData.resetAccumulation = m_resetAccumulation ? 1u : 0u;

		// Ray tracing pass - writes the output image
		RenderPassHandle rayTracingPass;
		phxRes = m_renderGraph.RegisterPass("RayTracingPass", PASS_TYPE::RAY_TRACING, rayTracingPass);
		CHECK_PHX_RES(phxRes);

		rayTracingPass.SetTextureOutput(m_rayTracingOutput, ATTACHMENT_LOAD_OP::IGNORE, ATTACHMENT_STORE_OP::STORE, {});
		rayTracingPass.SetTextureOutput(m_accumulationImageA, ATTACHMENT_LOAD_OP::IGNORE, ATTACHMENT_STORE_OP::STORE, {});
		rayTracingPass.SetAccelerationStructureInput(m_tlas);
		rayTracingPass.SetBufferInput(m_sceneVertexBuffer);
		rayTracingPass.SetBufferInput(m_sceneIndexBuffer);
		rayTracingPass.SetBufferInput(m_geometryInfoBuffer);
		rayTracingPass.SetBufferInput(m_materialBuffer);
		rayTracingPass.SetTextureInput(m_envCubeMap);
		for (u32 i = 0; i < m_sceneTextures.size(); i++)
		{
			rayTracingPass.SetTextureInput(m_sceneTextures[i]);
		}
		rayTracingPass.SetPipelineDescription(m_rayTracingPipelineDesc);
		rayTracingPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
		{
			deviceContext.CopyDataToBuffer(m_cameraUniformBuffer, &m_cameraData, sizeof(CameraData));

			for (u32 i = 0; i < m_sceneTextures.size(); i++)
			{
				m_rayTracingUniformCollection.QueueImageUpdate(m_sceneTextures[i], 2, 0, 0, i);
			}

			m_rayTracingUniformCollection.QueueAccelerationStructureUpdate(m_tlas, 0, 1);
			m_rayTracingUniformCollection.QueueBufferUpdate(m_sceneVertexBuffer, 0, 2, 0);
			m_rayTracingUniformCollection.QueueBufferUpdate(m_sceneIndexBuffer, 0, 3, 0);
			m_rayTracingUniformCollection.QueueBufferUpdate(m_geometryInfoBuffer, 0, 4, 0);
			m_rayTracingUniformCollection.QueueBufferUpdate(m_materialBuffer, 0, 5, 0);
			m_rayTracingUniformCollection.QueueImageUpdate(m_envCubeMap, 0, 6, 0);
			m_rayTracingUniformCollection.QueueImageUpdate(m_accumulationImageA, 0, 7, 0);

			m_rayTracingUniformCollection.QueueBufferUpdate(m_cameraUniformBuffer, 1, 0, 0);
			m_rayTracingUniformCollection.QueueImageUpdate(m_rayTracingOutput, 0, 0, 0);
			deviceContext.FlushUniformUpdates(m_rayTracingUniformCollection);

			deviceContext.BindUniformCollection(m_rayTracingUniformCollection);
			deviceContext.TraceRays({ m_swapChain.GetWidth(), m_swapChain.GetHeight(), 1 });
		});

		// Blit pass - samples the ray tracing output and draws to the swapchain
		RenderPassHandle blitPass;
		phxRes = m_renderGraph.RegisterPass("BlitPass", PASS_TYPE::GRAPHICS, blitPass);
		CHECK_PHX_RES(phxRes);

		blitPass.SetBufferInput(m_blitVertexBuffer);
		blitPass.SetTextureInput(m_rayTracingOutput);
		blitPass.SetColorOutput(m_swapChain.GetCurrentImage());
		blitPass.SetPipelineDescription(m_blitPipelineDesc);
		blitPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
		{
			m_blitUniformCollection.QueueImageUpdate(m_rayTracingOutput, 0, 0, 0);
			deviceContext.FlushUniformUpdates(m_blitUniformCollection);

			deviceContext.BindUniformCollection(m_blitUniformCollection);
			deviceContext.BindVertexBuffer(m_blitVertexBuffer);
			deviceContext.SetViewport({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
			deviceContext.SetScissor({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
			deviceContext.Draw(3);
		});

		// Update accumulation state for next frame
		if (m_resetAccumulation)
		{
			m_frameCount = 0;
			m_resetAccumulation = false;
		}
		m_frameCount++;
	}
	else
	{
		// Fallback clear-screen pass
		RenderPassHandle clearPass;
		phxRes = m_renderGraph.RegisterPass("ClearPass", PASS_TYPE::GRAPHICS, clearPass);
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

	m_renderGraph.EndFrame(m_swapChain);
}

void RayTracingSample::Init()
{
	STATUS_CODE phxRes;

	m_window.SetWindowTitle("PHX %u.%u.%u | RAY TRACING", PHX::GetMajorVersion(), PHX::GetMinorVersion(), PHX::GetPatchVersion());

	m_rayTracingSupported = m_renderDevice.IsRayTracingSupported();

	const glm::vec3 startingCameraPos = { -10.5, 2.0, 1.0 };
	const glm::vec3 startingCameraRot = { 60.0f, -30.0f, 0.0f };
	m_pCamera = new Common::FreeflyCamera(2.5f, 0.15f, startingCameraPos, startingCameraRot);

	m_projMatrix = glm::perspective(glm::radians(60.0f), static_cast<float>(m_swapChain.GetWidth()) / static_cast<float>(m_swapChain.GetHeight()), 0.1f, 1000.0f);
	m_projMatrix[1][1] *= -1.0f;

	// RAY TRACING OUTPUT IMAGE
	TextureBaseCreateInfo rtOutputBaseCI{};
	rtOutputBaseCI.pName = "RayTracingOutput";
	rtOutputBaseCI.width = m_swapChain.GetWidth();
	rtOutputBaseCI.height = m_swapChain.GetHeight();
	rtOutputBaseCI.mipLevels = 1;
	rtOutputBaseCI.generateMips = false;
	rtOutputBaseCI.format = BASE_FORMAT::R16G16B16A16_FLOAT;
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

	// BLIT VERTEX BUFFER
	BufferCreateInfo blitVBufferCI{};
	blitVBufferCI.pName = "BlitVertexBuffer";
	blitVBufferCI.bufferUsage = BUFFER_USAGE::VERTEX_BUFFER;
	blitVBufferCI.sizeBytes = sizeof(BlitVertex) * 3;
	phxRes = m_renderDevice.AllocateBuffer(blitVBufferCI, m_blitVertexBuffer);
	CHECK_PHX_RES(phxRes);

	// SCENE
	LoadSceneAssets();
	CreateDefaultTextures();
	CreateSceneTextures();
	CreateSceneGeometryBuffers();

	if (m_rayTracingSupported)
	{
		BuildSceneAccelerationStructures();
		LoadEnvironmentMap();
		CreateAccumulationImages();
	}
	UploadBlitVertices();

	// UNIFORM COLLECTIONS
	CreateUniformCollections();

	// BLIT SHADERS
	ShaderHandle blitVertShader;
	blitVertShader = m_pShaderManager->RegisterShader("../src/shaders/blit.vert", SHADER_STAGE::VERTEX, m_renderDevice);
	if (!blitVertShader.IsValid())
	{
		return;
	}
	m_blitPipelineShaders.push_back(blitVertShader);

	ShaderHandle blitFragShader;
	blitFragShader = m_pShaderManager->RegisterShader("../src/shaders/blit.frag", SHADER_STAGE::FRAGMENT, m_renderDevice);
	if (!blitFragShader.IsValid())
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

	// RAY TRACING SHADERS + PIPELINE
	if (m_rayTracingSupported)
	{
		ShaderHandle rayGenShader;
		rayGenShader = m_pShaderManager->RegisterShader("../src/shaders/raygen.rgen", SHADER_STAGE::RAYGEN, m_renderDevice);
		if (!rayGenShader.IsValid())
		{
			return;
		}
		m_rayTracingPipelineShaders.push_back(rayGenShader);

		ShaderHandle missShader;
		missShader = m_pShaderManager->RegisterShader("../src/shaders/miss.rmiss", SHADER_STAGE::MISS, m_renderDevice);
		if (!missShader.IsValid())
		{
			return;
		}
		m_rayTracingPipelineShaders.push_back(missShader);

		ShaderHandle shadowMissShader;
		shadowMissShader = m_pShaderManager->RegisterShader("../src/shaders/shadowMiss.rmiss", SHADER_STAGE::MISS, m_renderDevice);
		if (!shadowMissShader.IsValid())
		{
			return;
		}
		m_rayTracingPipelineShaders.push_back(shadowMissShader);

		ShaderHandle closestHitShader;
		closestHitShader = m_pShaderManager->RegisterShader("../src/shaders/closesthit.rchit", SHADER_STAGE::CLOSEST_HIT, m_renderDevice);
		if (!closestHitShader.IsValid())
		{
			return;
		}
		m_rayTracingPipelineShaders.push_back(closestHitShader);

		ShaderHandle anyHitShader;
		anyHitShader = m_pShaderManager->RegisterShader("../src/shaders/anyhit.rahit", SHADER_STAGE::ANY_HIT, m_renderDevice);
		if (!anyHitShader.IsValid())
		{
			return;
		}
		m_rayTracingPipelineShaders.push_back(anyHitShader);

		ShaderHandle bounceClosestHitShader;
		bounceClosestHitShader = m_pShaderManager->RegisterShader("../src/shaders/bounce_closesthit.rchit", SHADER_STAGE::CLOSEST_HIT, m_renderDevice);
		if (!bounceClosestHitShader.IsValid())
		{
			return;
		}
		m_rayTracingPipelineShaders.push_back(bounceClosestHitShader);

		// Shader indices: 0=raygen, 1=miss, 2=shadowMiss, 3=closestHit, 4=anyHit, 5=bounceClosestHit
		// Hit group 0: full closest-hit + any-hit (for primary rays — traces shadow + bounce)
		// Hit group 1: bounce closest-hit + any-hit (for bounce rays — traces shadow only, no further bounce)
		m_rayTracingHitGroups.resize(2);
		m_rayTracingHitGroups[0].closestHitShaderIndex = 3;
		m_rayTracingHitGroups[0].anyHitShaderIndex = 4;
		m_rayTracingHitGroups[0].intersectionShaderIndex = UINT32_MAX;
		m_rayTracingHitGroups[1].closestHitShaderIndex = 5;
		m_rayTracingHitGroups[1].anyHitShaderIndex = 4;
		m_rayTracingHitGroups[1].intersectionShaderIndex = UINT32_MAX;

		m_rayTracingPipelineDesc.pShaders = m_rayTracingPipelineShaders.data();
		m_rayTracingPipelineDesc.shaderCount = static_cast<u32>(m_rayTracingPipelineShaders.size());
		m_rayTracingPipelineDesc.pHitGroups = m_rayTracingHitGroups.data();
		m_rayTracingPipelineDesc.hitGroupCount = static_cast<u32>(m_rayTracingHitGroups.size());
		m_rayTracingPipelineDesc.uniformCollection = m_rayTracingUniformCollection;
		// raygen(0) -> closesthit(0) -> shadow ray(1) + bounce ray(1) -> bounceClosesthit(1) -> shadow ray(2) = depth 2
		m_rayTracingPipelineDesc.maxRecursionDepth = 2;
	}
}

void RayTracingSample::Shutdown()
{
	if (m_pCamera != nullptr)
	{
		delete m_pCamera;
		m_pCamera = nullptr;
	}
}

void RayTracingSample::LoadSceneAssets()
{
	m_assetHandle = AssetManager::Get().LoadOrImport("Bistro_v5_2/BistroExterior.fbx");
	if (m_assetHandle == Common::INVALID_ASSET_HANDLE)
	{
		std::cout << "Failed to load Bistro assets!" << std::endl;
		return;
	}

	m_pAsset = AssetManager::Get().GetAsset(m_assetHandle);
	if (m_pAsset == nullptr)
	{
		std::cout << "Failed to get Bistro asset!" << std::endl;
	}
}

void RayTracingSample::CreateDefaultTextures()
{
	// Default indices: 0=albedo(white), 1=normal(flat), 2=specular(white)
	struct DefaultTexData
	{
		const char* name;
		BASE_FORMAT format;
	};

	static const DefaultTexData defaults[] =
	{
		{"DefaultAlbedo",     BASE_FORMAT::R8G8B8A8_SRGB},
		{"DefaultNormal",     BASE_FORMAT::R8G8B8A8_UNORM},
		{"DefaultSpecular",   BASE_FORMAT::R8G8B8A8_UNORM},
	};

	for (u32 i = 0; i < s_defaultTextureCount; i++)
	{
		TextureBaseCreateInfo baseCI{};
		baseCI.pName = defaults[i].name;
		baseCI.width = 1;
		baseCI.height = 1;
		baseCI.arrayLayers = 1;
		baseCI.generateMips = false;
		baseCI.mipLevels = 1;
		baseCI.format = defaults[i].format;
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

		TextureHandle texHandle;
		STATUS_CODE res = m_renderDevice.AllocateTexture(baseCI, viewCI, samplerCI, texHandle);
		CHECK_PHX_RES(res);

		m_sceneTextures.push_back(texHandle);
	}
}

void RayTracingSample::CreateSceneTextures()
{
	if (m_pAsset == nullptr)
	{
		return;
	}

	m_textureIndexRemap.assign(m_pAsset->textures.size(), 0);

	for (u32 i = 0; i < static_cast<u32>(m_pAsset->textures.size()); i++)
	{
		const Common::TextureType& currTex = m_pAsset->textures[i];

		const u32 numMips = static_cast<u32>(currTex.mipLevels.size());
		if (numMips == 0)
		{
			continue;
		}

		m_textureIndexRemap[i] = static_cast<u32>(m_sceneTextures.size());

		TextureBaseCreateInfo baseCI{};
		baseCI.pName = currTex.name.c_str();
		baseCI.width = currTex.mipLevels[0].size.GetX();
		baseCI.height = currTex.mipLevels[0].size.GetY();
		baseCI.arrayLayers = 1;
		baseCI.generateMips = false;
		baseCI.mipLevels = numMips;

		if (currTex.IsCompressed())
		{
			baseCI.format = currTex.format;
		}
		else
		{
			baseCI.format = (currTex.type == Common::TEXTURE_TYPE::DIFFUSE) ? BASE_FORMAT::R8G8B8A8_SRGB : BASE_FORMAT::R8G8B8A8_UNORM;
		}
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

		TextureHandle texHandle;
		STATUS_CODE res = m_renderDevice.AllocateTexture(baseCI, viewCI, samplerCI, texHandle);
		if (res != STATUS_CODE::SUCCESS)
		{
			std::cout << "[ASSET] Failed to allocate scene texture \"" << currTex.name << "\", skipping" << std::endl;
			continue;
		}

		m_sceneTextures.push_back(texHandle);
	}
}

void RayTracingSample::CreateSceneGeometryBuffers()
{
	if (m_pAsset == nullptr)
	{
		return;
	}

	STATUS_CODE phxRes;

	BufferCreateInfo vBufferCI{};
	vBufferCI.pName = "SceneVertexBuffer";
	vBufferCI.bufferUsage = BUFFER_USAGE::ACCELERATION_STRUCTURE_BUILD_INPUT;
	vBufferCI.sizeBytes = sizeof(AssetVertex) * m_pAsset->vertices.size();
	phxRes = m_renderDevice.AllocateBuffer(vBufferCI, m_sceneVertexBuffer);
	CHECK_PHX_RES(phxRes);

	BufferCreateInfo iBufferCI{};
	iBufferCI.pName = "SceneIndexBuffer";
	iBufferCI.bufferUsage = BUFFER_USAGE::ACCELERATION_STRUCTURE_BUILD_INPUT;
	iBufferCI.sizeBytes = sizeof(Common::AssetIndexType) * m_pAsset->indices.size();
	phxRes = m_renderDevice.AllocateBuffer(iBufferCI, m_sceneIndexBuffer);
	CHECK_PHX_RES(phxRes);

	struct GeometryInfo
	{
		PHX::u32 firstVertex;
		PHX::u32 firstIndex;
		PHX::u32 materialIndex;
		PHX::u32 padding;
	};
	std::vector<GeometryInfo> geometryInfos;
	geometryInfos.reserve(m_pAsset->meshes.size());
	for (const Mesh& mesh : m_pAsset->meshes)
	{
		GeometryInfo info{};
		info.firstVertex = mesh.firstVertex;
		info.firstIndex = mesh.firstIndex;
		info.materialIndex = mesh.materialIndex;
		geometryInfos.push_back(info);
	}

	BufferCreateInfo gBufferCI{};
	gBufferCI.pName = "GeometryInfoBuffer";
	gBufferCI.bufferUsage = BUFFER_USAGE::STORAGE_BUFFER;
	gBufferCI.sizeBytes = sizeof(GeometryInfo) * geometryInfos.size();
	phxRes = m_renderDevice.AllocateBuffer(gBufferCI, m_geometryInfoBuffer);
	CHECK_PHX_RES(phxRes);

	// Build per-material texture index mapping
	std::vector<MaterialInfo> materialInfos;
	materialInfos.reserve(m_pAsset->materials.size());
	for (const Material& mat : m_pAsset->materials)
	{
		MaterialInfo matInfo{};
		for (u32 texIdx : mat.textureIndices)
		{
			if (texIdx >= m_textureIndexRemap.size())
				continue;

			Common::TEXTURE_TYPE texType = m_pAsset->textures[texIdx].type;
			u32 offsetIdx = m_textureIndexRemap[texIdx];

			switch (texType)
			{
			case Common::TEXTURE_TYPE::DIFFUSE:              matInfo.albedoTexIndex    = offsetIdx; break;
			case Common::TEXTURE_TYPE::NORMAL:               matInfo.normalTexIndex    = offsetIdx; break;
			case Common::TEXTURE_TYPE::SPECULAR:             matInfo.specularTexIndex  = offsetIdx; break;
			default: break;
			}
		}
		materialInfos.push_back(matInfo);
	}

	BufferCreateInfo matBufferCI{};
	matBufferCI.pName = "MaterialBuffer";
	matBufferCI.bufferUsage = BUFFER_USAGE::STORAGE_BUFFER;
	matBufferCI.sizeBytes = sizeof(MaterialInfo) * materialInfos.size();
	phxRes = m_renderDevice.AllocateBuffer(matBufferCI, m_materialBuffer);
	CHECK_PHX_RES(phxRes);

	BufferCreateInfo camBufferCI{};
	camBufferCI.pName = "CameraUniformBuffer";
	camBufferCI.bufferUsage = BUFFER_USAGE::UNIFORM_BUFFER;
	camBufferCI.sizeBytes = sizeof(CameraData);
	phxRes = m_renderDevice.AllocateBuffer(camBufferCI, m_cameraUniformBuffer);
	CHECK_PHX_RES(phxRes);
}

void RayTracingSample::BuildSceneAccelerationStructures()
{
	if (m_pAsset == nullptr)
	{
		return;
	}

	STATUS_CODE phxRes;

	// Allocate BLAS handles for each mesh
	m_blas.resize(m_pAsset->meshes.size());
	m_blasNames.reserve(m_pAsset->meshes.size());
	for (u32 i = 0; i < static_cast<u32>(m_pAsset->meshes.size()); i++)
	{
		const Mesh& mesh = m_pAsset->meshes[i];

		GeometryData geometry{};
		geometry.type = GEOMETRY_TYPE::TRIANGLES;
		geometry.flags = GEOMETRY_FLAG_NONE;
		geometry.vertexBuffer = m_sceneVertexBuffer;
		geometry.indexBuffer = m_sceneIndexBuffer;
		geometry.vertexCount = mesh.vertexCount;
		geometry.indexCount = mesh.indexCount;
		geometry.vertexStride = sizeof(AssetVertex);
		geometry.firstVertex = mesh.firstVertex;
		geometry.indexByteOffset = sizeof(Common::AssetIndexType) * mesh.firstIndex;
		geometry.indexType = INDEX_TYPE::U32;

		m_blasNames.emplace_back("BistroMesh_" + std::to_string(i));

		AccelerationStructureCreateInfo blasCI{};
		blasCI.pName = m_blasNames.back().c_str();
		blasCI.type = ACCELERATION_STRUCTURE_TYPE::BOTTOM_LEVEL;
		blasCI.pGeometries = &geometry;
		blasCI.geometryCount = 1;
		blasCI.buildFlags = AS_FLAG_PREFER_FAST_TRACE;

		phxRes = m_renderDevice.AllocateAccelerationStructure(blasCI, m_blas[i]);
		CHECK_PHX_RES(phxRes);
	}

	// Allocate TLAS
	AccelerationStructureCreateInfo tlasCI{};
	tlasCI.pName = "SceneTLAS";
	tlasCI.type = ACCELERATION_STRUCTURE_TYPE::TOP_LEVEL;
	tlasCI.maxInstanceCount = static_cast<u32>(m_pAsset->meshes.size());
	tlasCI.buildFlags = AS_FLAG_PREFER_FAST_TRACE;
	phxRes = m_renderDevice.AllocateAccelerationStructure(tlasCI, m_tlas);
	CHECK_PHX_RES(phxRes);

	// Allocate instance buffer
	BufferCreateInfo instanceBufferCI{};
	instanceBufferCI.pName = "SceneInstanceBuffer";
	instanceBufferCI.bufferUsage = BUFFER_USAGE::ACCELERATION_STRUCTURE_BUILD_INPUT;
	instanceBufferCI.sizeBytes = sizeof(PHX::AccelerationStructureInstance) * m_pAsset->meshes.size();
	phxRes = m_renderDevice.AllocateBuffer(instanceBufferCI, m_instanceBuffer);
	CHECK_PHX_RES(phxRes);

	// Build ASes by registering the passes here; the first Draw frame will execute them
	RenderPassHandle uploadGeometryPass;
	phxRes = m_renderGraph.RegisterPass("UploadGeometry", PASS_TYPE::TRANSFER, uploadGeometryPass);
	CHECK_PHX_RES(phxRes);

	uploadGeometryPass.SetBufferOutput(m_sceneVertexBuffer);
	uploadGeometryPass.SetBufferOutput(m_sceneIndexBuffer);
	uploadGeometryPass.SetBufferOutput(m_geometryInfoBuffer);
	uploadGeometryPass.SetBufferOutput(m_materialBuffer);
	for (u32 i = 0; i < m_sceneTextures.size(); i++)
	{
		uploadGeometryPass.SetTextureOutput(m_sceneTextures[i], ATTACHMENT_LOAD_OP::IGNORE, ATTACHMENT_STORE_OP::STORE, {});
	}
	uploadGeometryPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
	{
		deviceContext.CopyDataToBuffer(m_sceneVertexBuffer, m_pAsset->vertices.data(), sizeof(AssetVertex) * m_pAsset->vertices.size());
		deviceContext.CopyDataToBuffer(m_sceneIndexBuffer, m_pAsset->indices.data(), sizeof(Common::AssetIndexType) * m_pAsset->indices.size());

		struct GeometryInfo
		{
			PHX::u32 firstVertex;
			PHX::u32 firstIndex;
			PHX::u32 materialIndex;
			PHX::u32 padding;
		};
		std::vector<GeometryInfo> geometryInfos;
		geometryInfos.reserve(m_pAsset->meshes.size());
		for (const Mesh& mesh : m_pAsset->meshes)
		{
			GeometryInfo info{};
			info.firstVertex = mesh.firstVertex;
			info.firstIndex = mesh.firstIndex;
			info.materialIndex = mesh.materialIndex;
			geometryInfos.push_back(info);
		}
		deviceContext.CopyDataToBuffer(m_geometryInfoBuffer, geometryInfos.data(), sizeof(GeometryInfo) * geometryInfos.size());

		// Build and upload per-material texture index mapping
		std::vector<MaterialInfo> materialInfos;
		materialInfos.reserve(m_pAsset->materials.size());
		for (const Material& mat : m_pAsset->materials)
		{
			MaterialInfo matInfo{};
			for (u32 texIdx : mat.textureIndices)
			{
				if (texIdx >= m_textureIndexRemap.size())
					continue;

				Common::TEXTURE_TYPE texType = m_pAsset->textures[texIdx].type;
				u32 offsetIdx = m_textureIndexRemap[texIdx];

				switch (texType)
				{
				case Common::TEXTURE_TYPE::DIFFUSE:              matInfo.albedoTexIndex    = offsetIdx; break;
				case Common::TEXTURE_TYPE::NORMAL:               matInfo.normalTexIndex    = offsetIdx; break;
				case Common::TEXTURE_TYPE::SPECULAR:             matInfo.specularTexIndex  = offsetIdx; break;
				default: break;
				}
			}
			materialInfos.push_back(matInfo);
		}
		deviceContext.CopyDataToBuffer(m_materialBuffer, materialInfos.data(), sizeof(MaterialInfo) * materialInfos.size());

		// Upload default textures (1x1)
		for (u32 i = 0; i < s_defaultTextureCount; i++)
		{
			deviceContext.CopyDataToTexture(m_sceneTextures[i], &s_defaultTexturePixels[i], sizeof(u32), 0);
		}

		// Upload scene textures (use remap table — CreateSceneTextures may skip textures)
		for (u32 i = 0; i < static_cast<u32>(m_pAsset->textures.size()); i++)
		{
			const Common::TextureType& texSrc = m_pAsset->textures[i];
			u32 texHandleIdx = m_textureIndexRemap[i];
			if (texHandleIdx >= m_sceneTextures.size())
				continue;
			for (u32 mip = 0; mip < texSrc.mipLevels.size(); mip++)
			{
				deviceContext.CopyDataToTexture(m_sceneTextures[texHandleIdx], texSrc.mipLevels[mip].data.data(), texSrc.mipLevels[mip].dataSize, mip);
			}
		}
	});

	RenderPassHandle buildBLASPass;
	phxRes = m_renderGraph.RegisterPass("BuildBLAS", PASS_TYPE::AS_BUILD, buildBLASPass);
	CHECK_PHX_RES(phxRes);

	buildBLASPass.SetBufferInput(m_sceneVertexBuffer);
	buildBLASPass.SetBufferInput(m_sceneIndexBuffer);
	for (const PHX::AccelerationStructureHandle& blas : m_blas)
	{
		buildBLASPass.SetAccelerationStructureOutput(blas);
	}
	buildBLASPass.SetBufferOutput(m_instanceBuffer);
	buildBLASPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
	{
		for (const PHX::AccelerationStructureHandle& blas : m_blas)
		{
			deviceContext.BuildBottomLevelAccelerationStructure(blas);
		}

		// Compute a uniform scale from the original asset bounds so the scene fits in world space
		static constexpr float TARGET_SCENE_SIZE = 50.0f;
		PHX::Vec3f min(FLT_MAX);
		PHX::Vec3f max(-FLT_MAX);
		for (const AssetVertex& vert : m_pAsset->vertices)
		{
			const PHX::Vec3f& pos = vert.position;
			min.SetX(std::min(min.GetX(), pos.GetX()));
			min.SetY(std::min(min.GetY(), pos.GetY()));
			min.SetZ(std::min(min.GetZ(), pos.GetZ()));
			max.SetX(std::max(max.GetX(), pos.GetX()));
			max.SetY(std::max(max.GetY(), pos.GetY()));
			max.SetZ(std::max(max.GetZ(), pos.GetZ()));
		}

		const float extentX = max.GetX() - min.GetX();
		const float extentY = max.GetY() - min.GetY();
		const float extentZ = max.GetZ() - min.GetZ();
		const float maxExtent = std::max(extentX, std::max(extentY, extentZ));
		const float scale = (maxExtent > 0.0f) ? (TARGET_SCENE_SIZE / maxExtent) : 1.0f;

		const float centerX = (min.GetX() + max.GetX()) * 0.5f;

		std::vector<PHX::AccelerationStructureInstance> instances;
		instances.reserve(m_pAsset->meshes.size());
		for (u32 i = 0; i < static_cast<u32>(m_pAsset->meshes.size()); i++)
		{
			PHX::AccelerationStructureInstance instance{};
			// Coordinate system is already baked into the vertices at import.
			// Only uniform scale and X-centering are applied here.
			instance.transform[0][0] = scale;
			instance.transform[0][1] = 0.0f;
			instance.transform[0][2] = 0.0f;
			instance.transform[0][3] = -centerX * scale;
			instance.transform[1][0] = 0.0f;
			instance.transform[1][1] = scale;
			instance.transform[1][2] = 0.0f;
			instance.transform[1][3] = 0.0f;
			instance.transform[2][0] = 0.0f;
			instance.transform[2][1] = 0.0f;
			instance.transform[2][2] = scale;
			instance.transform[2][3] = 0.0f;
			instance.instanceCustomIndex = i;
			instance.mask = 0xFF;
			instance.instanceShaderBindingTableRecordOffset = 0;
			instance.flags = PHX::AS_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;

			instance.accelerationStructureReference = m_blas[i].GetDeviceAddress();

			instances.push_back(instance);
		}

		deviceContext.CopyDataToBuffer(m_instanceBuffer, instances.data(), sizeof(PHX::AccelerationStructureInstance) * instances.size());
	});

	RenderPassHandle buildTLASPass;
	phxRes = m_renderGraph.RegisterPass("BuildTLAS", PASS_TYPE::AS_BUILD, buildTLASPass);
	CHECK_PHX_RES(phxRes);

	buildTLASPass.SetBufferInput(m_instanceBuffer);
	for (const PHX::AccelerationStructureHandle& blas : m_blas)
	{
		buildTLASPass.SetAccelerationStructureInput(blas);
	}
	buildTLASPass.SetAccelerationStructureOutput(m_tlas);
	buildTLASPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
	{
		deviceContext.BuildTopLevelAccelerationStructure(m_tlas, m_instanceBuffer, static_cast<u32>(m_pAsset->meshes.size()));
	});
}

void RayTracingSample::UpdateCameraData(float dt)
{
	(void)dt;

	if (m_pCamera == nullptr)
	{
		return;
	}

	m_cameraData.viewInverse = m_pCamera->GetCameraMatrix();
	m_cameraData.projInverse = glm::inverse(m_projMatrix);
	m_cameraData.cameraPosition = m_pCamera->GetPosition();
	m_cameraData.viewport = glm::vec2(static_cast<float>(m_swapChain.GetWidth()), static_cast<float>(m_swapChain.GetHeight()));

	// Detect camera movement to reset accumulation
	if (m_rayTracingSupported)
	{
		glm::vec3 currentPos = m_pCamera->GetPosition();
		glm::vec3 currentForward = glm::vec3(m_cameraData.viewInverse[2]);

		if (glm::distance(currentPos, m_prevCameraPosition) > 0.001f ||
			glm::distance(currentForward, m_prevCameraForward) > 0.001f)
		{
			m_resetAccumulation = true;
		}

		m_prevCameraPosition = currentPos;
		m_prevCameraForward = currentForward;
	}
}

void RayTracingSample::OverrideSettings(Settings& settings)
{
	// Override version to allow ray tracing (>=1.2 in Vulkan)
	settings.backendAPIMajorVersion = 1;
	settings.backendAPIMinorVersion = 2;
}

void RayTracingSample::CreateUniformCollections()
{
	// Blit pipeline: combined image sampler for the ray tracing output
	UniformData blitImageData{};
	blitImageData.binding = 0;
	blitImageData.shaderStage = SHADER_STAGE_FLAG_FRAGMENT;
	blitImageData.type = UNIFORM_TYPE::COMBINED_IMAGE_SAMPLER;

	UniformDataGroup blitDataGroup{};
	blitDataGroup.set = 0;
	blitDataGroup.uniformArray = &blitImageData;
	blitDataGroup.uniformArrayCount = 1;

	UniformCollectionCreateInfo blitUniformCollectionCI{};
	blitUniformCollectionCI.dataGroups = &blitDataGroup;
	blitUniformCollectionCI.groupCount = 1;

	STATUS_CODE phxRes = m_renderDevice.AllocateUniformCollection(blitUniformCollectionCI, m_blitUniformCollection);
	CHECK_PHX_RES(phxRes);

	if (!m_rayTracingSupported)
	{
		return;
	}

	// Ray tracing pipeline: output image, TLAS, scene buffers, material buffer, env cube map, accumulation image
	UniformData rtData[8];
	rtData[0].binding = 0;
	rtData[0].shaderStage = SHADER_STAGE_FLAG_RAYGEN;
	rtData[0].type = UNIFORM_TYPE::STORAGE_IMAGE;
	rtData[1].binding = 1;
	rtData[1].shaderStage = SHADER_STAGE_FLAG_RAYGEN | SHADER_STAGE_FLAG_CLOSEST_HIT;
	rtData[1].type = UNIFORM_TYPE::ACCELERATION_STRUCTURE;
	rtData[2].binding = 2;
	rtData[2].shaderStage = SHADER_STAGE_FLAG_CLOSEST_HIT | SHADER_STAGE_FLAG_ANY_HIT;
	rtData[2].type = UNIFORM_TYPE::STORAGE_BUFFER;
	rtData[3].binding = 3;
	rtData[3].shaderStage = SHADER_STAGE_FLAG_CLOSEST_HIT | SHADER_STAGE_FLAG_ANY_HIT;
	rtData[3].type = UNIFORM_TYPE::STORAGE_BUFFER;
	rtData[4].binding = 4;
	rtData[4].shaderStage = SHADER_STAGE_FLAG_CLOSEST_HIT | SHADER_STAGE_FLAG_ANY_HIT;
	rtData[4].type = UNIFORM_TYPE::STORAGE_BUFFER;
	rtData[5].binding = 5;
	rtData[5].shaderStage = SHADER_STAGE_FLAG_CLOSEST_HIT | SHADER_STAGE_FLAG_ANY_HIT;
	rtData[5].type = UNIFORM_TYPE::STORAGE_BUFFER;
	rtData[6].binding = 6;
	rtData[6].shaderStage = SHADER_STAGE_FLAG_MISS | SHADER_STAGE_FLAG_CLOSEST_HIT;
	rtData[6].type = UNIFORM_TYPE::COMBINED_IMAGE_SAMPLER;
	rtData[7].binding = 7;
	rtData[7].shaderStage = SHADER_STAGE_FLAG_RAYGEN;
	rtData[7].type = UNIFORM_TYPE::STORAGE_IMAGE;

	UniformData cameraData{};
	cameraData.binding = 0;
	cameraData.shaderStage = SHADER_STAGE_FLAG_RAYGEN;
	cameraData.type = UNIFORM_TYPE::UNIFORM_BUFFER;

	// Set 2: Scene textures (single descriptor array binding, only if textures exist)
	const u32 texCount = static_cast<u32>(m_sceneTextures.size());
	std::vector<UniformData> texUniforms;
	UniformDataGroup texDataGroup{};
	texDataGroup.set = 2;

	if (texCount > 0)
	{
		texUniforms.resize(1);
		texUniforms[0].binding = 0;
		texUniforms[0].shaderStage = SHADER_STAGE_FLAG_CLOSEST_HIT | SHADER_STAGE_FLAG_ANY_HIT;
		texUniforms[0].type = UNIFORM_TYPE::COMBINED_IMAGE_SAMPLER;
		texUniforms[0].count = texCount;

		texDataGroup.uniformArray = texUniforms.data();
		texDataGroup.uniformArrayCount = 1;
	}

	UniformDataGroup rtDataGroups[3];
	rtDataGroups[0].set = 0;
	rtDataGroups[0].uniformArray = rtData;
	rtDataGroups[0].uniformArrayCount = 8;
	rtDataGroups[1].set = 1;
	rtDataGroups[1].uniformArray = &cameraData;
	rtDataGroups[1].uniformArrayCount = 1;
	rtDataGroups[2] = texDataGroup;

	const u32 groupCount = (texCount > 0) ? 3 : 2;

	UniformCollectionCreateInfo rtUniformCollectionCI{};
	rtUniformCollectionCI.dataGroups = rtDataGroups;
	rtUniformCollectionCI.groupCount = groupCount;

	phxRes = m_renderDevice.AllocateUniformCollection(rtUniformCollectionCI, m_rayTracingUniformCollection);
	CHECK_PHX_RES(phxRes);
}

void RayTracingSample::UploadBlitVertices()
{
	RenderPassHandle uploadPass;
	STATUS_CODE phxRes = m_renderGraph.RegisterPass("UploadBlitVertices", PASS_TYPE::TRANSFER, uploadPass);
	CHECK_PHX_RES(phxRes);

	uploadPass.SetBufferOutput(m_blitVertexBuffer);
	uploadPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
	{
		deviceContext.CopyDataToBuffer(m_blitVertexBuffer, s_blitTriangle, sizeof(BlitVertex) * 3);
	});
}

void RayTracingSample::LoadEnvironmentMap()
{
	STATUS_CODE phxRes;

	// Load HDR image using common utility (resolves asset root paths)
	std::filesystem::path hdrPath = Common::FindAssetFile("Bistro_v5_2/san_giuseppe_bridge_4k.hdr");
	if (hdrPath.empty())
	{
		std::cout << "Failed to find HDR environment map!" << std::endl;
		return;
	}

	Common::AssetDiskTexture hdrTex = Common::LoadHDRTexture(hdrPath);
	if (hdrTex.pData == nullptr)
	{
		std::cout << "Failed to load HDR environment map!" << std::endl;
		return;
	}

	const u32 width = hdrTex.size.GetX();
	const u32 height = hdrTex.size.GetY();

	// Create equirectangular 2D texture
	TextureBaseCreateInfo equirectBaseCI{};
	equirectBaseCI.pName = "EquirectEnvMap";
	equirectBaseCI.width = width;
	equirectBaseCI.height = height;
	equirectBaseCI.mipLevels = 1;
	equirectBaseCI.generateMips = false;
	equirectBaseCI.arrayLayers = 1;
	equirectBaseCI.format = BASE_FORMAT::R16G16B16A16_FLOAT;
	equirectBaseCI.usageFlags = USAGE_TYPE_FLAG_SAMPLED | USAGE_TYPE_FLAG_TRANSFER_DST;
	equirectBaseCI.sampleFlags = SAMPLE_COUNT::COUNT_1;

	TextureViewCreateInfo equirectViewCI{};
	equirectViewCI.type = VIEW_TYPE::TYPE_2D;
	equirectViewCI.scope = VIEW_SCOPE::ENTIRE;
	equirectViewCI.aspectFlags = ASPECT_TYPE_FLAG_COLOR;

	TextureSamplerCreateInfo equirectSamplerCI{};
	equirectSamplerCI.addressModeUVW = SAMPLER_ADDRESS_MODE::CLAMP_TO_EDGE;
	equirectSamplerCI.enableAnisotropicFiltering = false;
	equirectSamplerCI.magnificationFilter = FILTER_MODE::LINEAR;
	equirectSamplerCI.minificationFilter = FILTER_MODE::LINEAR;
	equirectSamplerCI.samplerMipMapFilter = FILTER_MODE::LINEAR;

	phxRes = m_renderDevice.AllocateTexture(equirectBaseCI, equirectViewCI, equirectSamplerCI, m_equirectTexture);
	if (phxRes != STATUS_CODE::SUCCESS)
	{
		std::cout << "Failed to allocate equirectangular texture!" << std::endl;
		Common::FreeTextureData(hdrTex);
		return;
	}

	// Convert float32 to float16 for upload (R16G16B16A16_FLOAT)
	const float* pFloatData = static_cast<const float*>(hdrTex.pData);
	const u32 pixelCount = width * height;
	std::vector<uint16_t> halfData(pixelCount * 4);
	for (u32 i = 0; i < pixelCount * 4; i++)
	{
		halfData[i] = glm::packHalf1x16(pFloatData[i]);
	}

	const u64 dataSize = static_cast<u64>(pixelCount) * 4 * sizeof(uint16_t);

	// Free the HDR float data now that we've converted to half-float
	Common::FreeTextureData(hdrTex);

	// Create cube map texture (512x512 per face)
	constexpr u32 CUBE_FACE_SIZE = 512;
	TextureBaseCreateInfo cubeBaseCI{};
	cubeBaseCI.pName = "EnvCubeMap";
	cubeBaseCI.width = CUBE_FACE_SIZE;
	cubeBaseCI.height = CUBE_FACE_SIZE;
	cubeBaseCI.mipLevels = 1;
	cubeBaseCI.generateMips = false;
	cubeBaseCI.arrayLayers = 6;
	cubeBaseCI.format = BASE_FORMAT::R16G16B16A16_FLOAT;
	cubeBaseCI.usageFlags = USAGE_TYPE_FLAG_STORAGE | USAGE_TYPE_FLAG_SAMPLED;
	cubeBaseCI.sampleFlags = SAMPLE_COUNT::COUNT_1;

	TextureViewCreateInfo cubeViewCI{};
	cubeViewCI.type = VIEW_TYPE::TYPE_CUBE;
	cubeViewCI.scope = VIEW_SCOPE::ENTIRE;
	cubeViewCI.aspectFlags = ASPECT_TYPE_FLAG_COLOR;

	TextureSamplerCreateInfo cubeSamplerCI{};
	cubeSamplerCI.addressModeUVW = SAMPLER_ADDRESS_MODE::CLAMP_TO_EDGE;
	cubeSamplerCI.enableAnisotropicFiltering = false;
	cubeSamplerCI.magnificationFilter = FILTER_MODE::LINEAR;
	cubeSamplerCI.minificationFilter = FILTER_MODE::LINEAR;
	cubeSamplerCI.samplerMipMapFilter = FILTER_MODE::LINEAR;

	phxRes = m_renderDevice.AllocateTexture(cubeBaseCI, cubeViewCI, cubeSamplerCI, m_envCubeMap);
	if (phxRes != STATUS_CODE::SUCCESS)
	{
		std::cout << "Failed to allocate cube map texture!" << std::endl;
		return;
	}

	// Create uniform buffer for cube face size
	struct CubeFaceSizeUBO
	{
		u32 faceSizeX;
		u32 faceSizeY;
		u32 padding[2];
	} faceSizeData{ CUBE_FACE_SIZE, CUBE_FACE_SIZE, 0, 0 };

	BufferCreateInfo faceSizeBufferCI{};
	faceSizeBufferCI.pName = "CubeFaceSizeBuffer";
	faceSizeBufferCI.bufferUsage = BUFFER_USAGE::UNIFORM_BUFFER;
	faceSizeBufferCI.sizeBytes = sizeof(CubeFaceSizeUBO);
	phxRes = m_renderDevice.AllocateBuffer(faceSizeBufferCI, m_cubeFaceSizeBuffer);
	CHECK_PHX_RES(phxRes);

	// Create equirect-to-cube compute shader
	ShaderHandle equirectToCubeShader;
	equirectToCubeShader = m_pShaderManager->RegisterShader("../src/shaders/equirect_to_cube.comp", SHADER_STAGE::COMPUTE, m_renderDevice);
	if (!equirectToCubeShader.IsValid())
	{
		std::cout << "Failed to load equirect_to_cube compute shader!" << std::endl;
		return;
	}
	m_equirectToCubeShaders.push_back(equirectToCubeShader);

	// Create uniform collection for equirect-to-cube pass
	UniformData equirectToCubeData[3];
	equirectToCubeData[0].binding = 0;
	equirectToCubeData[0].shaderStage = SHADER_STAGE_FLAG_COMPUTE;
	equirectToCubeData[0].type = UNIFORM_TYPE::COMBINED_IMAGE_SAMPLER;
	equirectToCubeData[1].binding = 1;
	equirectToCubeData[1].shaderStage = SHADER_STAGE_FLAG_COMPUTE;
	equirectToCubeData[1].type = UNIFORM_TYPE::STORAGE_IMAGE;
	equirectToCubeData[2].binding = 2;
	equirectToCubeData[2].shaderStage = SHADER_STAGE_FLAG_COMPUTE;
	equirectToCubeData[2].type = UNIFORM_TYPE::UNIFORM_BUFFER;

	UniformDataGroup equirectToCubeGroup{};
	equirectToCubeGroup.set = 0;
	equirectToCubeGroup.uniformArray = equirectToCubeData;
	equirectToCubeGroup.uniformArrayCount = 3;

	UniformCollectionCreateInfo equirectToCubeUniformCI{};
	equirectToCubeUniformCI.dataGroups = &equirectToCubeGroup;
	equirectToCubeUniformCI.groupCount = 1;

	phxRes = m_renderDevice.AllocateUniformCollection(equirectToCubeUniformCI, m_equirectToCubeUniformCollection);
	CHECK_PHX_RES(phxRes);

	// Set up compute pipeline desc
	m_equirectToCubePipelineDesc.shader = m_equirectToCubeShaders[0];
	m_equirectToCubePipelineDesc.uniformCollection = m_equirectToCubeUniformCollection;

	// Register passes: upload equirect data, then run equirect-to-cube compute
	RenderPassHandle uploadEquirectPass;
	phxRes = m_renderGraph.RegisterPass("UploadEquirect", PASS_TYPE::TRANSFER, uploadEquirectPass);
	CHECK_PHX_RES(phxRes);

	uploadEquirectPass.SetTextureOutput(m_equirectTexture, ATTACHMENT_LOAD_OP::IGNORE, ATTACHMENT_STORE_OP::STORE, {});
	uploadEquirectPass.SetBufferOutput(m_cubeFaceSizeBuffer);
	uploadEquirectPass.SetExecuteCallback([&, halfData, dataSize, faceSizeData](DeviceContextHandle deviceContext)
	{
		deviceContext.CopyDataToTexture(m_equirectTexture, halfData.data(), dataSize, 0);
		deviceContext.CopyDataToBuffer(m_cubeFaceSizeBuffer, &faceSizeData, sizeof(faceSizeData));
	});

	RenderPassHandle equirectToCubePass;
	phxRes = m_renderGraph.RegisterPass("EquirectToCube", PASS_TYPE::COMPUTE, equirectToCubePass);
	CHECK_PHX_RES(phxRes);

	equirectToCubePass.SetTextureInput(m_equirectTexture);
	equirectToCubePass.SetTextureOutput(m_envCubeMap, ATTACHMENT_LOAD_OP::IGNORE, ATTACHMENT_STORE_OP::STORE, {});
	equirectToCubePass.SetBufferInput(m_cubeFaceSizeBuffer);
	equirectToCubePass.SetPipelineDescription(m_equirectToCubePipelineDesc);
	equirectToCubePass.SetExecuteCallback([&, CUBE_FACE_SIZE](DeviceContextHandle deviceContext)
	{
		m_equirectToCubeUniformCollection.QueueImageUpdate(m_equirectTexture, 0, 0, 0);
		m_equirectToCubeUniformCollection.QueueImageUpdate(m_envCubeMap, 0, 1, 0);
		m_equirectToCubeUniformCollection.QueueBufferUpdate(m_cubeFaceSizeBuffer, 0, 2, 0);
		deviceContext.FlushUniformUpdates(m_equirectToCubeUniformCollection);

		deviceContext.BindUniformCollection(m_equirectToCubeUniformCollection);
		deviceContext.Dispatch({ CUBE_FACE_SIZE, CUBE_FACE_SIZE, 6 });
	});
}

void RayTracingSample::CreateAccumulationImages()
{
	STATUS_CODE phxRes;

	TextureBaseCreateInfo accumBaseCI{};
	accumBaseCI.pName = "AccumulationImageA";
	accumBaseCI.width = m_swapChain.GetWidth();
	accumBaseCI.height = m_swapChain.GetHeight();
	accumBaseCI.mipLevels = 1;
	accumBaseCI.generateMips = false;
	accumBaseCI.arrayLayers = 1;
	accumBaseCI.format = BASE_FORMAT::R16G16B16A16_FLOAT;
	accumBaseCI.usageFlags = USAGE_TYPE_FLAG_STORAGE | USAGE_TYPE_FLAG_SAMPLED;
	accumBaseCI.sampleFlags = SAMPLE_COUNT::COUNT_1;

	TextureViewCreateInfo accumViewCI{};
	accumViewCI.type = VIEW_TYPE::TYPE_2D;
	accumViewCI.scope = VIEW_SCOPE::ENTIRE;
	accumViewCI.aspectFlags = ASPECT_TYPE_FLAG_COLOR;

	TextureSamplerCreateInfo accumSamplerCI{};
	accumSamplerCI.addressModeUVW = SAMPLER_ADDRESS_MODE::CLAMP_TO_EDGE;
	accumSamplerCI.enableAnisotropicFiltering = false;
	accumSamplerCI.magnificationFilter = FILTER_MODE::NEAREST;
	accumSamplerCI.minificationFilter = FILTER_MODE::NEAREST;
	accumSamplerCI.samplerMipMapFilter = FILTER_MODE::NEAREST;

	phxRes = m_renderDevice.AllocateTexture(accumBaseCI, accumViewCI, accumSamplerCI, m_accumulationImageA);
	CHECK_PHX_RES(phxRes);
}
