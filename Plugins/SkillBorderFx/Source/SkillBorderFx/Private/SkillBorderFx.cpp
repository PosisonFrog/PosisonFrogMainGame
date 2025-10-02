// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkillBorderFx.h"

#include "Interfaces/IPluginManager.h"
#include "CSimpleSkillBorderCS.h"

#define LOCTEXT_NAMESPACE "FSkillBorderFxModule"

void FSkillBorderFxModule::StartupModule()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SkillBorderFx"));
	if (!Plugin.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Plugin not found"));
		return;
	}
    
	FString ShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/SkillBorderFx"), ShaderDir);
	UE_LOG(LogTemp, Log, TEXT("Shader mapping: /Plugin/SkillBorderFx -> %s"), *ShaderDir);
}

void FSkillBorderFxModule::ShutdownModule()
{
}

// 콘솔 명령어 (테스트용)
static FAutoConsoleCommand TestShaderCommand(
	TEXT("Test.CheckShader"),
	TEXT("Check if shader is registered"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		TShaderMapRef<CSimpleSkillBorderCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		if (Shader.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("Shader is VALID"));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Shader is INVALID"));
			UE_LOG(LogTemp, Log, TEXT("Trying to get shader map details..."));

			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			if (ShaderMap)
			{
				UE_LOG(LogTemp, Log, TEXT("Shader map exists"));
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("Shader map is NULL"));
			}
		}
	})
);
	
IMPLEMENT_MODULE(FSkillBorderFxModule, SkillBorderFx)