using UnrealBuildTool;

public class ExcaSimulatorTarget : TargetRules
{
	public ExcaSimulatorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ExcaSimulator");
	}
}
