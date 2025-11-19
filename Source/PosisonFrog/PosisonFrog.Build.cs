// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PosisonFrog : ModuleRules
{
	public PosisonFrog(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add(ModuleDirectory);

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput",
			"AnimGraphRuntime",
			"NavigationSystem",
			"GameplayTasks",
			"GameplayTags",
			"AIModule",
			"UMG",
			"Niagara",
			"Slate",        
            "SlateCore",
            "RHI",
            "RenderCore",
            "SkillBorderFx",
            "MediaAssets",
            "Media",
            "LevelSequence",    
            "MovieScene",
            "NiagaraAnimNotifies"
		});
		
		PrivateDependencyModuleNames.AddRange(new string[] 
		{
			"Slate", 
			"SlateCore",   
			"UMG",
			"ToolMenus",
			"GameplayCameras",
		});
	}
}
