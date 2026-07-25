
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>
#include <imgui.h>
#include <random>
#include <sstream>

#include "instanced_animation_sample.h"

#include "../../common/src/camera/freefly_camera.h"
#include "../../common/src/utils/shader_utils.h"

using namespace PHX;

#define CHECK_PHX_RES(phxRes) if(phxRes != PHX::STATUS_CODE::SUCCESS) { return; }

static constexpr uint32_t MAX_INSTANCES = 10000;

InstancedAnimationSample::InstancedAnimationSample()
	: m_assetID(Common::INVALID_ASSET_HANDLE)
	, m_instanceDataDirty(true)
	, m_globalTime(0.0f)
	, m_paused(false)
	, m_animSpeed(1.0f)
	, m_randomSeed(42)
	, m_activeClipIndex(0)
	, m_clipMode(ClipAssignmentMode::ROUND_ROBIN)
	, m_instanceCount(MAX_INSTANCES)
	, m_lastFrameTime(0.0f)
{
}

InstancedAnimationSample::~InstancedAnimationSample()
{
}

void InstancedAnimationSample::UpdateSample(float dt)
{
	m_lastFrameTime = dt;

	// Advance global animation time
	if (!m_paused)
	{
		m_globalTime += dt * m_animSpeed;
	}

	// Update camera
	m_cameraData.view = m_pCamera->GetViewMatrix();

	// ImGui
	m_imguiBackend.NewFrame(dt, m_swapChain.GetWidth(), m_swapChain.GetHeight());
	BuildImGuiUI();
}

