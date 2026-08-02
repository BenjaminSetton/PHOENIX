#include "lod_sample.h"

#include <imgui.h>
#include <iostream>
#include <random>
#include <cmath>

#include "../../common/src/input_manager.h"
#include "../../common/src/utils/shader_utils.h"

LodSample::LodSample()
{
}

LodSample::~LodSample()
{
}

void LodSample::InitSample()
{
	PHX::STATUS_CODE phxRes;

	m_window.SetWindowTitle("PHX %u.%u.%u | LOD", PHX::GetMajorVersion(), PHX::GetMinorVersion(), PHX::GetPatchVersion());

	// Check drawIndirectCount support — query the device capability
	m_useIndirectCount = m_renderDevice.IsDrawIndirectCountSupported();
	if (m_useIndirectCount)
	{
		std::cout << "[LOD SAMPLE] drawIndirectCount supported. Using GPU-driven draw count" << std::endl;
	}
	else
	{
		std::cout << "[LOD SAMPLE] drawIndirectCount not supported. Falling back to fixed draw count" << std::endl;
	}

	// LOAD MODEL — use common shape assets (hard-coded paths per USER_NOTE)
	// We import multiple shapes and combine them into LOD groups at load time
	// since the common shapes don't have authored LODs.
	// This lets us verify LOD switching visually (sphere=LOD0, cylinder=LOD1, cube=LOD2).
	Common::AssetHandle sphereID = AssetManager::Get().LoadOrImport("sphere.fbx", false, false);
	Common::AssetHandle cylinderID = AssetManager::Get().LoadOrImport("cylinder.fbx", false, false);
	Common::AssetHandle cubeID = AssetManager::Get().LoadOrImport("cube.fbx", false, false);

	AssetType* sphereAsset = AssetManager::Get().GetAsset(sphereID);
	AssetType* cylinderAsset = AssetManager::Get().GetAsset(cylinderID);
	AssetType* cubeAsset = AssetManager::Get().GetAsset(cubeID);

	if (sphereAsset && cylinderAsset && cubeAsset)
	{
		// Combine into a single LOD group with 3 levels
		// Take the first (and only) LOD group from each shape
		LodGroupData combinedGroup;

		// LOD thresholds are screen-space ratios: radius / (distance * tan(fov/2)).
		// For unit shapes (radius 1) and a 60-degree FOV (tan(30) ~= 0.577), the ratio
		// is ~1.73 / distance. The thresholds below map to approximate switch distances:
		//   LOD0 (sphere)   : distance <~ 130 units
		//   LOD1 (cylinder) : distance <~ 260 units
		//   LOD2 (cube)     : everything further (fallback)
		// This spreads the three LODs into clearly visible concentric bands across the field.

		// LOD 0: sphere (highest quality)
		if (!sphereAsset->lodGroups.empty() && !sphereAsset->lodGroups[0].levels.empty())
		{
			combinedGroup.levels.push_back(sphereAsset->lodGroups[0].levels[0]);
			combinedGroup.levels.back().screenRatio = 0.0133f;
		}

		// LOD 1: cylinder (medium quality)
		if (!cylinderAsset->lodGroups.empty() && !cylinderAsset->lodGroups[0].levels.empty())
		{
			combinedGroup.levels.push_back(cylinderAsset->lodGroups[0].levels[0]);
			combinedGroup.levels.back().screenRatio = 0.0067f;
		}

		// LOD 2: cube (lowest quality)
		if (!cubeAsset->lodGroups.empty() && !cubeAsset->lodGroups[0].levels.empty())
		{
			combinedGroup.levels.push_back(cubeAsset->lodGroups[0].levels[0]);
			combinedGroup.levels.back().screenRatio = 0.0f; // Always visible (fallback)
		}

		sphereAsset->lodGroups.clear();
		sphereAsset->lodGroups.push_back(std::move(combinedGroup));
	}

	std::cout << "[LOD SAMPLE] Loaded asset with " << sphereAsset->lodGroups.size() << " LOD groups" << std::endl;
	for (uint32_t g = 0; g < sphereAsset->lodGroups.size(); g++)
	{
		std::cout << "  Group " << g << ": " << sphereAsset->lodGroups[g].levels.size() << " levels" << std::endl;
		for (uint32_t l = 0; l < sphereAsset->lodGroups[g].levels.size(); l++)
		{
			std::cout << "    Level " << l << ": " << sphereAsset->lodGroups[g].levels[l].vertices.size()
				<< " verts, " << sphereAsset->lodGroups[g].levels[l].indices.size()
				<< " indices, ratio=" << sphereAsset->lodGroups[g].levels[l].screenRatio << std::endl;
		}
	}

	// CAMERAS — two freefly cameras.
	// FreeflyCamera rotation is (yaw, pitch, roll) in degrees; identity looks down -Z.
	// A negative pitch tilts the camera downward.
	//
	// - LOD/frustum camera ("player"): sits above and behind the instance field looking
	//   down at it, so its view frustum slices through the field. Culling + LOD are always
	//   computed from this camera.
	// - Freefly observer: sits further back/higher looking at the same field so the frozen
	//   green frustum and the culling/LOD result can be inspected from the outside.
	m_pLodCamera = new Common::FreeflyCamera(40.0f, 0.1f, glm::vec3(0.0f, 60.0f, 220.0f), glm::vec3(0.0f, -18.0f, 0.0f));
	m_pFreeflyCamera = new Common::FreeflyCamera(80.0f, 0.1f, glm::vec3(0.0f, 250.0f, 450.0f), glm::vec3(0.0f, -28.0f, 0.0f));

	// Set the base sample's camera to the LOD camera (so BaseSample updates it)
	m_pCamera = m_pLodCamera;

	// SHADERS
	{
		PHX::ShaderHandle computeShader = m_pShaderManager->RegisterShader("../src/shaders/lod_cull.comp.slang", PHX::SHADER_STAGE::COMPUTE, m_renderDevice);
		if (!computeShader.IsValid()) return;
		m_computeShaders = { computeShader };

		PHX::ShaderHandle vertShader = m_pShaderManager->RegisterShader("../src/shaders/lod_mesh.vert.slang", PHX::SHADER_STAGE::VERTEX, m_renderDevice);
		if (!vertShader.IsValid()) return;
		PHX::ShaderHandle fragShader = m_pShaderManager->RegisterShader("../src/shaders/lod_mesh.frag.slang", PHX::SHADER_STAGE::FRAGMENT, m_renderDevice);
		if (!fragShader.IsValid()) return;
		m_meshShaders = { vertShader, fragShader };

		// Debug line shaders (from common shaders)
		PHX::ShaderHandle debugVert = m_pShaderManager->RegisterShader("../../common/src/shaders/debug_line.vert.slang", PHX::SHADER_STAGE::VERTEX, m_renderDevice);
		if (!debugVert.IsValid()) return;
		PHX::ShaderHandle debugFrag = m_pShaderManager->RegisterShader("../../common/src/shaders/debug_line.frag.slang", PHX::SHADER_STAGE::FRAGMENT, m_renderDevice);
		if (!debugFrag.IsValid()) return;
		m_debugLineShaders = { debugVert, debugFrag };
	}

	// INPUT ATTRIBUTES (mesh)
	m_meshInputAttributes =
	{
		{ 0, 0, PHX::BASE_FORMAT::R32G32B32_FLOAT },  // position (vec3)
		{ 1, 0, PHX::BASE_FORMAT::R32G32B32_FLOAT },  // normal (vec3)
		{ 2, 0, PHX::BASE_FORMAT::R32G32_FLOAT },     // uv (vec2)
	};

	// INPUT ATTRIBUTES (debug lines)
	m_debugLineInputAttributes =
	{
		{ 0, 0, PHX::BASE_FORMAT::R32G32B32_FLOAT },  // position (vec3)
		{ 1, 0, PHX::BASE_FORMAT::R32G32B32_FLOAT },  // color (vec3)
	};

	// DEPTH BUFFER
	{
		PHX::TextureBaseCreateInfo depthBufferBaseCI{};
		depthBufferBaseCI.pName = "DepthBuffer";
		depthBufferBaseCI.width = m_swapChain.GetWidth();
		depthBufferBaseCI.height = m_swapChain.GetHeight();
		depthBufferBaseCI.arrayLayers = 1;
		depthBufferBaseCI.generateMips = false;
		depthBufferBaseCI.format = PHX::BASE_FORMAT::D32_FLOAT;
		depthBufferBaseCI.usageFlags = PHX::USAGE_TYPE_FLAG_DEPTH_STENCIL_ATTACHMENT | PHX::USAGE_TYPE_FLAG_SAMPLED;

		PHX::TextureViewCreateInfo depthBufferViewCI{};
		depthBufferViewCI.type = PHX::VIEW_TYPE::TYPE_2D;
		depthBufferViewCI.scope = PHX::VIEW_SCOPE::ENTIRE;
		depthBufferViewCI.aspectFlags = PHX::ASPECT_TYPE_FLAG_DEPTH;

		PHX::TextureSamplerCreateInfo depthBufferSamplerCI{};
		depthBufferSamplerCI.addressModeUVW = PHX::SAMPLER_ADDRESS_MODE::CLAMP_TO_EDGE;
		depthBufferSamplerCI.enableAnisotropicFiltering = false;
		depthBufferSamplerCI.magnificationFilter = PHX::FILTER_MODE::NEAREST;
		depthBufferSamplerCI.minificationFilter = PHX::FILTER_MODE::NEAREST;
		depthBufferSamplerCI.samplerMipMapFilter = PHX::FILTER_MODE::NEAREST;

		phxRes = m_renderDevice.AllocateTexture(depthBufferBaseCI, depthBufferViewCI, depthBufferSamplerCI, m_depthBuffer);
		if (phxRes != PHX::STATUS_CODE::SUCCESS)
		{
			std::cout << "[LOD SAMPLE] Failed to allocate depth buffer!" << std::endl;
			return;
		}
	}

	// INITIALIZE LOD MANAGER
	{
		AssetType* asset = AssetManager::Get().GetAsset(sphereID);
		if (asset == nullptr) return;

		// Convert AssetType LOD groups to AssetDiskLodGroup for the LOD manager
		std::vector<Common::AssetDiskLodGroup> lodGroups;
		lodGroups.resize(asset->lodGroups.size());
		for (uint32_t g = 0; g < asset->lodGroups.size(); g++)
		{
			lodGroups[g].levels.resize(asset->lodGroups[g].levels.size());
			for (uint32_t l = 0; l < asset->lodGroups[g].levels.size(); l++)
			{
				const LodLevelData& srcLevel = asset->lodGroups[g].levels[l];
				Common::AssetDiskLodLevel& dstLevel = lodGroups[g].levels[l];
				dstLevel.screenRatio = srcLevel.screenRatio;

				dstLevel.vertices.resize(srcLevel.vertices.size());
				for (uint32_t v = 0; v < srcLevel.vertices.size(); v++)
				{
					dstLevel.vertices[v].position = PHX::Vec3f(srcLevel.vertices[v].position.x, srcLevel.vertices[v].position.y, srcLevel.vertices[v].position.z);
					dstLevel.vertices[v].normal = PHX::Vec3f(srcLevel.vertices[v].normal.x, srcLevel.vertices[v].normal.y, srcLevel.vertices[v].normal.z);
					dstLevel.vertices[v].uv = PHX::Vec2f(srcLevel.vertices[v].uv.x, srcLevel.vertices[v].uv.y);
				}
				dstLevel.indices = srcLevel.indices;
			}
		}

		m_lodManager.Initialize(m_renderDevice, m_renderGraph, lodGroups);
	}

	// GENERATE INSTANCES
	GenerateInstances();

	// UNIFORM COLLECTIONS
	CreateUniformCollections();

	// COMPUTE PIPELINE
	m_computePipelineDesc.shader = m_computeShaders[0];
	m_computePipelineDesc.uniformCollection = m_computeUniforms;

	// MESH GRAPHICS PIPELINE
	m_meshPipelineDesc.viewportSize = { m_swapChain.GetWidth(), m_swapChain.GetHeight() };
	m_meshPipelineDesc.viewportPos = { 0, 0 };
	m_meshPipelineDesc.polygonMode = PHX::POLYGON_MODE::FILL;
	m_meshPipelineDesc.topology = PHX::PRIMITIVE_TOPOLOGY::TRIANGLE_LIST;
	m_meshPipelineDesc.cullMode = PHX::CULL_MODE::BACK;
	m_meshPipelineDesc.frontFaceWinding = PHX::FRONT_FACE_WINDING::COUNTER_CLOCKWISE;
	m_meshPipelineDesc.pShaders = m_meshShaders.data();
	m_meshPipelineDesc.shaderCount = static_cast<PHX::u32>(m_meshShaders.size());
	m_meshPipelineDesc.pInputAttributes = m_meshInputAttributes.data();
	m_meshPipelineDesc.attributeCount = static_cast<PHX::u32>(m_meshInputAttributes.size());
	m_meshPipelineDesc.uniformCollection = m_meshUniforms;
	m_meshPipelineDesc.enableDepthTest = true;
	m_meshPipelineDesc.enableDepthWrite = true;

	// DEBUG LINE GRAPHICS PIPELINE
	m_debugLinePipelineDesc.viewportSize = { m_swapChain.GetWidth(), m_swapChain.GetHeight() };
	m_debugLinePipelineDesc.viewportPos = { 0, 0 };
	m_debugLinePipelineDesc.polygonMode = PHX::POLYGON_MODE::FILL;
	m_debugLinePipelineDesc.topology = PHX::PRIMITIVE_TOPOLOGY::LINE_LIST;
	m_debugLinePipelineDesc.cullMode = PHX::CULL_MODE::NONE;
	m_debugLinePipelineDesc.pShaders = m_debugLineShaders.data();
	m_debugLinePipelineDesc.shaderCount = static_cast<PHX::u32>(m_debugLineShaders.size());
	m_debugLinePipelineDesc.pInputAttributes = m_debugLineInputAttributes.data();
	m_debugLinePipelineDesc.attributeCount = static_cast<PHX::u32>(m_debugLineInputAttributes.size());
	m_debugLinePipelineDesc.uniformCollection = m_debugLineUniforms;
	m_debugLinePipelineDesc.enableDepthTest = false;
	m_debugLinePipelineDesc.enableDepthWrite = false;

	// DEBUG LINE BUFFERS
	{
		// Vertex buffer for frustum lines (24 lines * 2 verts = 48 verts, but we use more for grid)
		PHX::BufferCreateInfo ci{};
		ci.pName = "DebugLineVertexBuffer";
		ci.bufferUsage = PHX::BUFFER_USAGE_FLAG_VERTEX_BUFFER;
		ci.sizeBytes = 256 * sizeof(float) * 6; // 256 verts * (3 floats pos + 3 floats color)
		phxRes = m_renderDevice.AllocateBuffer(ci, m_debugLineVertexBuffer);
		if (phxRes != PHX::STATUS_CODE::SUCCESS)
		{
			std::cout << "[LOD SAMPLE] Failed to allocate debug line vertex buffer!" << std::endl;
			return;
		}

		// Camera UBO for debug lines
		PHX::BufferCreateInfo camCI{};
		camCI.pName = "DebugCameraBuffer";
		camCI.bufferUsage = PHX::BUFFER_USAGE_FLAG_UNIFORM_BUFFER;
		camCI.sizeBytes = sizeof(DebugCameraData);
		phxRes = m_renderDevice.AllocateBuffer(camCI, m_debugCameraBuffer);
		if (phxRes != PHX::STATUS_CODE::SUCCESS)
		{
			std::cout << "[LOD SAMPLE] Failed to allocate debug camera buffer!" << std::endl;
			return;
		}
	}

	// UPLOAD STATIC DATA (LOD vertex/index/descriptors)
	m_lodManager.UploadStaticData(m_renderGraph, m_swapChain);
}

