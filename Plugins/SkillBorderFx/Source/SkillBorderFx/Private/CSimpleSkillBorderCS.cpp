#include "CSimpleSkillBorderCS.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "ShaderCore.h"

IMPLEMENT_GLOBAL_SHADER(CSimpleSkillBorderCS, "/Plugin/SkillBorderFx/Private/SimpleSkillBorder.usf", "MainCS", SF_Compute);

bool CSimpleSkillBorderCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

void CSimpleSkillBorderCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), 8);
	OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), 8);
}
