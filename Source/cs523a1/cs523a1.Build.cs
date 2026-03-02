// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class cs523a1 : ModuleRules
{
	public cs523a1(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(new string[] {
				"Core",
				"CoreUObject",
				"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
				"GameplayStateTreeModule",
				"UMG",
				"Slate",
				"SlateCore"
			});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"cs523a1",
			"cs523a1/Variant_Horror",
			"cs523a1/Variant_Horror/UI",
			"cs523a1/Variant_Shooter",
			"cs523a1/Variant_Shooter/AI",
			"cs523a1/Variant_Shooter/UI",
			"cs523a1/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
