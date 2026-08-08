#pragma once

#include <vector>

#include "BSL/integral_types.h"
#include "BSL/serialization.h"
#include "PHX/types/shader_desc.h"
#include "PHX/types/status_code.h"

namespace PHX
{
	// Serialize a CompiledShader into a PHXS byte blob
	STATUS_CODE SerializeCompiledShader(const CompiledShader& shader, SHADER_STAGE stage, std::vector<u8>& outBlob);

	// Deserialize a PHXS byte blob into a CompiledShader
	STATUS_CODE DeserializeCompiledShader(const u8* data, u64 size, CompiledShader& outShader, SHADER_STAGE& outStage);
}
