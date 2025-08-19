using UnrealBuildTool;

public class SteamSessionHelper : ModuleRules
{
    public SteamSessionHelper(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "OnlineSubsystem",
            "OnlineSubsystemUtils"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "OnlineSubsystemSteam"
        });
    }
}
