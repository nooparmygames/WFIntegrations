// Copyright NoOpArmy 2026
#include "WFGasLifecycleTestTypes.h"
#include "AbilitySystemComponent.h"

UWFGasLifecycleTestAbility::UWFGasLifecycleTestAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UWFGasLifecycleTestAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // Stay active until the scenario ends this exact ability instance.
}

void UWFGasLifecycleTestAbility::FinishForTest()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
