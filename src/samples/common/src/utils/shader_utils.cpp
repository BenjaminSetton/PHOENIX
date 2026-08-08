
#include <fstream>
#include <sstream>
#include <string>

#include "shader_utils.h"

namespace Common
{
	const std::vector<std::string>& GetCommonShaderIncludePath()
	{
		static const std::vector<std::string> paths = { "../../common/src/shaders" };
		return paths;
	}
}