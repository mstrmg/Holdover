using UnrealBuildTool;
using System.Collections.Generic;

public class HoldoverTarget : TargetRules
{
	public HoldoverTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Holdover");
	}
}
