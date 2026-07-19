#pragma once

#include "integral_types.h"
#include "input_attribute.h"
#include "PHX/interface/shader.h"
#include "PHX/interface/uniform.h"
#include "shader_desc.h"
#include "texture_desc.h"
#include "vec_types.h"

namespace PHX
{
	enum class PRIMITIVE_TOPOLOGY
	{
		POINT_LIST = 0,
		LINE_LIST,
		LINE_STRIP,
		TRIANGLE_LIST,
		TRIANGLE_STRIP,
		TRIANGLE_FAN,

		MAX
	};

	enum class POLYGON_MODE
	{
		FILL = 0,
		LINE,
		POINT,

		MAX
	};

	enum class CULL_MODE
	{
		NONE = 0,
		FRONT,
		BACK,
		FRONT_AND_BACK,

		MAX
	};

	enum class FRONT_FACE_WINDING
	{
		COUNTER_CLOCKWISE = 0,
		CLOCKWISE,

		MAX
	};

	enum class COMPARE_OP
	{
		NEVER = 0,
		LESS,
		EQUAL,
		LESS_OR_EQUAL,
		GREATER,
		NOT_EQUAL,
		GREATER_OR_EQUAL,
		ALWAYS,

		MAX
	};

	enum class STENCIL_OP
	{
		KEEP = 0,
		ZERO,
		REPLACE,
		INCREMENT_AND_CLAMP,
		DECREMENT_AND_CLAMP,
		INVERT,
		INCREMENT_AND_WRAP,
		DECREMENT_AND_WRAP,

		MAX
	};

	struct StencilOpState
	{
		STENCIL_OP failOp;
		STENCIL_OP passOp;
		STENCIL_OP depthFailOp;
		COMPARE_OP compareOp;
		u32 compareMask;
		u32 writeMask;
		u32 reference;
	};

	enum class BLEND_FACTOR
	{
		ZERO = 0,
		ONE,
		SRC_COLOR,
		ONE_MINUS_SRC_COLOR,
		DST_COLOR,
		ONE_MINUS_DST_COLOR,
		SRC_ALPHA,
		ONE_MINUS_SRC_ALPHA,
		DST_ALPHA,
		ONE_MINUS_DST_ALPHA,
		CONSTANT_COLOR,
		ONE_MINUS_CONSTANT_COLOR,
		CONSTANT_ALPHA,
		ONE_MINUS_CONSTANT_ALPHA,
		SRC_ALPHA_SATURATE,

		MAX
	};

	enum class BLEND_OP
	{
		ADD = 0,
		SUBTRACT,
		REVERSE_SUBTRACT,
		MIN_VALUE,
		MAX_VALUE,

		MAX
	};

	enum COLOR_COMPONENT_FLAG : u8
	{
		COLOR_COMPONENT_FLAG_NONE = 0,
		COLOR_COMPONENT_FLAG_R    = (1 << 0),
		COLOR_COMPONENT_FLAG_G    = (1 << 1),
		COLOR_COMPONENT_FLAG_B    = (1 << 2),
		COLOR_COMPONENT_FLAG_A    = (1 << 3),
	};
	using ColorComponentFlags = u8;

	struct BlendState
	{
		bool                enableBlend        = false;
		BLEND_FACTOR        srcColorFactor     = BLEND_FACTOR::SRC_ALPHA;
		BLEND_FACTOR        dstColorFactor     = BLEND_FACTOR::ONE_MINUS_SRC_ALPHA;
		BLEND_OP            colorBlendOp       = BLEND_OP::ADD;
		BLEND_FACTOR        srcAlphaFactor     = BLEND_FACTOR::ONE;
		BLEND_FACTOR        dstAlphaFactor     = BLEND_FACTOR::ONE_MINUS_SRC_ALPHA;
		BLEND_OP            alphaBlendOp       = BLEND_OP::ADD;
		ColorComponentFlags colorWriteMask     = COLOR_COMPONENT_FLAG_R | COLOR_COMPONENT_FLAG_G | COLOR_COMPONENT_FLAG_B | COLOR_COMPONENT_FLAG_A;
	};

