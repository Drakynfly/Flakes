﻿// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

using UnrealBuildTool;

public class Flakes : ModuleRules
{
	public Flakes(ReadOnlyTargetRules Target) : base(Target)
	{
		ApplySharedModuleSetup(this, Target);

		PublicDependencyModuleNames.AddRange(
			new []
			{
				"Core"
			});

		PrivateDependencyModuleNames.AddRange(
			new []
			{
				"CoreUObject",
				"Engine"
			});
	}

	public static void ApplySharedModuleSetup(ModuleRules Module, ReadOnlyTargetRules Target)
	{
		Module.PCHUsage = PCHUsageMode.NoPCHs;
		Module.DefaultBuildSettings = BuildSettingsVersion.Latest;
		Module.IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		// This is to emulate engine installation and verify includes during development
		if (Target.Configuration == UnrealTargetConfiguration.DebugGame
		    || Target.Configuration == UnrealTargetConfiguration.Debug)
		{
			Module.bUseUnity = false;
			Module.bTreatAsEngineModule = true;
			Module.CppCompileWarningSettings.NonInlinedGenCppWarningLevel = WarningLevel.Warning;
			Module.CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Warning;
		}
	}
}