#include "VulkanUtils.h"

#include <cstdio>
#include <iostream>

#include <sstream>

#include <glslang\Public\ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include "glslang/Include/ResourceLimits.h"
#include "DirStackFileIncluder.h"

const TBuiltInResource defaultTBuiltInResource = {
	/* .MaxLights = */ 32,
	/* .MaxClipPlanes = */ 6,
	/* .MaxTextureUnits = */ 32,
	/* .MaxTextureCoords = */ 32,
	/* .MaxVertexAttribs = */ 64,
	/* .MaxVertexUniformComponents = */ 4096,
	/* .MaxVaryingFloats = */ 64,
	/* .MaxVertexTextureImageUnits = */ 32,
	/* .MaxCombinedTextureImageUnits = */ 80,
	/* .MaxTextureImageUnits = */ 32,
	/* .MaxFragmentUniformComponents = */ 4096,
	/* .MaxDrawBuffers = */ 32,
	/* .MaxVertexUniformVectors = */ 128,
	/* .MaxVaryingVectors = */ 8,
	/* .MaxFragmentUniformVectors = */ 16,
	/* .MaxVertexOutputVectors = */ 16,
	/* .MaxFragmentInputVectors = */ 15,
	/* .MinProgramTexelOffset = */ -8,
	/* .MaxProgramTexelOffset = */ 7,
	/* .MaxClipDistances = */ 8,
	/* .MaxComputeWorkGroupCountX = */ 65535,
	/* .MaxComputeWorkGroupCountY = */ 65535,
	/* .MaxComputeWorkGroupCountZ = */ 65535,
	/* .MaxComputeWorkGroupSizeX = */ 1024,
	/* .MaxComputeWorkGroupSizeY = */ 1024,
	/* .MaxComputeWorkGroupSizeZ = */ 64,
	/* .MaxComputeUniformComponents = */ 1024,
	/* .MaxComputeTextureImageUnits = */ 16,
	/* .MaxComputeImageUniforms = */ 8,
	/* .MaxComputeAtomicCounters = */ 8,
	/* .MaxComputeAtomicCounterBuffers = */ 1,
	/* .MaxVaryingComponents = */ 60,
	/* .MaxVertexOutputComponents = */ 64,
	/* .MaxGeometryInputComponents = */ 64,
	/* .MaxGeometryOutputComponents = */ 128,
	/* .MaxFragmentInputComponents = */ 128,
	/* .MaxImageUnits = */ 8,
	/* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
	/* .MaxCombinedShaderOutputResources = */ 8,
	/* .MaxImageSamples = */ 0,
	/* .MaxVertexImageUniforms = */ 0,
	/* .MaxTessControlImageUniforms = */ 0,
	/* .MaxTessEvaluationImageUniforms = */ 0,
	/* .MaxGeometryImageUniforms = */ 0,
	/* .MaxFragmentImageUniforms = */ 8,
	/* .MaxCombinedImageUniforms = */ 8,
	/* .MaxGeometryTextureImageUnits = */ 16,
	/* .MaxGeometryOutputVertices = */ 256,
	/* .MaxGeometryTotalOutputComponents = */ 1024,
	/* .MaxGeometryUniformComponents = */ 1024,
	/* .MaxGeometryVaryingComponents = */ 64,
	/* .MaxTessControlInputComponents = */ 128,
	/* .MaxTessControlOutputComponents = */ 128,
	/* .MaxTessControlTextureImageUnits = */ 16,
	/* .MaxTessControlUniformComponents = */ 1024,
	/* .MaxTessControlTotalOutputComponents = */ 4096,
	/* .MaxTessEvaluationInputComponents = */ 128,
	/* .MaxTessEvaluationOutputComponents = */ 128,
	/* .MaxTessEvaluationTextureImageUnits = */ 16,
	/* .MaxTessEvaluationUniformComponents = */ 1024,
	/* .MaxTessPatchComponents = */ 120,
	/* .MaxPatchVertices = */ 32,
	/* .MaxTessGenLevel = */ 64,
	/* .MaxViewports = */ 16,
	/* .MaxVertexAtomicCounters = */ 0,
	/* .MaxTessControlAtomicCounters = */ 0,
	/* .MaxTessEvaluationAtomicCounters = */ 0,
	/* .MaxGeometryAtomicCounters = */ 0,
	/* .MaxFragmentAtomicCounters = */ 8,
	/* .MaxCombinedAtomicCounters = */ 8,
	/* .MaxAtomicCounterBindings = */ 1,
	/* .MaxVertexAtomicCounterBuffers = */ 0,
	/* .MaxTessControlAtomicCounterBuffers = */ 0,
	/* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
	/* .MaxGeometryAtomicCounterBuffers = */ 0,
	/* .MaxFragmentAtomicCounterBuffers = */ 1,
	/* .MaxCombinedAtomicCounterBuffers = */ 1,
	/* .MaxAtomicCounterBufferSize = */ 16384,
	/* .MaxTransformFeedbackBuffers = */ 4,
	/* .MaxTransformFeedbackInterleavedComponents = */ 64,
	/* .MaxCullDistances = */ 8,
	/* .MaxCombinedClipAndCullDistances = */ 8,
	/* .MaxSamples = */ 4,

	/* .maxMeshOutputVerticesNV = */ 256,
	/* .maxMeshOutputPrimitivesNV = */ 512,
	/* .maxMeshWorkGroupSizeX_NV = */ 32,
	/* .maxMeshWorkGroupSizeY_NV = */ 1,
	/* .maxMeshWorkGroupSizeZ_NV = */ 1,
	/* .maxTaskWorkGroupSizeX_NV = */ 32,
	/* .maxTaskWorkGroupSizeY_NV = */ 1,
	/* .maxTaskWorkGroupSizeZ_NV = */ 1,
	/* .maxMeshViewCountNV = */ 4,

	/* .limits = */
	/* .nonInductiveForLoops = */ 1,
	{
		/* .whileLoops = */ 1,
		/* .doWhileLoops = */ 1,
		/* .generalUniformIndexing = */ 1,
		/* .generalAttributeMatrixVectorIndexing = */ 1,
		/* .generalVaryingIndexing = */ 1,
		/* .generalSamplerIndexing = */ 1,
		/* .generalVariableIndexing = */ 1,
		/* .generalConstantMatrixVectorIndexing = */ 1
	}
};

