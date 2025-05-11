// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CLWorks_Sandbox : ModuleRules
{
	public CLWorks_Sandbox(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] 
		{ 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput",

			"CLWorks"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
