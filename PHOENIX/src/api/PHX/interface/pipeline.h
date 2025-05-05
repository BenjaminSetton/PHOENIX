#pragma once

#include "../types/pipeline_desc.h"
#include "../types/shader_desc.h"
#include "../types/texture_desc.h"
#include "../types/input_attribute.h"
#include "../types/vec_types.h"
#include "shader.h"
#include "uniform.h"
#include "framebuffer.h"

namespace PHX
{
	struct ComputePipelineCreateInfo
	{
		IShader* pShader						= nullptr;
		IUniformCollection* pUniformCollection	= nullptr;
	};

	// TODO?
	// - Dynamic state
	// - Viewport state
	// - Color blending state
	struct GraphicsPipelineCreateInfo
	{
		// Input assembler
		PRIMITIVE_TOPOLOGY topology				= PRIMITIVE_TOPOLOGY::TRIANGLE_STRIP;
		bool enableRestartPrimitives			= false;

		// Vertex input layout
		InputAttribute* pInputAttributes		= nullptr;
		u32 attributeCount						= 0;
		u32 inputBinding						= 0;
		VERTEX_INPUT_RATE inputRate				= VERTEX_INPUT_RATE::PER_VERTEX;

		// Viewport info
		Vec2u viewportPos						= { 0, 0 };
		Vec2u viewportSize						= { 0, 0 };
		Vec2f viewportDepthRange				= { 0.0f, 1.0f };

		// Scissor info
		Vec2u scissorOffset						= { 0, 0 };
		Vec2u scissorExtent						= { 0, 0 };

		// Rasterizer state
		bool enableDepthClamp					= false;
		bool enableRasterizerDiscard			= false;
		POLYGON_MODE polygonMode				= POLYGON_MODE::FILL;
		CULL_MODE cullMode						= CULL_MODE::BACK;
		FRONT_FACE_WINDING frontFaceWinding		= FRONT_FACE_WINDING::COUNTER_CLOCKWISE;
		bool enableDepthBias					= false;
		float depthBiasConstantFactor			= 0.0f;
		float depthBiasClamp					= 0.0f;
		float depthBiasSlopeFactor				= 0.0f;
		float lineWidth							= 1.0f;

		// Multi-sampling state
		SAMPLE_COUNT rasterizationSamples		= SAMPLE_COUNT::COUNT_1;
		bool enableAlphaToCoverage				= false;
		bool enableAlphaToOne					= false;

		// Depth stencil state
		bool enableDepthTest					= true;
		bool enableDepthWrite					= true;
		COMPARE_OP compareOp					= COMPARE_OP::LESS_OR_EQUAL;
		bool enableDepthBoundsTest				= true;
		bool enableStencilTest					= false;
		StencilOpState stencilFront				= { };
		StencilOpState stencilBack				= { };
		Vec2f depthBoundsRange					= { 0.0f, 1.0f };

		// Pipeline layout
		IUniformCollection* pUniformCollection	= nullptr;

		// Shader create info
		IShader** ppShaders						= nullptr;
		u32 shaderCount							= 0;
	};

	class IPipeline
	{
	public:

		virtual ~IPipeline() { }
	};
}