void ShaderError(const std::string& file, const std::string& error)
{
	printf("SHADER %s: %s", file.c_str(), error.c_str());
	exit(1);
}

EShLanguage GetGlslangShaderType(ShaderType type)
{
	switch (type)
	{
	case ShaderType::Vertex:
		return EShLangVertex;
		break;
	case ShaderType::Fragment:
		return EShLangFragment;
		break;
	case ShaderType::Compute:
		return EShLangCompute;
		break;
	default:
		ShaderError("", "Unkown shader type!");
		return EShLangVertex;
		break;
	}
}

bool UseGlslang(std::vector<unsigned int>& spirv, const std::string& fileName, const char* shaderCode, ShaderType type)
{
	auto shaderStage = GetGlslangShaderType(type);

	// Create TShader and pass input to it
	glslang::TShader shader(shaderStage);
	shader.setStrings(&shaderCode, 1);

	// Set up resources
	int clientInputSemanticsVersion = 100;
	glslang::EShTargetClientVersion vulkanClientVersion = glslang::EShTargetVulkan_1_0;
	glslang::EShTargetLanguageVersion targetVersion = glslang::EShTargetSpv_1_0;

	shader.setEnvInput(glslang::EShSourceGlsl, shaderStage, glslang::EShClientVulkan, clientInputSemanticsVersion);
	shader.setEnvClient(glslang::EShClientVulkan, vulkanClientVersion);
	shader.setEnvTarget(glslang::EShTargetSpv, targetVersion);

	//resources = defaultTBuiltInResource;
	TBuiltInResource resources;
	resources = defaultTBuiltInResource;
	EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

	const int defaultVersion = 100;

	DirStackFileIncluder includer;
	//std::string filepath = getFilePath(path);
	//includer.pushExternalLocalDirectory(filepath);

	std::string preprocessedGLSL;

	if (!shader.preprocess(&resources, defaultVersion, ENoProfile, false, false, messages, &preprocessedGLSL, includer))
	{
		std::stringstream str;

		str << "GLSL Preprocessing Failed for:" << "\n";
		str << shader.getInfoLog() << "\n";
		str << shader.getInfoDebugLog() << "\n";
		ShaderError(fileName, str.str());

		return false;
	}

	const char* preprocessedCStr = preprocessedGLSL.c_str();
	shader.setStrings(&preprocessedCStr, 1);

	// Parse shader
	if (!shader.parse(&resources, 100, false, messages))
	{
		std::stringstream str;

		str << "GLSL parsing failed." << "\n";
		str << shader.getInfoLog() << "\n";
		str << shader.getInfoDebugLog() << "\n";
		ShaderError(fileName, str.str());
		
		return false;
	}

	// Pass to program and link
	glslang::TProgram program;
	program.addShader(&shader);

	if (!program.link(messages))
	{
		std::stringstream str;

		str << "GLSL Linking Failed." << "\n";
		str << shader.getInfoLog() << "\n";
		str << shader.getInfoDebugLog() << "\n";
		ShaderError(fileName, str.str());
		
		return false;
	}

	spv::SpvBuildLogger logger;
	glslang::SpvOptions spvOptions;
	glslang::GlslangToSpv(*program.getIntermediate(shaderStage), spirv, &logger, &spvOptions);

	return true;
}

void CompileShader(const std::string& file, std::vector<unsigned int>& spirv, ShaderType type)
{
	static bool firstRun = true;
	if(firstRun) 
	{
		glslang::InitializeProcess();
		firstRun = false;
	}

	FILE* fhandle;
	fopen_s(&fhandle, file.c_str(), "rb");
	if (fhandle == NULL)
	{
		ShaderError(file, "could not open file");
		return;
	}
	fseek(fhandle, 0, SEEK_END);
	size_t size = ftell(fhandle);
	rewind(fhandle);
	if (size == 0)
	{
		ShaderError(file, "file has no contents");
		return;
	}
	char* source = (char*)malloc(size+1);
	if(source == NULL)
	{
		ShaderError(file, "could not allocate contents");
		return;
	}
	fread(source, 1, size, fhandle);
	fclose(fhandle);
	source[size] = 0;

	UseGlslang(spirv, file, source, type);
}
