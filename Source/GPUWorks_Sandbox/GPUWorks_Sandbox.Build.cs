// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GPUWorks_Sandbox : ModuleRules
{
	public GPUWorks_Sandbox(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] 
		{ 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput",

            "ProceduralMeshComponent",

			"GPUWorks"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
