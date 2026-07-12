
#include <array>
#include <cfloat>
#include <iostream>

#include "ray_tracing_sample.h"

#include <gtc/matrix_transform.hpp>

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
	bool shouldClose = BaseSample::Update(dt);
	UpdateCameraData(dt);
	return shouldClose;
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
		rayTracingPass.SetAccelerationStructureInput(m_tlas);
		rayTracingPass.SetBufferInput(m_sceneVertexBuffer);
		rayTracingPass.SetBufferInput(m_sceneIndexBuffer);
		rayTracingPass.SetBufferInput(m_geometryInfoBuffer);
		for (u32 i = 0; i < m_sceneTextures.size(); i++)
		{
			rayTracingPass.SetTextureInput(m_sceneTextures[i]);
		}
		rayTracingPass.SetPipelineDescription(m_rayTracingPipelineDesc);
		rayTracingPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
		{
			deviceContext.CopyDataToBuffer(m_cameraUniformBuffer, &m_cameraData, sizeof(CameraData));

			if (!m_texturesBound)
			{
				for (u32 i = 0; i < m_sceneTextures.size(); i++)
				{
					m_rayTracingUniformCollection.QueueImageUpdate(m_sceneTextures[i], 2, 0, 0, i);
				}
				m_texturesBound = true;
			}

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

	m_pCamera = new Common::FreeflyCamera(2.5f, 0.15f, glm::vec3(0.0f, 5.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f));

	m_projMatrix = glm::perspective(glm::radians(60.0f), static_cast<float>(m_swapChain.GetWidth()) / static_cast<float>(m_swapChain.GetHeight()), 0.1f, 1000.0f);
	m_projMatrix[1][1] *= -1.0f;

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
	CreateSceneTextures();
	CreateSceneGeometryBuffers();
	if (m_rayTracingSupported)
	{
		BuildSceneAccelerationStructures();
	}

	// UNIFORM COLLECTIONS
	CreateUniformCollections();

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

	// RAY TRACING SHADERS + PIPELINE
	if (m_rayTracingSupported)
	{
		ShaderHandle rayGenShader;
		if (!Common::AllocateShader("../src/shaders/raygen.rgen", SHADER_STAGE::RAYGEN, m_renderDevice, rayGenShader))
		{
			return;
		}
		m_rayTracingPipelineShaders.push_back(rayGenShader);

		ShaderHandle missShader;
		if (!Common::AllocateShader("../src/shaders/miss.rmiss", SHADER_STAGE::MISS, m_renderDevice, missShader))
		{
			return;
		}
		m_rayTracingPipelineShaders.push_back(missShader);

		ShaderHandle closestHitShader;
		if (!Common::AllocateShader("../src/shaders/closesthit.rchit", SHADER_STAGE::CLOSEST_HIT, m_renderDevice, closestHitShader))
		{
			return;
		}
		m_rayTracingPipelineShaders.push_back(closestHitShader);

		m_rayTracingPipelineDesc.pShaders = m_rayTracingPipelineShaders.data();
		m_rayTracingPipelineDesc.shaderCount = static_cast<u32>(m_rayTracingPipelineShaders.size());
		m_rayTracingPipelineDesc.uniformCollection = m_rayTracingUniformCollection;
	}

	UploadBlitVertices();
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
	std::shared_ptr<Common::AssetDisk> pAssetDisk = Common::ImportAsset("../assets/Bistro_v5_2/BistroExterior.fbx");
	if (pAssetDisk == nullptr)
	{
		std::cout << "Failed to import Bistro assets!" << std::endl;
		return;
	}

	m_assetHandle = ConvertAssetDiskToAssetType(pAssetDisk.get());
	m_pAsset = AssetManager::Get().GetAsset(m_assetHandle);
	if (m_pAsset == nullptr)
	{
		std::cout << "Failed to convert Bistro assets!" << std::endl;
	}
}

