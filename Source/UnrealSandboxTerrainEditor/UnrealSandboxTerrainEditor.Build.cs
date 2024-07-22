
using UnrealBuildTool;

public class UnrealSandboxTerrainEditor : ModuleRules {
	public UnrealSandboxTerrainEditor(ReadOnlyTargetRules Target) : base(Target) {
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "Engine", "Slate", "SlateCore",	"RenderCore", "RHI", "ToolMenus" } );
	}
}
