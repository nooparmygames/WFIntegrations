// Copyright NoOpArmy 2026
using UnrealBuildTool;

public class WFIntegrationsTests : ModuleRules
{
    public WFIntegrationsTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "AIModule", "NavigationSystem", "GameplayTags",
            "SmartObjectsModule", "WiseFeline", "WiseFelineInfluenceMaps", "WFIntegrations",
            "GameplayTasks", "GameplayAbilities", "GameplayBehaviorsModule", "GameplayBehaviorSmartObjectsModule"
        });
    }
}