void RayTracingSample::CreateSceneTextures()
{
	if (m_pAsset == nullptr)
	{
		return;
	}

	for (u32 i = 0; i < static_cast<u32>(m_pAsset->textures.size()); i++)
	{
		const Texture& currTex = m_pAsset->textures[i];

		const u32 numMips = static_cast<u32>(currTex.mipLevels.size());
		if (numMips == 0)
		{
			continue;
		}

		TextureBaseCreateInfo baseCI{};
		baseCI.pName = currTex.pName;
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
		CHECK_PHX_RES(res);

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
		PHX::u32 textureIndex;
	};
	std::vector<GeometryInfo> geometryInfos;
	geometryInfos.reserve(m_pAsset->meshes.size());
	for (const Mesh& mesh : m_pAsset->meshes)
	{
		GeometryInfo info{};
		info.firstVertex = mesh.firstVertex;
		info.firstIndex = mesh.firstIndex;
		info.materialIndex = mesh.materialIndex;
		info.textureIndex = m_pAsset->materials[mesh.materialIndex].textureIndices.empty() ? 0 : m_pAsset->materials[mesh.materialIndex].textureIndices[0];
		geometryInfos.push_back(info);
	}

	BufferCreateInfo gBufferCI{};
	gBufferCI.pName = "GeometryInfoBuffer";
	gBufferCI.bufferUsage = BUFFER_USAGE::STORAGE_BUFFER;
	gBufferCI.sizeBytes = sizeof(GeometryInfo) * geometryInfos.size();
	phxRes = m_renderDevice.AllocateBuffer(gBufferCI, m_geometryInfoBuffer);
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
	phxRes = m_renderGraph.RegisterPass("UploadGeometry", BIND_POINT::TRANSFER, uploadGeometryPass);
	CHECK_PHX_RES(phxRes);

	uploadGeometryPass.SetBufferOutput(m_sceneVertexBuffer);
	uploadGeometryPass.SetBufferOutput(m_sceneIndexBuffer);
	uploadGeometryPass.SetBufferOutput(m_geometryInfoBuffer);
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
			PHX::u32 textureIndex;
		};
		std::vector<GeometryInfo> geometryInfos;
		geometryInfos.reserve(m_pAsset->meshes.size());
		for (const Mesh& mesh : m_pAsset->meshes)
		{
			GeometryInfo info{};
			info.firstVertex = mesh.firstVertex;
			info.firstIndex = mesh.firstIndex;
			info.materialIndex = mesh.materialIndex;
			info.textureIndex = m_pAsset->materials[mesh.materialIndex].textureIndices.empty() ? 0 : m_pAsset->materials[mesh.materialIndex].textureIndices[0];
			geometryInfos.push_back(info);
		}
		deviceContext.CopyDataToBuffer(m_geometryInfoBuffer, geometryInfos.data(), sizeof(GeometryInfo) * geometryInfos.size());

		for (u32 i = 0; i < m_sceneTextures.size(); i++)
		{
			const Texture& texSrc = m_pAsset->textures[i];
			for (u32 mip = 0; mip < texSrc.mipLevels.size(); mip++)
			{
				deviceContext.CopyDataToTexture(m_sceneTextures[i], texSrc.mipLevels[mip].data, texSrc.mipLevels[mip].dataSize, mip);
			}
		}
	});

	RenderPassHandle buildBLASPass;
	phxRes = m_renderGraph.RegisterPass("BuildBLAS", BIND_POINT::TRANSFER, buildBLASPass);
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
			instance.flags = PHX::AS_INSTANCE_FLAG_TRIANGLE_FACING_CULL_DISABLE;

			instance.accelerationStructureReference = m_blas[i].GetDeviceAddress();

			instances.push_back(instance);
		}

		deviceContext.CopyDataToBuffer(m_instanceBuffer, instances.data(), sizeof(PHX::AccelerationStructureInstance) * instances.size());
	});

	RenderPassHandle buildTLASPass;
	phxRes = m_renderGraph.RegisterPass("BuildTLAS", BIND_POINT::TRANSFER, buildTLASPass);
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
	blitImageData.shaderStage = SHADER_STAGE::FRAGMENT;
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

	// Ray tracing pipeline: output image, TLAS, scene buffers and camera
	UniformData rtData[5];
	rtData[0].binding = 0;
	rtData[0].shaderStage = SHADER_STAGE::RAYGEN;
	rtData[0].type = UNIFORM_TYPE::STORAGE_IMAGE;
	rtData[1].binding = 1;
	rtData[1].shaderStage = SHADER_STAGE::RAYGEN;
	rtData[1].type = UNIFORM_TYPE::ACCELERATION_STRUCTURE;
	rtData[2].binding = 2;
	rtData[2].shaderStage = SHADER_STAGE::CLOSEST_HIT;
	rtData[2].type = UNIFORM_TYPE::STORAGE_BUFFER;
	rtData[3].binding = 3;
	rtData[3].shaderStage = SHADER_STAGE::CLOSEST_HIT;
	rtData[3].type = UNIFORM_TYPE::STORAGE_BUFFER;
	rtData[4].binding = 4;
	rtData[4].shaderStage = SHADER_STAGE::CLOSEST_HIT;
	rtData[4].type = UNIFORM_TYPE::STORAGE_BUFFER;

	UniformData cameraData{};
	cameraData.binding = 0;
	cameraData.shaderStage = SHADER_STAGE::RAYGEN;
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
		texUniforms[0].shaderStage = SHADER_STAGE::CLOSEST_HIT;
		texUniforms[0].type = UNIFORM_TYPE::COMBINED_IMAGE_SAMPLER;
		texUniforms[0].count = texCount;

		texDataGroup.uniformArray = texUniforms.data();
		texDataGroup.uniformArrayCount = 1;
	}

	UniformDataGroup rtDataGroups[3];
	rtDataGroups[0].set = 0;
	rtDataGroups[0].uniformArray = rtData;
	rtDataGroups[0].uniformArrayCount = 5;
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

	m_rayTracingUniformCollection.QueueAccelerationStructureUpdate(m_tlas, 0, 1);
	m_rayTracingUniformCollection.QueueBufferUpdate(m_sceneVertexBuffer, 0, 2, 0);
	m_rayTracingUniformCollection.QueueBufferUpdate(m_sceneIndexBuffer, 0, 3, 0);
	m_rayTracingUniformCollection.QueueBufferUpdate(m_geometryInfoBuffer, 0, 4, 0);
	m_rayTracingUniformCollection.QueueBufferUpdate(m_cameraUniformBuffer, 1, 0, 0);
	m_rayTracingUniformCollection.FlushUpdateQueue();
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