void LodSample::ShutdownSample()
{
	if (m_pLodCamera != nullptr)
	{
		delete m_pLodCamera;
		m_pLodCamera = nullptr;
	}
	if (m_pFreeflyCamera != nullptr)
	{
		delete m_pFreeflyCamera;
		m_pFreeflyCamera = nullptr;
	}
	m_pCamera = nullptr;
}

void LodSample::UpdateSample(float dt)
{
	// ImGui
	m_imguiBackend.NewFrame(dt, m_swapChain.GetWidth(), m_swapChain.GetHeight());

	ImGui::Begin("LOD Controls");
	ImGui::SliderInt("Instance Count", reinterpret_cast<int*>(&m_instanceCount), 100, 10000);
	ImGui::Checkbox("Wireframe", &m_wireframe);
	ImGui::Checkbox("Show Frustum", &m_showFrustum);
	ImGui::SliderFloat("LOD Distance Scale", &m_lodDistanceScale, 0.1f, 5.0f);
	ImGui::Separator();
	ImGui::Text("Active camera: %s", m_freeflyActive ? "Freefly" : "LOD");
	ImGui::Text("Press F to toggle camera");
	ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "Green = inside LOD frustum");
	ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Red = frustum-culled");
	ImGui::Text("Toggle to Freefly to inspect culling");
	ImGui::Separator();
	ImGui::Text("Instances: %u", m_lodManager.GetInstanceCount());
	ImGui::Text("LOD groups: %u", m_lodManager.GetGroupCount());
	ImGui::Text("Total LOD levels: %u", m_lodManager.GetTotalLodLevels());
	ImGui::Text("IndirectCount: %s", m_useIndirectCount ? "Yes" : "No (fallback)");

	if (ImGui::Button("Regenerate Instances"))
	{
		GenerateInstances();
	}

	const PHX::Metrics& metrics = m_renderGraph.GetMetrics();
	ImGui::Separator();
	ImGui::Text("Draw calls: %u", metrics.drawCalls);
	ImGui::Text("Triangles: %u", metrics.triangles);
	ImGui::Text("Pass count: %u", metrics.passCount);
	ImGui::Text("GPU frame time: %.3f ms", metrics.gpuFrameTime);
	ImGui::End();

	// Toggle camera with F key (edge-triggered)
	static bool fWasPressed = false;
	bool fIsPressed = Common::InputManager::GetInstance().IsKeyPressed(PHX::KeyCode::KEY_F);
	if (fIsPressed && !fWasPressed)
	{
		m_freeflyActive = !m_freeflyActive;
		// Switch the base sample's camera so BaseSample updates the active one.
		// The LOD camera stops receiving input while the observer is active, which freezes
		// both the culling frustum and its visualization in place.
		m_pCamera = m_freeflyActive ? m_pFreeflyCamera : m_pLodCamera;
		std::cout << "[LOD SAMPLE] Switched to " << (m_freeflyActive ? "freefly" : "LOD") << " camera" << std::endl;
	}
	fWasPressed = fIsPressed;
}

