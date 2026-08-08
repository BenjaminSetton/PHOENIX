#include "shader_serialization.h"

#include <cstring>
#include <sstream>

#include "BSL/logger.h"
#include "BSL/serialization.h"

using namespace BSL;

namespace PHX
{
	// PHXS magic number and format version
	static constexpr u32 PHXS_MAGIC = BSL::MakeMagicNumber("PHXS");
	static constexpr u32 PHXS_VERSION = 1;

	static void WriteIOEntry(std::ostream& os, const ShaderIOData& io)
	{
		WriteString(os, io.name ? std::string(io.name) : std::string());
		WriteTrivial(os, static_cast<u32>(io.format));
		WriteTrivial(os, io.location);
		WriteTrivial(os, io.binding);
	}

	static void ReadIOEntry(std::istream& is, ShaderIOData& io, std::string& outName)
	{
		outName = ReadString(is);
		io.format   = static_cast<BASE_FORMAT>(ReadTrivial<u32>(is));
		io.location = ReadTrivial<u32>(is);
		io.binding  = ReadTrivial<u32>(is);
	}

	STATUS_CODE SerializeCompiledShader(const CompiledShader& shader, SHADER_STAGE stage, std::vector<u8>& outBlob)
	{
		std::ostringstream os(std::ios::binary);

		// Magic + version
		WriteTrivial(os, PHXS_MAGIC);
		WriteTrivial(os, PHXS_VERSION);

		// Reflection blob
		WriteTrivial(os, static_cast<u8>(stage));
		u8 hasReflection = shader.reflectionData.isValid ? 1 : 0;
		WriteTrivial(os, hasReflection);

		if (shader.reflectionData.isValid)
		{
			const ShaderReflectionData& r = shader.reflectionData;

			// Uniforms
			WriteTrivial(os, r.uniformCount);
			for (u32 i = 0; i < r.uniformCount; i++)
			{
				WriteString(os, r.uniforms[i].name ? std::string(r.uniforms[i].name) : std::string());
				WriteTrivial(os, r.uniforms[i].stages);
				WriteTrivial(os, r.uniforms[i].size);
				WriteTrivial(os, r.uniforms[i].binding);
				WriteTrivial(os, r.uniforms[i].offset);
			}

			// Inputs
			WriteTrivial(os, r.inputCount);
			for (u32 i = 0; i < r.inputCount; i++)
			{
				WriteIOEntry(os, r.inputs[i]);
			}

			// Outputs
			WriteTrivial(os, r.outputCount);
			for (u32 i = 0; i < r.outputCount; i++)
			{
				WriteIOEntry(os, r.outputs[i]);
			}

			// Local size
			WriteTrivial(os, r.localSize.GetX());
			WriteTrivial(os, r.localSize.GetY());
			WriteTrivial(os, r.localSize.GetZ());
		}

		// Shader bytecode
		u32 bytecodeWordCount = shader.size;
		WriteTrivial(os, bytecodeWordCount);
		if (bytecodeWordCount > 0 && shader.data != nullptr)
		{
			os.write(reinterpret_cast<const char*>(shader.data.get()), static_cast<std::streamsize>(bytecodeWordCount * sizeof(u32)));
		}

		// Copy stream contents into output blob
		std::string str = os.str();
		outBlob.assign(reinterpret_cast<const u8*>(str.data()), reinterpret_cast<const u8*>(str.data()) + str.size());

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeserializeCompiledShader(const u8* data, u64 size, CompiledShader& outShader, SHADER_STAGE& outStage)
	{
		if (data == nullptr || size == 0)
		{
			LogError("Failed to deserialize PHXS blob. Data is null or empty");
			return STATUS_CODE::ERR_API;
		}

		std::string buf(reinterpret_cast<const char*>(data), size);
		std::istringstream is(buf, std::ios::binary);

		// Magic + version
		u32 magic = ReadTrivial<u32>(is);
		if (magic != PHXS_MAGIC)
		{
			LogError("Failed to deserialize PHXS blob: magic mismatch (expected 0x%08X, got 0x%08X)", PHXS_MAGIC, magic);
			return STATUS_CODE::ERR_API;
		}

		u32 version = ReadTrivial<u32>(is);
		if (version != PHXS_VERSION)
		{
			LogError("Failed to deserialize PHXS blob: version mismatch (expected %u, got %u)", PHXS_VERSION, version);
			return STATUS_CODE::ERR_API;
		}

		// Reflection blob
		outStage = static_cast<SHADER_STAGE>(ReadTrivial<u8>(is));
		u8 hasReflection = ReadTrivial<u8>(is);

		if (hasReflection)
		{
			ShaderReflectionData& r = outShader.reflectionData;

			// Read uniforms (store names as std::string first, then pack into arena)
			r.uniformCount = ReadTrivial<u32>(is);
			std::vector<std::string> uniformNames;
			if (r.uniformCount > 0)
			{
				r.uniforms = std::shared_ptr<ShaderUniformData[]>(new ShaderUniformData[r.uniformCount]);
				uniformNames.resize(r.uniformCount);
				for (u32 i = 0; i < r.uniformCount; i++)
				{
					uniformNames[i] = ReadString(is);
					r.uniforms[i].stages  = ReadTrivial<u32>(is);
					r.uniforms[i].size    = ReadTrivial<u32>(is);
					r.uniforms[i].binding = ReadTrivial<u32>(is);
					r.uniforms[i].offset  = ReadTrivial<u32>(is);
				}
			}

			// Read inputs
			r.inputCount = ReadTrivial<u32>(is);
			std::vector<std::string> inputNames;
			if (r.inputCount > 0)
			{
				r.inputs = std::shared_ptr<ShaderIOData[]>(new ShaderIOData[r.inputCount]);
				inputNames.resize(r.inputCount);
				for (u32 i = 0; i < r.inputCount; i++)
				{
					ReadIOEntry(is, r.inputs[i], inputNames[i]);
				}
			}

			// Read outputs
			r.outputCount = ReadTrivial<u32>(is);
			std::vector<std::string> outputNames;
			if (r.outputCount > 0)
			{
				r.outputs = std::shared_ptr<ShaderIOData[]>(new ShaderIOData[r.outputCount]);
				outputNames.resize(r.outputCount);
				for (u32 i = 0; i < r.outputCount; i++)
				{
					ReadIOEntry(is, r.outputs[i], outputNames[i]);
				}
			}

			// Local size
			u32 lx = ReadTrivial<u32>(is);
			u32 ly = ReadTrivial<u32>(is);
			u32 lz = ReadTrivial<u32>(is);
			r.localSize = Vec3u(lx, ly, lz);

			r.isValid = true;

			// Build string arena
			// Compute total size needed (each string + null terminator)
			u64 arenaSize = 0;
			for (const std::string& s : uniformNames) arenaSize += s.size() + 1;
			for (const std::string& s : inputNames)   arenaSize += s.size() + 1;
			for (const std::string& s : outputNames)  arenaSize += s.size() + 1;

			if (arenaSize > 0)
			{
				auto arena = std::make_shared<std::vector<char>>(arenaSize);
				u64 offset = 0;

				auto packString = [&](const std::string& s, const char*& outPtr)
				{
					if (s.empty())
					{
						outPtr = nullptr;
						return;
					}
					std::memcpy(arena->data() + offset, s.c_str(), s.size() + 1);
					outPtr = arena->data() + offset;
					offset += s.size() + 1;
				};

				for (u32 i = 0; i < r.uniformCount; i++)
				{
					packString(uniformNames[i], r.uniforms[i].name);
				}
				for (u32 i = 0; i < r.inputCount; i++)
				{
					packString(inputNames[i], r.inputs[i].name);
				}
				for (u32 i = 0; i < r.outputCount; i++)
				{
					packString(outputNames[i], r.outputs[i].name);
				}

				r.stringArena = arena;
			}
		}

		// Shader bytecode
		u32 bytecodeWordCount = ReadTrivial<u32>(is);
		if (bytecodeWordCount > 0)
		{
			outShader.size = bytecodeWordCount;
			outShader.data = std::shared_ptr<u32[]>(new u32[bytecodeWordCount]);
			is.read(reinterpret_cast<char*>(outShader.data.get()), static_cast<std::streamsize>(bytecodeWordCount * sizeof(u32)));
			if (!is.good())
			{
				LogError("Failed to deserialize PHXS blob. Shader bytecode is corrupt!");
				return STATUS_CODE::ERR_INTERNAL;
			}
		}
		else
		{
			outShader.size = 0;
			outShader.data = nullptr;
		}

		return STATUS_CODE::SUCCESS;
	}
}
