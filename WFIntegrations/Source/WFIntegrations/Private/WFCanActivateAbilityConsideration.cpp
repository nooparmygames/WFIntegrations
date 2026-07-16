// Copyright NoOpArmy 2024


#include "WFCanActivateAbilityConsideration.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbilitySpec.h"
#include "GameFramework/Actor.h"

float UWFCanActivateAbilityConsideration::GetValue_Implementation(const AActor* InTargetActor)
{
	// Late-fetch the ASC in case it was not ready at begin play.
	if (!IsValid(AbilitySystem) && IsValid(GetControlledActor()))
		AbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetControlledActor());

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
	if (IsValid(GetControlledActor()))
		AbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetControlledActor());
}
