#pragma once

#include <string>
#include <vector>
#include <PHX/phx.h>
#include <PHX/interface/shader.h>

namespace Common
{
	// Returns the default include search paths for shared shader include files.
	// Currently returns the path to samples/common/src/shaders/.
	const std::vector<std::string>& GetCommonShaderIncludePath();
}