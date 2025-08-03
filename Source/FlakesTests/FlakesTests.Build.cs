// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

using UnrealBuildTool;

public class FlakesTests : ModuleRules
{
	public FlakesTests(ReadOnlyTargetRules Target) : base(Target)
	{
		Flakes.ApplySharedModuleSetup(this, Target);

		PublicDependencyModuleNames.AddRange(
			new []
			{
				"Core",
				"Flakes"
			});

		PrivateDependencyModuleNames.AddRange(
			new []
			{
				"CoreUObject",
				"Engine",
				"GameplayTags",
			});
	}
}