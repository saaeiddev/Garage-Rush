using UnrealBuildTool;

public class GarageRush : ModuleRules
{
    public GarageRush(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.NoPCHs; // Portable core has no injected UE macros.
        CppStandard = CppStandardVersion.Cpp20;
        bEnableExceptions = true; // Bounded save decoder catches exceptions.
        bUseUnity = false; // Keep platform save implementation isolated.
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
            "UMG", "ChaosVehicles", "PhysicsCore"
        });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore" });
    }
}
