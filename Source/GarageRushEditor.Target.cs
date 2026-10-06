using UnrealBuildTool;
using System.Collections.Generic;

public class GarageRushEditorTarget : TargetRules
{
    public GarageRushEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        ExtraModuleNames.Add("GarageRush");
    }
}
