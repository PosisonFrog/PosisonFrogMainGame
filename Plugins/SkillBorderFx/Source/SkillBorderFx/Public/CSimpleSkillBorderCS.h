#pragma once

#include "GlobalShader.h"
#include "RenderGraphUtils.h"

class CSimpleSkillBorderCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(CSimpleSkillBorderCS);
	SHADER_USE_PARAMETER_STRUCT(CSimpleSkillBorderCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D,              InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState,               InputSampler)
		SHADER_PARAMETER(FVector2f,                          TextureSize)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};
