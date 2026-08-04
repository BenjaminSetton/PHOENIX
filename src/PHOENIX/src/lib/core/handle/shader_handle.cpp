
#include "PHX/interface/render_device.h"

#include "core/handle/handle_utils.h"
#include "core/interface_types/shader_interface.h"
#include "utils/sanity.h"

namespace PHX
{
	static const ShaderReflectionData s_defaultReflectionData;

	DEFINE_PHX_HANDLE(ShaderHandle, HANDLE_TYPE::SHADER)

	SHADER_STAGE ShaderHandle::GetStage() const
	{
		IShader* pShader = HANDLE_UTILS::ResolveHandle(*this);
		if (pShader != nullptr)
		{
			return pShader->GetStage();
		}

		ASSERT_ALWAYS("Failed to get shader stage. Could not resolve shader handle!");
		return SHADER_STAGE::MAX;
	}

	const ShaderReflectionData& ShaderHandle::GetReflectionData() const
	{
		IShader* pShader = HANDLE_UTILS::ResolveHandle(*this);
		if (pShader != nullptr)
		{
			return pShader->GetReflectionData();
		}

		ASSERT_ALWAYS("Failed to get shader reflection data. Could not resolve shader handle!");
		return s_defaultReflectionData;
	}
}