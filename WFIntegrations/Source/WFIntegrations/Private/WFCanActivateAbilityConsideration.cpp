// Copyright NoOpArmy 2024


#include "WFCanActivateAbilityConsideration.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbilitySpec.h"
#include "GameFramework/Actor.h"

float UWFCanActivateAbilityConsideration::GetValue_Implementation(const AActor* InTargetActor)
{
	// Resolve the current actor each time, including components added after possession.
	AbilitySystem = IsValid(GetControlledActor())
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetControlledActor())
		: nullptr;

	bool bCanActivate = false;
	if (IsValid(AbilitySystem) && AbilityClass != nullptr)
	{
		if (const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(AbilityClass))
		{
			if (IsValid(Spec->Ability))
				bCanActivate = Spec->Ability->CanActivateAbility(Spec->Handle, AbilitySystem->AbilityActorInfo.Get());
		}
	}

	const bool bResult = bInvert ? !bCanActivate : bCanActivate;
	return bResult ? 1.0f : 0.0f;
}

void UWFCanActivateAbilityConsideration::OnBeginPlay_Implementation()
{
	Super::OnBeginPlay_Implementation();
	AbilitySystem = IsValid(GetControlledActor())
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetControlledActor())
		: nullptr;
}

void UWFCanActivateAbilityConsideration::OnPossessionChanged_Implementation()
{
	Super::OnPossessionChanged_Implementation();
	AbilitySystem = IsValid(GetControlledActor())
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetControlledActor())
		: nullptr;
}
