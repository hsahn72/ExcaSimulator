using UnrealBuildTool;

public class ExcaSimulatorEditorTarget : TargetRules
{
	public ExcaSimulatorEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ExcaSimulator");
	}
}