void LodSample::CreateUniformCollections()
{
	PHX::STATUS_CODE phxRes;

	// COMPUTE uniform collection (set 0)
	// 0: InstanceBuffer, 1: LodLevelBuffer, 2: LodGroupBuffer,
	// 3: ArgsBuffer, 4: CountBuffer, 5: CameraBuffer, 6: InstanceIndexListBuffer
	std::vector<PHX::UniformData> computeUniforms =
	{
		{ 0, PHX::UNIFORM_TYPE::STORAGE_BUFFER, PHX::SHADER_STAGE_FLAG_COMPUTE },
		{ 1, PHX::UNIFORM_TYPE::STORAGE_BUFFER, PHX::SHADER_STAGE_FLAG_COMPUTE },
		{ 2, PHX::UNIFORM_TYPE::STORAGE_BUFFER, PHX::SHADER_STAGE_FLAG_COMPUTE },
		{ 3, PHX::UNIFORM_TYPE::STORAGE_BUFFER, PHX::SHADER_STAGE_FLAG_COMPUTE },
		{ 4, PHX::UNIFORM_TYPE::STORAGE_BUFFER, PHX::SHADER_STAGE_FLAG_COMPUTE },
		{ 5, PHX::UNIFORM_TYPE::UNIFORM_BUFFER, PHX::SHADER_STAGE_FLAG_COMPUTE },
		{ 6, PHX::UNIFORM_TYPE::STORAGE_BUFFER, PHX::SHADER_STAGE_FLAG_COMPUTE },
	};

	PHX::UniformDataGroup computeGroup{};
	computeGroup.set = 0;
	computeGroup.uniformArray = computeUniforms.data();
	computeGroup.uniformArrayCount = static_cast<PHX::u32>(computeUniforms.size());

	std::array<PHX::UniformDataGroup, 1> computeGroups = { computeGroup };

	PHX::UniformCollectionCreateInfo computeCI{};
	computeCI.dataGroups = computeGroups.data();
	computeCI.groupCount = static_cast<PHX::u32>(computeGroups.size());

	phxRes = m_renderDevice.AllocateUniformCollection(computeCI, m_computeUniforms);
	if (phxRes != PHX::STATUS_CODE::SUCCESS) return;

	// MESH graphics uniform collection (set 0)
	// 0: InstanceBuffer, 1: InstanceIndexListBuffer, 2: CameraBuffer
	std::vector<PHX::UniformData> meshUniforms =
	{
		{ 0, PHX::UNIFORM_TYPE::STORAGE_BUFFER, PHX::SHADER_STAGE_FLAG_VERTEX },
		{ 1, PHX::UNIFORM_TYPE::STORAGE_BUFFER, PHX::SHADER_STAGE_FLAG_VERTEX },
		{ 2, PHX::UNIFORM_TYPE::UNIFORM_BUFFER, PHX::SHADER_STAGE_FLAG_VERTEX },
	};

	PHX::UniformDataGroup meshGroup{};
	meshGroup.set = 0;
	meshGroup.uniformArray = meshUniforms.data();
	meshGroup.uniformArrayCount = static_cast<PHX::u32>(meshUniforms.size());

	std::array<PHX::UniformDataGroup, 1> meshGroups = { meshGroup };

	PHX::UniformCollectionCreateInfo meshCI{};
	meshCI.dataGroups = meshGroups.data();
	meshCI.groupCount = static_cast<PHX::u32>(meshGroups.size());

	phxRes = m_renderDevice.AllocateUniformCollection(meshCI, m_meshUniforms);
	if (phxRes != PHX::STATUS_CODE::SUCCESS) return;

	// Mesh uniform bindings are queued per-frame in the mesh pass callback,
	// since the instance buffer isn't allocated until SetInstances() is called.

	// DEBUG LINE uniform collection (set 0)
	// 0: DebugCameraBuffer (UBO)
	std::vector<PHX::UniformData> debugLineUniforms =
	{
		{ 0, PHX::UNIFORM_TYPE::UNIFORM_BUFFER, PHX::SHADER_STAGE_FLAG_VERTEX },
	};

	PHX::UniformDataGroup debugGroup{};
	debugGroup.set = 0;
	debugGroup.uniformArray = debugLineUniforms.data();
	debugGroup.uniformArrayCount = static_cast<PHX::u32>(debugLineUniforms.size());

	std::array<PHX::UniformDataGroup, 1> debugGroups = { debugGroup };

	PHX::UniformCollectionCreateInfo debugCI{};
	debugCI.dataGroups = debugGroups.data();
	debugCI.groupCount = static_cast<PHX::u32>(debugGroups.size());

	phxRes = m_renderDevice.AllocateUniformCollection(debugCI, m_debugLineUniforms);
	if (phxRes != PHX::STATUS_CODE::SUCCESS) return;

	// Debug line uniform bindings are queued per-frame in the debug pass callback.
}