void InstancedAnimationSample::Draw()
{
	const AssetType* asset = AssetManager::Get().GetAsset(m_assetID);
	if (asset == nullptr)
	{
		return;
	}

	STATUS_CODE phxRes;

	ClearValues clearColor{};
	clearColor.color.color = Vec4f(0.2f, 0.2f, 0.25f, 1.0f);
	clearColor.useClearColor = true;

	ClearValues clearDepth{};
	clearDepth.depthStencil.depthClear = 1.0f;
	clearDepth.depthStencil.stencilClear = 0;
	clearDepth.useClearColor = false;

	std::array<ClearValues, 2> clearVals = { clearColor, clearDepth };

	m_renderGraph.BeginFrame(m_swapChain);

	// Re-upload instance data if dirty
	if (m_instanceDataDirty)
	{
		RenderPassHandle transferPass;
		phxRes = m_renderGraph.RegisterPass("InstanceDataUpload", PASS_TYPE::TRANSFER, transferPass);
		CHECK_PHX_RES(phxRes);

		transferPass.SetBufferOutput(m_instanceBuffer);

		transferPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
		{
			deviceContext.CopyDataToBuffer(m_instanceBuffer, m_instances.data(), m_instanceCount * sizeof(InstanceData));
		});

		m_instanceDataDirty = false;
	}

	// COMPUTE PASS — evaluate bone matrices
	{
		RenderPassHandle computePass;
		phxRes = m_renderGraph.RegisterPass("AnimationCompute", PASS_TYPE::COMPUTE, computePass);
		CHECK_PHX_RES(phxRes);

		computePass.SetBufferInput(m_skeletonBuffer);
		computePass.SetBufferInput(m_keyframeBuffer);
		computePass.SetBufferInput(m_clipInfoBuffer);
		computePass.SetBufferInput(m_channelInfoBuffer);
		computePass.SetBufferInput(m_instanceBuffer);
		computePass.SetBufferInput(m_nodeParentBuffer);
		computePass.SetBufferInput(m_nodeTransformBuffer);
		computePass.SetBufferOutput(m_boneMatrixBuffer);
		computePass.SetPipelineDescription(m_computePipelineDesc);

		uint32_t boneCount = static_cast<uint32_t>(asset->skeleton.bones.size());
		uint32_t totalThreads = m_instanceCount * boneCount;

		computePass.SetExecuteCallback([&, boneCount, totalThreads](DeviceContextHandle deviceContext)
		{
			AnimParamsData params{};
			params.globalTime = m_globalTime;
			params.boneCount = boneCount;
			params.instanceCount = m_instanceCount;
			deviceContext.CopyDataToBuffer(m_animParamsBuffer, &params, sizeof(AnimParamsData));

			m_computeUniformCollection.QueueBufferUpdate(m_skeletonBuffer, 0, 0, 0);
			m_computeUniformCollection.QueueBufferUpdate(m_keyframeBuffer, 0, 1, 0);
			m_computeUniformCollection.QueueBufferUpdate(m_clipInfoBuffer, 0, 2, 0);
			m_computeUniformCollection.QueueBufferUpdate(m_channelInfoBuffer, 0, 3, 0);
			m_computeUniformCollection.QueueBufferUpdate(m_instanceBuffer, 0, 4, 0);
			m_computeUniformCollection.QueueBufferUpdate(m_boneMatrixBuffer, 0, 5, 0);
			m_computeUniformCollection.QueueBufferUpdate(m_animParamsBuffer, 0, 6, 0);
			m_computeUniformCollection.QueueBufferUpdate(m_nodeParentBuffer, 0, 7, 0);
			m_computeUniformCollection.QueueBufferUpdate(m_nodeTransformBuffer, 0, 8, 0);
			deviceContext.FlushUniformUpdates(m_computeUniformCollection);

			deviceContext.BindUniformCollection(m_computeUniformCollection);
			deviceContext.Dispatch(Vec3u(totalThreads, 1, 1));
		});
	}

	// GRAPHICS PASS — render instanced skinned models
	{
		RenderPassHandle graphicsPass;
		phxRes = m_renderGraph.RegisterPass("ModelRender", PASS_TYPE::GRAPHICS, graphicsPass);
		CHECK_PHX_RES(phxRes);

		graphicsPass.SetTextureOutput(m_swapChain.GetCurrentImage(), ATTACHMENT_LOAD_OP::CLEAR, ATTACHMENT_STORE_OP::STORE, clearColor);
		graphicsPass.SetDepthOutput(m_depthBuffer);

		graphicsPass.SetBufferInput(m_boneMatrixBuffer);
		graphicsPass.SetBufferInput(m_instanceBuffer);

		for (TextureHandle assetTex : m_assetTextures)
		{
			graphicsPass.SetTextureInput(assetTex);
		}

		graphicsPass.SetPipelineDescription(m_graphicsPipelineDesc);

		uint32_t indexCount = static_cast<uint32_t>(asset->indices.size());

		graphicsPass.SetExecuteCallback([&, indexCount](DeviceContextHandle deviceContext)
		{
			// Update camera UBO
			m_cameraData.boneCount = static_cast<uint32_t>(asset->skeleton.bones.size());
			deviceContext.CopyDataToBuffer(m_cameraBuffer, &m_cameraData, sizeof(CameraData));

			m_graphicsUniformCollection.QueueBufferUpdate(m_boneMatrixBuffer, 0, 0, 0);
			m_graphicsUniformCollection.QueueBufferUpdate(m_instanceBuffer, 0, 1, 0);
			m_graphicsUniformCollection.QueueBufferUpdate(m_cameraBuffer, 0, 2, 0);

			for (u32 i = 0; i < m_assetTextures.size(); i++)
			{
				m_graphicsUniformCollection.QueueImageUpdate(m_assetTextures[i], 1, i, 0);
			}

			deviceContext.FlushUniformUpdates(m_graphicsUniformCollection);

			deviceContext.BindUniformCollection(m_graphicsUniformCollection);
			deviceContext.BindMesh(m_vertexBuffer, m_indexBuffer);
			deviceContext.SetScissor({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
			deviceContext.SetViewport({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
			deviceContext.DrawIndexedInstanced(indexCount, m_instanceCount, 0, 0, 0);
		});
	}

	// IMGUI PASS
	ImGui::Render();
	m_imguiRenderer.RenderDrawData(m_renderGraph, m_swapChain, ImGui::GetDrawData(), false);

	m_renderGraph.Bake(m_swapChain);

	// Viz
	{
		const u32 frameNumber = m_renderGraph.GetFrameNumber();
		const u32 nameLen = 64;
		char renderGraphVisName[nameLen];
		snprintf(renderGraphVisName, nameLen, "./InstancedAnimation_RG_%u.dot", frameNumber);
		m_renderGraph.GenerateVisualization(renderGraphVisName);
	}

	m_renderGraph.EndFrame(m_swapChain);
}

void InstancedAnimationSample::InitSample()
{
	STATUS_CODE phxRes;

	m_window.SetWindowTitle("PHX %u.%u.%u | INSTANCED ANIMATION", PHX::GetMajorVersion(), PHX::GetMinorVersion(), PHX::GetPatchVersion());

	// ImGui
	if (!m_imguiBackend.Init())
	{
		return;
	}
	if (!m_imguiRenderer.Init(m_renderDevice, m_swapChain, m_pShaderManager))
	{
		return;
	}

	// LOAD MODEL
	m_assetID = AssetManager::Get().LoadOrImport("wolf_model/source/WOLF_DEMO.fbx", false, true);
	if (m_assetID == Common::INVALID_ASSET_HANDLE)
	{
		return;
	}

	const AssetType* asset = AssetManager::Get().GetAsset(m_assetID);
	const uint32_t boneCount = static_cast<uint32_t>(asset->skeleton.bones.size());
	std::cout << "[SAMPLE] Loaded model: " << asset->vertices.size() << " vertices, "
	          << asset->indices.size() << " indices, " << boneCount << " bones, "
	          << asset->animations.size() << " animations" << std::endl;

	// CAMERA
	const float cameraSpeed = 20.0f;
	const float cameraSensitivity = 0.2f;
	m_pCamera = new Common::FreeflyCamera(cameraSpeed, cameraSensitivity, glm::vec3(0.0f, 5.0f, 30.0f), glm::vec3(0.0f));

	const float fov = 45.0f;
	const float aspectRatio = static_cast<float>(m_window.GetCurrentWidth()) / m_window.GetCurrentHeight();
	m_cameraData.view = m_pCamera->GetViewMatrix();
	m_cameraData.proj = glm::perspective(glm::radians(fov), aspectRatio, 0.1f, 1000.0f);
	m_cameraData.proj[1][1] *= -1.0f;

	// SHADERS
	ShaderHandle computeShader = m_pShaderManager->RegisterShader("../src/shaders/animation.comp", SHADER_STAGE::COMPUTE, m_renderDevice);
	if (!computeShader.IsValid()) return;
	m_computeShaders = { computeShader };

	ShaderHandle vertShader = m_pShaderManager->RegisterShader("../src/shaders/animated_model.vert", SHADER_STAGE::VERTEX, m_renderDevice);
	if (!vertShader.IsValid()) return;
	ShaderHandle fragShader = m_pShaderManager->RegisterShader("../src/shaders/animated_model.frag", SHADER_STAGE::FRAGMENT, m_renderDevice);
	if (!fragShader.IsValid()) return;
	m_graphicsShaders = { vertShader, fragShader };

	// INPUT ATTRIBUTES
	m_inputAttributes =
	{
		{ 0, 0, BASE_FORMAT::R32G32B32_FLOAT },     // position (vec3)
		{ 1, 0, BASE_FORMAT::R32G32B32_FLOAT },     // normal (vec3)
		{ 2, 0, BASE_FORMAT::R32G32_FLOAT },        // uv (vec2)
		{ 3, 0, BASE_FORMAT::R32G32B32A32_UINT },   // boneIndices (uvec4)
		{ 4, 0, BASE_FORMAT::R32G32B32A32_FLOAT },  // boneWeights (vec4)
	};

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

	// ALLOCATE BUFFERS
	// Vertex buffer
	{
		BufferCreateInfo ci{};
		ci.pName = "VertexBuffer";
		ci.bufferUsage = BUFFER_USAGE::VERTEX_BUFFER;
		ci.sizeBytes = asset->vertices.size() * sizeof(AssetVertex);
		phxRes = m_renderDevice.AllocateBuffer(ci, m_vertexBuffer);
		CHECK_PHX_RES(phxRes);
	}

	// Index buffer
	{
		BufferCreateInfo ci{};
		ci.pName = "IndexBuffer";
		ci.bufferUsage = BUFFER_USAGE::INDEX_BUFFER;
		ci.sizeBytes = asset->indices.size() * sizeof(Common::AssetIndexType);
		phxRes = m_renderDevice.AllocateBuffer(ci, m_indexBuffer);
		CHECK_PHX_RES(phxRes);
	}

	// Skeleton SSBO
	{
		BufferCreateInfo ci{};
		ci.pName = "SkeletonBuffer";
		ci.bufferUsage = BUFFER_USAGE::STORAGE_BUFFER;
		ci.sizeBytes = boneCount * sizeof(BoneInfoGPU);
		phxRes = m_renderDevice.AllocateBuffer(ci, m_skeletonBuffer);
		CHECK_PHX_RES(phxRes);
	}

	// Build keyframe/channel/clip data
	std::vector<KeyframeGPU> keyframesGPU;
	std::vector<ChannelInfoGPU> channelsGPU;
	std::vector<ClipInfoGPU> clipsGPU;

	for (uint32_t clipIdx = 0; clipIdx < asset->animations.size(); clipIdx++)
	{
		const AnimationClip& clip = asset->animations[clipIdx];
		ClipInfoGPU clipInfo{};
		clipInfo.duration = clip.duration;
		clipInfo.ticksPerSecond = clip.ticksPerSecond > 0.0f ? clip.ticksPerSecond : 25.0f;
		clipInfo.channelOffset = static_cast<uint32_t>(channelsGPU.size());
		clipInfo.channelCount = static_cast<uint32_t>(clip.channels.size());

		for (uint32_t chIdx = 0; chIdx < clip.channels.size(); chIdx++)
		{
			const AnimationChannel& channel = clip.channels[chIdx];
			ChannelInfoGPU chInfo{};
			chInfo.nodeIndex = channel.nodeIndex;
			chInfo.keyframeOffset = static_cast<uint32_t>(keyframesGPU.size());
			chInfo.keyframeCount = static_cast<uint32_t>(channel.keys.size());

			for (uint32_t kIdx = 0; kIdx < channel.keys.size(); kIdx++)
			{
				const AnimationKey& key = channel.keys[kIdx];
				KeyframeGPU kf{};
				kf.time = key.time;
				kf.translation = glm::vec4(key.translation, 0.0f);
				kf.rotation = glm::vec4(key.rotation.x, key.rotation.y, key.rotation.z, key.rotation.w);
				kf.scale = glm::vec4(key.scale, 0.0f);
				keyframesGPU.push_back(kf);
			}

			channelsGPU.push_back(chInfo);
		}

		clipsGPU.push_back(clipInfo);
	}

	// Keyframe SSBO
	if (!keyframesGPU.empty())
	{
		BufferCreateInfo ci{};
		ci.pName = "KeyframeBuffer";
		ci.bufferUsage = BUFFER_USAGE::STORAGE_BUFFER;
		ci.sizeBytes = keyframesGPU.size() * sizeof(KeyframeGPU);
		phxRes = m_renderDevice.AllocateBuffer(ci, m_keyframeBuffer);
		CHECK_PHX_RES(phxRes);
	}

	// Channel info SSBO
	if (!channelsGPU.empty())
	{
		BufferCreateInfo ci{};
		ci.pName = "ChannelInfoBuffer";
		ci.bufferUsage = BUFFER_USAGE::STORAGE_BUFFER;
		ci.sizeBytes = channelsGPU.size() * sizeof(ChannelInfoGPU);
		phxRes = m_renderDevice.AllocateBuffer(ci, m_channelInfoBuffer);
		CHECK_PHX_RES(phxRes);
	}

	// Clip info SSBO
	if (!clipsGPU.empty())
	{
		BufferCreateInfo ci{};
		ci.pName = "ClipInfoBuffer";
		ci.bufferUsage = BUFFER_USAGE::STORAGE_BUFFER;
		ci.sizeBytes = clipsGPU.size() * sizeof(ClipInfoGPU);
		phxRes = m_renderDevice.AllocateBuffer(ci, m_clipInfoBuffer);
		CHECK_PHX_RES(phxRes);
	}

	// Instance SSBO
	{
		BufferCreateInfo ci{};
		ci.pName = "InstanceBuffer";
		ci.bufferUsage = BUFFER_USAGE::STORAGE_BUFFER;
		ci.sizeBytes = MAX_INSTANCES * sizeof(InstanceData);
		phxRes = m_renderDevice.AllocateBuffer(ci, m_instanceBuffer);
		CHECK_PHX_RES(phxRes);
	}

	// Node parent indices SSBO
	if (!asset->skeleton.nodeParentIndices.empty())
	{
		BufferCreateInfo ci{};
		ci.pName = "NodeParentBuffer";
		ci.bufferUsage = BUFFER_USAGE::STORAGE_BUFFER;
		ci.sizeBytes = asset->skeleton.nodeParentIndices.size() * sizeof(int32_t);
		phxRes = m_renderDevice.AllocateBuffer(ci, m_nodeParentBuffer);
		CHECK_PHX_RES(phxRes);
	}

	// Node bind-pose local transforms SSBO
	if (!asset->skeleton.nodeLocalTransforms.empty())
	{
		BufferCreateInfo ci{};
		ci.pName = "NodeTransformBuffer";
		ci.bufferUsage = BUFFER_USAGE::STORAGE_BUFFER;
		ci.sizeBytes = asset->skeleton.nodeLocalTransforms.size() * sizeof(glm::mat4);
		phxRes = m_renderDevice.AllocateBuffer(ci, m_nodeTransformBuffer);
		CHECK_PHX_RES(phxRes);
	}

	// Bone matrix SSBO (output of compute, input of graphics)
	{
		BufferCreateInfo ci{};
		ci.pName = "BoneMatrixBuffer";
		ci.bufferUsage = BUFFER_USAGE::STORAGE_BUFFER;
		ci.sizeBytes = MAX_INSTANCES * boneCount * sizeof(glm::mat4);
		phxRes = m_renderDevice.AllocateBuffer(ci, m_boneMatrixBuffer);
		CHECK_PHX_RES(phxRes);
	}

	// Camera UBO
	{
		BufferCreateInfo ci{};
		ci.pName = "CameraBuffer";
		ci.bufferUsage = BUFFER_USAGE::UNIFORM_BUFFER;
		ci.sizeBytes = sizeof(CameraData);
		phxRes = m_renderDevice.AllocateBuffer(ci, m_cameraBuffer);
		CHECK_PHX_RES(phxRes);
	}

	// Animation params UBO
	{
		BufferCreateInfo ci{};
		ci.pName = "AnimParamsBuffer";
		ci.bufferUsage = BUFFER_USAGE::UNIFORM_BUFFER;
		ci.sizeBytes = sizeof(AnimParamsData);
		phxRes = m_renderDevice.AllocateBuffer(ci, m_animParamsBuffer);
		CHECK_PHX_RES(phxRes);
	}

	// UNIFORM COLLECTIONS
	CreateUniformCollections();

	// COMPUTE PIPELINE
	m_computePipelineDesc.shader = m_computeShaders[0];
	m_computePipelineDesc.uniformCollection = m_computeUniformCollection;

	// GRAPHICS PIPELINE
	m_graphicsPipelineDesc.viewportSize = { m_swapChain.GetWidth(), m_swapChain.GetHeight() };
	m_graphicsPipelineDesc.viewportPos = { 0, 0 };
	m_graphicsPipelineDesc.polygonMode = POLYGON_MODE::FILL;
	m_graphicsPipelineDesc.topology = PRIMITIVE_TOPOLOGY::TRIANGLE_LIST;
	m_graphicsPipelineDesc.cullMode = CULL_MODE::BACK;
	m_graphicsPipelineDesc.frontFaceWinding = FRONT_FACE_WINDING::COUNTER_CLOCKWISE;
	m_graphicsPipelineDesc.pShaders = m_graphicsShaders.data();
	m_graphicsPipelineDesc.shaderCount = static_cast<u32>(m_graphicsShaders.size());
	m_graphicsPipelineDesc.pInputAttributes = m_inputAttributes.data();
	m_graphicsPipelineDesc.attributeCount = static_cast<u32>(m_inputAttributes.size());
	m_graphicsPipelineDesc.uniformCollection = m_graphicsUniformCollection;
	m_graphicsPipelineDesc.enableDepthTest = true;
	m_graphicsPipelineDesc.enableDepthWrite = true;

	// ASSET TEXTURES
	CreateAssetTextures();

	// GENERATE INSTANCE DATA
	m_instances.resize(MAX_INSTANCES);
	RegenerateInstanceData();

	// UPLOAD STATIC DATA TO GPU
	UploadDataToGPU();
}

void InstancedAnimationSample::ShutdownSample()
{
	m_assetTextures.clear();
	m_computeShaders.clear();
	m_graphicsShaders.clear();

	m_imguiRenderer.Shutdown();
	m_imguiBackend.Shutdown();

	if (m_pCamera != nullptr)
	{
		delete m_pCamera;
		m_pCamera = nullptr;
	}
}

void InstancedAnimationSample::CreateAssetTextures()
{
	AssetType* pAsset = AssetManager::Get().GetAsset(m_assetID);
	if (pAsset == nullptr) return;

	for (u32 i = 0; i < static_cast<u32>(pAsset->textures.size()); i++)
	{
		const Common::TextureType& currTex = pAsset->textures[i];

		const u32 numMips = static_cast<u32>(currTex.mipLevels.size());
		if (numMips == 0) continue;

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
		baseCI.usageFlags = USAGE_TYPE_FLAG_SAMPLED | USAGE_TYPE_FLAG_TRANSFER_DST | USAGE_TYPE_FLAG_INPUT_ATTACHMENT;

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

		TextureHandle pAssetTex;
		STATUS_CODE res = m_renderDevice.AllocateTexture(baseCI, viewCI, samplerCI, pAssetTex);
		CHECK_PHX_RES(res);

		m_assetTextures.push_back(pAssetTex);
	}
}

void InstancedAnimationSample::CreateUniformCollections()
{
	STATUS_CODE phxRes;

	// COMPUTE uniform collection (set 0)
	std::vector<UniformData> computeUniforms =
	{
		{ 0, UNIFORM_TYPE::STORAGE_BUFFER, SHADER_STAGE_FLAG_COMPUTE },
		{ 1, UNIFORM_TYPE::STORAGE_BUFFER, SHADER_STAGE_FLAG_COMPUTE },
		{ 2, UNIFORM_TYPE::STORAGE_BUFFER, SHADER_STAGE_FLAG_COMPUTE },
		{ 3, UNIFORM_TYPE::STORAGE_BUFFER, SHADER_STAGE_FLAG_COMPUTE },
		{ 4, UNIFORM_TYPE::STORAGE_BUFFER, SHADER_STAGE_FLAG_COMPUTE },
		{ 5, UNIFORM_TYPE::STORAGE_BUFFER, SHADER_STAGE_FLAG_COMPUTE },
		{ 6, UNIFORM_TYPE::UNIFORM_BUFFER, SHADER_STAGE_FLAG_COMPUTE },
		{ 7, UNIFORM_TYPE::STORAGE_BUFFER, SHADER_STAGE_FLAG_COMPUTE },
		{ 8, UNIFORM_TYPE::STORAGE_BUFFER, SHADER_STAGE_FLAG_COMPUTE },
	};

	UniformDataGroup computeGroup{};
	computeGroup.set = 0;
	computeGroup.uniformArray = computeUniforms.data();
	computeGroup.uniformArrayCount = static_cast<u32>(computeUniforms.size());

	std::array<UniformDataGroup, 1> computeGroups = { computeGroup };

	UniformCollectionCreateInfo computeCI{};
	computeCI.dataGroups = computeGroups.data();
	computeCI.groupCount = static_cast<u32>(computeGroups.size());

	phxRes = m_renderDevice.AllocateUniformCollection(computeCI, m_computeUniformCollection);
	CHECK_PHX_RES(phxRes);

	// Queue buffer bindings for compute
	m_computeUniformCollection.QueueBufferUpdate(m_skeletonBuffer, 0, 0, 0);
	m_computeUniformCollection.QueueBufferUpdate(m_keyframeBuffer, 0, 1, 0);
	m_computeUniformCollection.QueueBufferUpdate(m_clipInfoBuffer, 0, 2, 0);
	m_computeUniformCollection.QueueBufferUpdate(m_channelInfoBuffer, 0, 3, 0);
	m_computeUniformCollection.QueueBufferUpdate(m_instanceBuffer, 0, 4, 0);
	m_computeUniformCollection.QueueBufferUpdate(m_boneMatrixBuffer, 0, 5, 0);
	m_computeUniformCollection.QueueBufferUpdate(m_animParamsBuffer, 0, 6, 0);
	m_computeUniformCollection.QueueBufferUpdate(m_nodeParentBuffer, 0, 7, 0);
	m_computeUniformCollection.QueueBufferUpdate(m_nodeTransformBuffer, 0, 8, 0);

	// GRAPHICS uniform collection
	// Set 0: bone matrices, instance data, camera
	std::vector<UniformData> graphicsSet0 =
	{
		{ 0, UNIFORM_TYPE::STORAGE_BUFFER, SHADER_STAGE_FLAG_VERTEX },
		{ 1, UNIFORM_TYPE::STORAGE_BUFFER, SHADER_STAGE_FLAG_VERTEX },
		{ 2, UNIFORM_TYPE::UNIFORM_BUFFER, SHADER_STAGE_FLAG_VERTEX },
	};

	UniformDataGroup gSet0{};
	gSet0.set = 0;
	gSet0.uniformArray = graphicsSet0.data();
	gSet0.uniformArrayCount = static_cast<u32>(graphicsSet0.size());

	// Set 1: textures
	std::vector<UniformData> graphicsSet1;
	for (u32 i = 0; i < 5; i++)
	{
		graphicsSet1.push_back({ i, UNIFORM_TYPE::COMBINED_IMAGE_SAMPLER, SHADER_STAGE_FLAG_FRAGMENT });
	}

	UniformDataGroup gSet1{};
	gSet1.set = 1;
	gSet1.uniformArray = graphicsSet1.data();
	gSet1.uniformArrayCount = static_cast<u32>(graphicsSet1.size());

	std::array<UniformDataGroup, 2> graphicsGroups = { gSet0, gSet1 };

	UniformCollectionCreateInfo graphicsCI{};
	graphicsCI.dataGroups = graphicsGroups.data();
	graphicsCI.groupCount = static_cast<u32>(graphicsGroups.size());

	phxRes = m_renderDevice.AllocateUniformCollection(graphicsCI, m_graphicsUniformCollection);
	CHECK_PHX_RES(phxRes);

	// Queue buffer bindings for graphics
	m_graphicsUniformCollection.QueueBufferUpdate(m_boneMatrixBuffer, 0, 0, 0);
	m_graphicsUniformCollection.QueueBufferUpdate(m_instanceBuffer, 0, 1, 0);
	m_graphicsUniformCollection.QueueBufferUpdate(m_cameraBuffer, 0, 2, 0);
}

void InstancedAnimationSample::RegenerateInstanceData()
{
	const AssetType* asset = AssetManager::Get().GetAsset(m_assetID);
	if (asset == nullptr) return;

	uint32_t clipCount = static_cast<uint32_t>(asset->animations.size());
	if (clipCount == 0) clipCount = 1;

	std::mt19937 rng(static_cast<uint32_t>(m_randomSeed));
	std::uniform_real_distribution<float> timeDist(0.0f, 10.0f);
	std::uniform_int_distribution<uint32_t> clipDist(0, clipCount - 1);

	// Grid layout
	const uint32_t gridSide = static_cast<uint32_t>(std::sqrt(static_cast<float>(m_instanceCount)));
	const float spacing = 5.0f;
	const float gridOffset = (gridSide - 1) * spacing * 0.5f;

	for (uint32_t i = 0; i < m_instanceCount; i++)
	{
		uint32_t x = i % gridSide;
		uint32_t z = i / gridSide;

		glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(
			x * spacing - gridOffset,
			0.0f,
			z * spacing - gridOffset
		));

		m_instances[i].modelMatrix = model;
		m_instances[i].timeOffset = timeDist(rng);

		switch (m_clipMode)
		{
			case ClipAssignmentMode::ALL_SAME:
				m_instances[i].clipIndex = m_activeClipIndex % clipCount;
				break;
			case ClipAssignmentMode::ROUND_ROBIN:
				m_instances[i].clipIndex = i % clipCount;
				break;
			case ClipAssignmentMode::RANDOM:
				m_instances[i].clipIndex = clipDist(rng);
				break;
		}
	}

	m_instanceDataDirty = true;
}

void InstancedAnimationSample::UploadDataToGPU()
{
	const AssetType* asset = AssetManager::Get().GetAsset(m_assetID);
	if (asset == nullptr) return;

	STATUS_CODE phxRes;

	RenderPassHandle transferPass;
	phxRes = m_renderGraph.RegisterPass("DataUpload", PASS_TYPE::TRANSFER, transferPass);
	CHECK_PHX_RES(phxRes);

	transferPass.SetBufferOutput(m_vertexBuffer);
	transferPass.SetBufferOutput(m_indexBuffer);
	transferPass.SetBufferOutput(m_skeletonBuffer);
	transferPass.SetBufferOutput(m_instanceBuffer);

	if (m_keyframeBuffer.IsValid()) transferPass.SetBufferOutput(m_keyframeBuffer);
	if (m_channelInfoBuffer.IsValid()) transferPass.SetBufferOutput(m_channelInfoBuffer);
	if (m_clipInfoBuffer.IsValid()) transferPass.SetBufferOutput(m_clipInfoBuffer);
	if (m_nodeParentBuffer.IsValid()) transferPass.SetBufferOutput(m_nodeParentBuffer);
	if (m_nodeTransformBuffer.IsValid()) transferPass.SetBufferOutput(m_nodeTransformBuffer);

	for (u32 i = 0; i < m_assetTextures.size(); i++)
	{
		transferPass.SetColorOutput(m_assetTextures[i]);
	}

	// Build GPU-side skeleton data
	std::vector<BoneInfoGPU> skeletonGPU(asset->skeleton.bones.size());
	for (uint32_t i = 0; i < asset->skeleton.bones.size(); i++)
	{
		skeletonGPU[i].parentIndex = asset->skeleton.bones[i].parentIndex;
		skeletonGPU[i].nodeIndex = asset->skeleton.bones[i].nodeIndex;
		skeletonGPU[i].offsetMatrix = asset->skeleton.bones[i].offsetMatrix;
		skeletonGPU[i].bindLocalTransform = asset->skeleton.nodeLocalTransforms[asset->skeleton.bones[i].nodeIndex];
	}

	// Build GPU-side keyframe/channel/clip data
	std::vector<KeyframeGPU> keyframesGPU;
	std::vector<ChannelInfoGPU> channelsGPU;
	std::vector<ClipInfoGPU> clipsGPU;

	for (uint32_t clipIdx = 0; clipIdx < asset->animations.size(); clipIdx++)
	{
		const AnimationClip& clip = asset->animations[clipIdx];
		ClipInfoGPU clipInfo{};
		clipInfo.duration = clip.duration;
		clipInfo.ticksPerSecond = clip.ticksPerSecond > 0.0f ? clip.ticksPerSecond : 25.0f;
		clipInfo.channelOffset = static_cast<uint32_t>(channelsGPU.size());
		clipInfo.channelCount = static_cast<uint32_t>(clip.channels.size());

		for (uint32_t chIdx = 0; chIdx < clip.channels.size(); chIdx++)
		{
			const AnimationChannel& channel = clip.channels[chIdx];
			ChannelInfoGPU chInfo{};
			chInfo.nodeIndex = channel.nodeIndex;
			chInfo.keyframeOffset = static_cast<uint32_t>(keyframesGPU.size());
			chInfo.keyframeCount = static_cast<uint32_t>(channel.keys.size());

			for (uint32_t kIdx = 0; kIdx < channel.keys.size(); kIdx++)
			{
				const AnimationKey& key = channel.keys[kIdx];
				KeyframeGPU kf{};
				kf.time = key.time;
				kf.translation = glm::vec4(key.translation, 0.0f);
				kf.rotation = glm::vec4(key.rotation.x, key.rotation.y, key.rotation.z, key.rotation.w);
				kf.scale = glm::vec4(key.scale, 0.0f);
				keyframesGPU.push_back(kf);
			}

			channelsGPU.push_back(chInfo);
		}

		clipsGPU.push_back(clipInfo);
	}

	transferPass.SetExecuteCallback([&, asset, skeletonGPU, keyframesGPU, channelsGPU, clipsGPU](DeviceContextHandle deviceContext) mutable
	{
		// Vertex data
		deviceContext.CopyDataToBuffer(m_vertexBuffer, asset->vertices.data(), asset->vertices.size() * sizeof(AssetVertex));
		deviceContext.CopyDataToBuffer(m_indexBuffer, asset->indices.data(), asset->indices.size() * sizeof(Common::AssetIndexType));

		// Skeleton
		deviceContext.CopyDataToBuffer(m_skeletonBuffer, skeletonGPU.data(), skeletonGPU.size() * sizeof(BoneInfoGPU));

		// Keyframes / channels / clips
		if (!keyframesGPU.empty())
			deviceContext.CopyDataToBuffer(m_keyframeBuffer, keyframesGPU.data(), keyframesGPU.size() * sizeof(KeyframeGPU));
		if (!channelsGPU.empty())
			deviceContext.CopyDataToBuffer(m_channelInfoBuffer, channelsGPU.data(), channelsGPU.size() * sizeof(ChannelInfoGPU));
		if (!clipsGPU.empty())
			deviceContext.CopyDataToBuffer(m_clipInfoBuffer, clipsGPU.data(), clipsGPU.size() * sizeof(ClipInfoGPU));

		// Instance data
		deviceContext.CopyDataToBuffer(m_instanceBuffer, m_instances.data(), m_instanceCount * sizeof(InstanceData));

		// Node hierarchy
		if (!asset->skeleton.nodeParentIndices.empty())
			deviceContext.CopyDataToBuffer(m_nodeParentBuffer, asset->skeleton.nodeParentIndices.data(), asset->skeleton.nodeParentIndices.size() * sizeof(int32_t));
		if (!asset->skeleton.nodeLocalTransforms.empty())
			deviceContext.CopyDataToBuffer(m_nodeTransformBuffer, asset->skeleton.nodeLocalTransforms.data(), asset->skeleton.nodeLocalTransforms.size() * sizeof(glm::mat4));

		// Textures
		for (u32 i = 0; i < m_assetTextures.size(); i++)
		{
			const Common::TextureType& texSrc = asset->textures[i];
			TextureHandle texDst = m_assetTextures[i];

			for (u32 mip = 0; mip < texSrc.mipLevels.size(); mip++)
			{
				deviceContext.CopyDataToTexture(texDst, texSrc.mipLevels[mip].data.data(), texSrc.mipLevels[mip].dataSize, mip);
			}
		}
	});
}

void InstancedAnimationSample::BuildImGuiUI()
{
	const AssetType* asset = AssetManager::Get().GetAsset(m_assetID);

	ImGui::Begin("Animation Controls");

	// Play/Pause
	ImGui::Checkbox("Play", &m_paused);
	ImGui::SameLine();
	ImGui::Text("(checked = paused)");

	// Animation speed
	ImGui::SliderFloat("Speed", &m_animSpeed, 0.0f, 5.0f, "%.2f");

	// Random seed
	if (ImGui::DragInt("Seed", &m_randomSeed, 1, 0, 100000))
	{
		RegenerateInstanceData();
	}

	// Instance count
	if (ImGui::SliderInt("Instances", reinterpret_cast<int*>(&m_instanceCount), 1, static_cast<int>(MAX_INSTANCES)))
	{
		m_instanceCount = std::max(1u, m_instanceCount);
		RegenerateInstanceData();
	}

	ImGui::Separator();

	// Clip assignment mode
	int mode = static_cast<int>(m_clipMode);
	if (ImGui::RadioButton("All Same Clip", mode == static_cast<int>(ClipAssignmentMode::ALL_SAME)))
	{
		m_clipMode = ClipAssignmentMode::ALL_SAME;
		RegenerateInstanceData();
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Round-Robin", mode == static_cast<int>(ClipAssignmentMode::ROUND_ROBIN)))
	{
		m_clipMode = ClipAssignmentMode::ROUND_ROBIN;
		RegenerateInstanceData();
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Random", mode == static_cast<int>(ClipAssignmentMode::RANDOM)))
	{
		m_clipMode = ClipAssignmentMode::RANDOM;
		RegenerateInstanceData();
	}

	// Clip selector (only relevant in ALL_SAME mode)
	if (asset && !asset->animations.empty())
	{
		std::vector<const char*> clipNames;
		for (uint32_t i = 0; i < asset->animations.size(); i++)
		{
			clipNames.push_back(asset->animations[i].name.c_str());
		}

		if (ImGui::Combo("Active Clip", reinterpret_cast<int*>(&m_activeClipIndex), clipNames.data(), static_cast<int>(clipNames.size())))
		{
			if (m_clipMode == ClipAssignmentMode::ALL_SAME)
			{
				RegenerateInstanceData();
			}
		}
	}

	ImGui::Separator();

	// Stats
	ImGui::Text("FPS: %.1f", 1000.0f / (m_lastFrameTime * 1000.0f));
	ImGui::Text("Frame time: %.3f ms", m_lastFrameTime * 1000.0f);
	ImGui::Text("Global time: %.2f", m_globalTime);

	if (asset)
	{
		ImGui::Text("Bones: %u", static_cast<uint32_t>(asset->skeleton.bones.size()));
		ImGui::Text("Animations: %u", static_cast<uint32_t>(asset->animations.size()));
		ImGui::Text("Vertices: %u", static_cast<uint32_t>(asset->vertices.size()));
		ImGui::Text("Indices: %u", static_cast<uint32_t>(asset->indices.size()));
	}

	const PHX::Metrics& metrics = m_renderGraph.GetMetrics();
	ImGui::Separator();
	ImGui::Text("Draw calls: %u", metrics.drawCalls);
	ImGui::Text("Triangles: %u", metrics.triangles);
	ImGui::Text("Pass count: %u", metrics.passCount);
	ImGui::Text("GPU frame time: %.3f ms", metrics.gpuFrameTime);

	ImGui::End();
}

void InstancedAnimationSample::OnKeyDown(PHX::KeyCode keycode)
{
	BaseSample::OnKeyDown(keycode);
	m_imguiBackend.OnKeyDown(keycode);
}

void InstancedAnimationSample::OnKeyUp(PHX::KeyCode keycode)
{
	BaseSample::OnKeyUp(keycode);
	m_imguiBackend.OnKeyUp(keycode);
}

void InstancedAnimationSample::OnMouseButtonDown(PHX::MouseButtonCode mouseButton)
{
	BaseSample::OnMouseButtonDown(mouseButton);
	m_imguiBackend.OnMouseButtonDown(mouseButton);
}

void InstancedAnimationSample::OnMouseButtonUp(PHX::MouseButtonCode mouseButton)
{
	BaseSample::OnMouseButtonUp(mouseButton);
	m_imguiBackend.OnMouseButtonUp(mouseButton);
}

void InstancedAnimationSample::OnMouseMoved(float newX, float newY)
{
	BaseSample::OnMouseMoved(newX, newY);
	m_imguiBackend.OnMouseMoved(newX, newY);
}
