#pragma once

#include <string>
#include <vector>

enum class ShaderType
{
	Vertex,
	Fragment,
	Compute
};


void CompileShader(const std::string& file, std::vector<unsigned int>& spirv, ShaderType type);