void LodSample::GenerateInstances()
{
	m_instances.clear();
	m_instances.resize(m_instanceCount);

	// Lay the instances out on a regular grid on the XZ plane (y = 0), centered on the
	// origin. A flat, evenly spaced field (rather than overlapping rings) makes both
	// effects easy to read from the observer camera:
	//   - Frustum culling: instances outside the player's view frustum vanish.
	//   - LOD selection:   instances form concentric quality bands by distance.
	// Spacing is larger than the unit-shape radius so instances never overlap.
	const float spacing = 10.0f;
	const uint32_t gridDim = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(m_instanceCount))));
	const float halfExtent = (gridDim - 1) * spacing * 0.5f;

	std::mt19937 rng(42);
	std::uniform_real_distribution<float> jitter(-1.5f, 1.5f);

	for (uint32_t i = 0; i < m_instanceCount; i++)
	{
		const uint32_t gx = i % gridDim;
		const uint32_t gz = i / gridDim;

		const float x = gx * spacing - halfExtent + jitter(rng);
		const float z = gz * spacing - halfExtent + jitter(rng);

		glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, z));

		m_instances[i].modelMatrix = glm::transpose(model);
		m_instances[i].groupIndex = 0; // All instances use LOD group 0
	}

	m_instancesDirty = true;
}

