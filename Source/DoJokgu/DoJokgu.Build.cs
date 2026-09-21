// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DoJokgu : ModuleRules
{
	public DoJokgu(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"AnimGraphRuntime",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"DoJokgu",
			"DoJokgu/Variant_Platforming",
			"DoJokgu/Variant_Platforming/Animation",
			"DoJokgu/Variant_Combat",
			"DoJokgu/Variant_Combat/AI",
			"DoJokgu/Variant_Combat/Animation",
			"DoJokgu/Variant_Combat/Gameplay",
			"DoJokgu/Variant_Combat/Interfaces",
			"DoJokgu/Variant_Combat/UI",
			"DoJokgu/Variant_SideScrolling",
			"DoJokgu/Variant_SideScrolling/AI",
			"DoJokgu/Variant_SideScrolling/Gameplay",
			"DoJokgu/Variant_SideScrolling/Interfaces",
			"DoJokgu/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