	struct HitGroupDesc
	{
		u32 closestHitShaderIndex   = U32_MAX; // Index into pShaders, or U32_MAX if none
		u32 anyHitShaderIndex       = U32_MAX; // Index into pShaders, or U32_MAX if none
		u32 intersectionShaderIndex = U32_MAX; // Index into pShaders, or U32_MAX if none
	};

	struct GraphicsPipelineDesc
	{
		// Input assembler
		PRIMITIVE_TOPOLOGY topology					= PRIMITIVE_TOPOLOGY::TRIANGLE_STRIP;
		bool enableRestartPrimitives				= false;

		// Vertex input layout
		InputAttribute* pInputAttributes			= nullptr;
		u32 attributeCount							= 0;
		u32 inputBinding							= 0;
		VERTEX_INPUT_RATE inputRate					= VERTEX_INPUT_RATE::PER_VERTEX;

		// Viewport info
		Vec2u viewportPos							= { 0, 0 };
		Vec2u viewportSize							= { 0, 0 };
		Vec2f viewportDepthRange					= { 0.0f, 1.0f };

		// Scissor info
		Vec2u scissorOffset							= { 0, 0 };
		Vec2u scissorExtent							= { 0, 0 };

		// Rasterizer state
		bool enableDepthClamp						= false;
		bool enableRasterizerDiscard				= false;
		POLYGON_MODE polygonMode					= POLYGON_MODE::FILL;
		CULL_MODE cullMode							= CULL_MODE::BACK;
		FRONT_FACE_WINDING frontFaceWinding			= FRONT_FACE_WINDING::COUNTER_CLOCKWISE;
		bool enableDepthBias						= false;
		float depthBiasConstantFactor				= 0.0f;
		float depthBiasClamp						= 0.0f;
		float depthBiasSlopeFactor					= 0.0f;
		float lineWidth								= 1.0f;

		// Multi-sampling state
		SAMPLE_COUNT rasterizationSamples			= SAMPLE_COUNT::COUNT_1;
		bool enableAlphaToCoverage					= false;
		bool enableAlphaToOne						= false;

		// Depth stencil state
		bool enableDepthTest						= false;
		bool enableDepthWrite						= false;
		COMPARE_OP compareOp						= COMPARE_OP::LESS_OR_EQUAL;
		bool enableDepthBoundsTest					= false;
		bool enableStencilTest						= false;
		StencilOpState stencilFront					= { };
		StencilOpState stencilBack					= { };
		Vec2f depthBoundsRange						= { 0.0f, 1.0f };

		// Color blend state
		BlendState blendState                       = { };

		// Pipeline layout
		UniformCollectionHandle uniformCollection	= INVALID_HANDLE;

		// Shader create info
		ShaderHandle* pShaders						= nullptr;
		u32 shaderCount								= 0;

		////////
		bool GraphicsPipelineDesc::operator==(const GraphicsPipelineDesc& other) const;
		////////
	};

	struct ComputePipelineDesc
	{
		ShaderHandle shader = INVALID_HANDLE;
		UniformCollectionHandle uniformCollection = INVALID_HANDLE;

		////////
		bool operator==(const ComputePipelineDesc& other) const;
		////////
	};

	struct RayTracingPipelineDesc
	{
		ShaderHandle* pShaders = nullptr;
		u32 shaderCount = 0;

		// May be used to combine hit shaders into a single SBT hit group, if necessary
		// Allows things like alpha discarding since any-hit and closest-hit can be grouped into a single hit group
		HitGroupDesc* pHitGroups = nullptr;
		u32 hitGroupCount = 0;

		UniformCollectionHandle uniformCollection = INVALID_HANDLE;

		////////
		bool operator==(const RayTracingPipelineDesc& other) const;
		////////
	};
}