glm::mat4 LodSample::GetProjectionMatrix() const
{
	// Adjust far plane for freefly cam so it's easier to see culling
	float farPlane = m_freeflyActive ? kFarPlane + 1000.0f : kFarPlane;

	const float aspect = float(m_swapChain.GetWidth()) / float(m_swapChain.GetHeight());
	glm::mat4 proj = glm::perspective(glm::radians(kFovDegrees), aspect, kNearPlane, farPlane);
	proj[1][1] *= -1.0f; // Vulkan Y-flip (clip-space Y points down)
	return proj;
}

void LodSample::ComputeFrustumVertices()
{
	if (!m_pLodCamera) return;

	// The frustum viz is ALWAYS built from the LOD/frustum camera using the exact same
	// projection that culling uses (GetProjectionMatrix), so the green wireframe encloses
	// precisely the volume culled against. When the LOD camera is active the wireframe is
	// invisible (its edges sit on the screen border); when the observer camera is active
	// the LOD camera stops updating, so the wireframe stays frozen in place for inspection.
	const glm::mat4 view = m_pLodCamera->GetViewMatrix();
	const glm::mat4 invViewProj = glm::inverse(GetProjectionMatrix() * view);

	// 8 NDC corners. Depth range is GLM default [-1, 1], so near z = -1, far z = +1.
	const glm::vec3 ndcCorners[8] = {
		{ -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 },  // near
		{ -1, -1,  1 }, { 1, -1,  1 }, { 1, 1,  1 }, { -1, 1,  1 },  // far
	};

	glm::vec3 worldCorners[8];
	for (int i = 0; i < 8; i++)
	{
		glm::vec4 pt = invViewProj * glm::vec4(ndcCorners[i], 1.0f);
		worldCorners[i] = glm::vec3(pt) / pt.w;
	}

	// Line list: 12 edges of the frustum
	static const int edges[12][2] = {
		{0,1},{1,2},{2,3},{3,0},  // near plane
		{4,5},{5,6},{6,7},{7,4},  // far plane
		{0,4},{1,5},{2,6},{3,7},  // connecting edges
	};

	const glm::vec3 lineColor(0.0f, 1.0f, 0.0f); // Green frustum

	m_frustumVertices.clear();
	for (int i = 0; i < 12; i++)
	{
		m_frustumVertices.push_back({ worldCorners[edges[i][0]], lineColor });
		m_frustumVertices.push_back({ worldCorners[edges[i][1]], lineColor });
	}
	m_debugLineVertexCount = static_cast<uint32_t>(m_frustumVertices.size());
}

