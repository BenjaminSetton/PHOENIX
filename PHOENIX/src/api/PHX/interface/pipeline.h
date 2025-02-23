#pragma once

#include "../types/pipeline_desc.h"
#include "../types/shader_desc.h"
#include "../types/texture_desc.h"
#include "../types/input_attribute.h"
#include "shader.h"
#include "uniform.h"
#include "framebuffer.h"

namespace PHX
{
	struct PipelineCreateInfo
	{
		PIPELINE_TYPE type;

		// Input assembler
		PRIMITIVE_TOPOLOGY topology;
		bool enableRestartPrimitives;

		// Vertex input layout
		InputAttribute* pInputAttributes;
		u32 attributeCount;
		u32 inputBinding;
		VERTEX_INPUT_RATE inputRate;

		// Viewport info
		float viewportX;
		float viewportY;
		float viewportWidth;
		float viewportHeight;
		float viewportMinDepth;
		float viewportMaxDepth;

		// Scissor info
		float scissorOffsetX;
		float scissorOffsetY;
		float scissorExtentX;
		float scissorExtentY;

		// Dynamic state
		// ...

		// Viewport state
		// ...

		// Rasterizer state
		bool enableDepthClamp;
		bool enableRasterizerDiscard;
		POLYGON_MODE polygonMode;
		CULL_MODE cullMode;
		FRONT_FACE_WINDING frontFaceWinding;
		bool enableDepthBias;
		float depthBiasConstantFactor;
		float depthBiasClamp;
		float depthBiasSlopeFactor;
		float lineWidth;

		// Multi-sampling state
		SAMPLE_COUNT rasterizationSamples;
		bool enableAlphaToCoverage;
		bool enableAlphaToOne;

		// Color blending state
		// ...

		// Depth stencil state
		bool enableDepthTest;
		bool enableDepthWrite;
		COMPARE_OP compareOp;
		bool enableDepthBoundsTest;
		bool enableStencilTest;
		StencilOpState stencilFront;
		StencilOpState stencilBack;
		float minDepthBounds;
		float maxDepthBounds;

		// Pipeline layout
		IUniformCollection* pUniformCollection;

		// Shader create info
		IShader** ppShaders;
		u32 shaderCount;

		// Attachment info
		IFramebuffer* pFramebuffer;
	};

	class IPipeline
	{
	public:

		virtual ~IPipeline() { }
	};
}