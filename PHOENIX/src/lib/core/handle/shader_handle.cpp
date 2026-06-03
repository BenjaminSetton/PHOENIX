
#include "PHX/interface/shader.h"
#include "PHX/interface/render_device.h"

#include "utils/sanity.h"

namespace PHX
{
	static const ShaderReflectionData s_defaultReflectionData;

	ShaderHandle::ShaderHandle() : Handle(HANDLE_TYPE::RENDER_PASS)
	{
	}

	ShaderHandle::ShaderHandle(const Handle& base) : Handle(base)
	{
	}

	ShaderHandle::~ShaderHandle()
	{
	}

	ShaderHandle::ShaderHandle(const ShaderHandle& other) : Handle(other)
	{
	}

	ShaderHandle& ShaderHandle::operator=(const ShaderHandle& other)
	{
		if (this == &other)
		{
			return *this;
		}

		Handle::operator=(other);
		return *this;
	}

	ShaderHandle::ShaderHandle(ShaderHandle&& other) noexcept : Handle(std::move(other))
	{
	}

	SHADER_STAGE ShaderHandle::GetStage() const
	{
		IShader* pShader = m_pRenderDevice->ResolveHandle(*this);
		if (pShader != nullptr)
		{
			return pShader->GetStage();
		}

		ASSERT_ALWAYS("Failed to get shader stage. Could not resolve shader handle!");
		return SHADER_STAGE::MAX;
	}

	const ShaderReflectionData& ShaderHandle::GetReflectionData() const
	{
		IShader* pShader = m_pRenderDevice->ResolveHandle(*this);
		if (pShader != nullptr)
		{
			return pShader->GetReflectionData();
		}

		ASSERT_ALWAYS("Failed to get shader reflection data. Could not resolve shader handle!");
		return s_defaultReflectionData;
	}
}