void LodSample::Draw()
{
	PHX::STATUS_CODE phxRes;

	PHX::ClearValues clearColor{};
	clearColor.color.color = PHX::Vec4f(0.15f, 0.15f, 0.2f, 1.0f);
	clearColor.useClearColor = true;

	PHX::ClearValues clearDepth{};
	clearDepth.depthStencil.depthClear = 1.0f;
	clearDepth.depthStencil.stencilClear = 0;
	clearDepth.useClearColor = false;

	m_renderGraph.BeginFrame(m_swapChain);

	// Upload instance data if dirty
	if (m_instancesDirty)
	{
		m_lodManager.SetInstances(m_instances);
		m_lodManager.UploadInstances(m_renderGraph, m_swapChain);
		m_instancesDirty = false;
	}

	// Zero draw args/count buffers before the compute pass
	m_lodManager.ZeroDrawBuffers(m_renderGraph, m_swapChain);

	// COMPUTE PASS — LOD cull + select
	{
		PHX::RenderPassHandle computePass;
		phxRes = m_renderGraph.RegisterPass("LodCull", PHX::PASS_TYPE::COMPUTE, computePass);
		if (phxRes != PHX::STATUS_CODE::SUCCESS) return;

		computePass.SetBufferInput(m_lodManager.GetInstanceBuffer());
		computePass.SetBufferInput(m_lodManager.GetLodLevelBuffer());
		computePass.SetBufferInput(m_lodManager.GetLodGroupBuffer());
		// Args/count are both read (InterlockedAdd) and written by the compute shader,
		// so declare them as both input and output. This ensures the render graph
		// inserts a barrier between the transfer pass (which zeroes them) and this pass.
		computePass.SetBufferInput(m_lodManager.GetArgsBuffer());
		computePass.SetBufferInput(m_lodManager.GetCountBuffer());
		computePass.SetBufferOutput(m_lodManager.GetArgsBuffer());
		computePass.SetBufferOutput(m_lodManager.GetCountBuffer());
		computePass.SetBufferOutput(m_lodManager.GetInstanceIndexListBuffer());
		computePass.SetPipelineDescription(m_computePipelineDesc);

		computePass.SetExecuteCallback([this](PHX::DeviceContextHandle deviceContext)
		{
			// Culling is ALWAYS performed from the LOD/frustum camera ("player"), regardless
			// of which camera is currently active. This is what lets the observer camera fly
			// around and inspect the frozen culling result.
			Common::LodCameraData camData{};
			camData.viewMatrix = glm::transpose(m_pLodCamera->GetViewMatrix()); // Row-major for GPU
			camData.projMatrix = glm::transpose(GetProjectionMatrix());
			camData.cameraPos = glm::vec4(m_pLodCamera->GetPosition(), 0.0f);
			camData.screenParams = glm::vec4(
				float(m_swapChain.GetWidth()),
				float(m_swapChain.GetHeight()),
				glm::radians(kFovDegrees),
				float(m_swapChain.GetWidth()) / float(m_swapChain.GetHeight())
			);
			camData.instanceCount = m_lodManager.GetInstanceCount();
			camData.groupCount = m_lodManager.GetGroupCount();

			m_lodManager.CullAndSelectGPU(deviceContext, m_computeUniforms, camData, m_useIndirectCount);
		});
	}

	// GRAPHICS PASS — render LOD meshes
	{
		PHX::RenderPassHandle graphicsPass;
		phxRes = m_renderGraph.RegisterPass("LodMeshRender", PHX::PASS_TYPE::GRAPHICS, graphicsPass);
		if (phxRes != PHX::STATUS_CODE::SUCCESS) return;

		graphicsPass.SetTextureOutput(m_swapChain.GetCurrentImage(), PHX::ATTACHMENT_LOAD_OP::CLEAR, PHX::ATTACHMENT_STORE_OP::STORE, clearColor);
		graphicsPass.SetDepthOutput(m_depthBuffer);

		graphicsPass.SetBufferInput(m_lodManager.GetInstanceBuffer());
		graphicsPass.SetBufferInput(m_lodManager.GetInstanceIndexListBuffer());
		graphicsPass.SetBufferInput(m_lodManager.GetArgsBuffer());
		if (m_useIndirectCount)
		{
			graphicsPass.SetBufferInput(m_lodManager.GetCountBuffer());
		}

		// Update pipeline (wireframe toggle)
		m_meshPipelineDesc.polygonMode = m_wireframe ? PHX::POLYGON_MODE::LINE : PHX::POLYGON_MODE::FILL;
		graphicsPass.SetPipelineDescription(m_meshPipelineDesc);

		graphicsPass.SetExecuteCallback([this](PHX::DeviceContextHandle deviceContext)
		{
			// Render from the active camera's perspective. Culling is done from the LOD camera
			// in the compute pass, but the user views the scene from whichever camera is active.
			Common::LodCameraData camData{};
			camData.viewMatrix = glm::transpose(m_pCamera->GetViewMatrix());
			camData.projMatrix = glm::transpose(GetProjectionMatrix());
			camData.cameraPos = glm::vec4(m_pCamera->GetPosition(), 0.0f);
			camData.screenParams = glm::vec4(
				float(m_swapChain.GetWidth()),
				float(m_swapChain.GetHeight()),
				glm::radians(kFovDegrees),
				float(m_swapChain.GetWidth()) / float(m_swapChain.GetHeight())
			);
			camData.instanceCount = m_lodManager.GetInstanceCount();
			camData.groupCount = m_lodManager.GetGroupCount();

			deviceContext.CopyDataToBuffer(m_lodManager.GetRenderCameraBuffer(), &camData, sizeof(Common::LodCameraData));

			// Bind mesh uniforms
			m_meshUniforms.QueueBufferUpdate(m_lodManager.GetInstanceBuffer(), 0, 0, 0);
			m_meshUniforms.QueueBufferUpdate(m_lodManager.GetInstanceIndexListBuffer(), 0, 1, 0);
			m_meshUniforms.QueueBufferUpdate(m_lodManager.GetRenderCameraBuffer(), 0, 2, 0);
			deviceContext.FlushUniformUpdates(m_meshUniforms);
			deviceContext.BindUniformCollection(m_meshUniforms);

			deviceContext.SetScissor({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
			deviceContext.SetViewport({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });

			m_lodManager.DrawIndirect(deviceContext, m_useIndirectCount);
		});
	}

	// FRUSTUM VISUALIZATION — upload + draw the LOD camera's frustum wireframe.
	// Rebuilt every frame from the LOD camera so it tracks the "player" while that camera
	// is active, and stays frozen (the LOD camera is not updated) while observing.
	if (m_showFrustum)
	{
		ComputeFrustumVertices();

		// Upload the frustum line vertices via a transfer pass (registered inside the
		// frame so the render graph can order it before the draw below).
		{
			PHX::RenderPassHandle uploadPass;
			phxRes = m_renderGraph.RegisterPass("FrustumUpload", PHX::PASS_TYPE::TRANSFER, uploadPass);
			if (phxRes != PHX::STATUS_CODE::SUCCESS) return;

			uploadPass.SetBufferOutput(m_debugLineVertexBuffer);
			uploadPass.SetExecuteCallback([this](PHX::DeviceContextHandle deviceContext)
			{
				deviceContext.CopyDataToBuffer(m_debugLineVertexBuffer, m_frustumVertices.data(),
					m_frustumVertices.size() * sizeof(DebugLineVertex));
			});
		}

		// Draw the frustum wireframe from the ACTIVE camera's viewpoint.
		PHX::RenderPassHandle debugPass;
		phxRes = m_renderGraph.RegisterPass("FrustumViz", PHX::PASS_TYPE::GRAPHICS, debugPass);
		if (phxRes != PHX::STATUS_CODE::SUCCESS) return;

		debugPass.SetTextureOutput(m_swapChain.GetCurrentImage(), PHX::ATTACHMENT_LOAD_OP::LOAD, PHX::ATTACHMENT_STORE_OP::STORE);
		debugPass.SetBufferInput(m_debugLineVertexBuffer);
		debugPass.SetPipelineDescription(m_debugLinePipelineDesc);

		debugPass.SetExecuteCallback([this](PHX::DeviceContextHandle deviceContext)
		{
			m_debugCameraData.viewMatrix = glm::transpose(m_pCamera->GetViewMatrix());
			m_debugCameraData.projMatrix = glm::transpose(GetProjectionMatrix());
			deviceContext.CopyDataToBuffer(m_debugCameraBuffer, &m_debugCameraData, sizeof(DebugCameraData));

			m_debugLineUniforms.QueueBufferUpdate(m_debugCameraBuffer, 0, 0, 0);
			deviceContext.FlushUniformUpdates(m_debugLineUniforms);
			deviceContext.BindUniformCollection(m_debugLineUniforms);

			deviceContext.SetScissor({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
			deviceContext.SetViewport({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });

			deviceContext.BindVertexBuffer(m_debugLineVertexBuffer);
			deviceContext.Draw(m_debugLineVertexCount);
		});
	}

	// IMGUI PASS
	ImGui::Render();
	m_imguiRenderer.RenderDrawData(m_renderGraph, m_swapChain, ImGui::GetDrawData(), false);

	m_renderGraph.Bake(m_swapChain);

	GenerateRenderGraphVisualization("LodSample");

	m_renderGraph.EndFrame(m_swapChain);
}
