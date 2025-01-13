// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealSandboxTerrain : ModuleRules {
	public UnrealSandboxTerrain(ReadOnlyTargetRules Target) : base(Target) {

		//PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[] { "Core" });
		PrivateDependencyModuleNames.AddRange(new string[] { "CoreUObject", "Engine", "Slate", "SlateCore", "RenderCore", "RHI" });
        PrivateDependencyModuleNames.AddRange(new string[] { "Json", "JsonUtilities", "Networking", "Sockets" });
    }
}
