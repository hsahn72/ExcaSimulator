using UnrealBuildTool;

public class ExcaSimulator : ModuleRules
{
	public ExcaSimulator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"HeadMountedDisplay",
			"XRBase",
			"ProceduralMeshComponent"
		});
	}
}
