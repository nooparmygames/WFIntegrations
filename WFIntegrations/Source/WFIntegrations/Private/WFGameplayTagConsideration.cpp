// Copyright NoOpArmy 2024


#include "WFGameplayTagConsideration.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Actor.h"

float UWFGameplayTagConsideration::GetValue_Implementation(const AActor* InTargetActor)
{
	// Resolve the current actor each time, including components added after possession.
	AbilitySystem = IsValid(GetControlledActor())
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetControlledActor())
		: nullptr;

	if (!IsValid(AbilitySystem) || Tags.IsEmpty())
		return 0.0f;

	const bool bHasTags = bRequireAll
		? AbilitySystem->HasAllMatchingGameplayTags(Tags)
		: AbilitySystem->HasAnyMatchingGameplayTags(Tags);

	const bool bResult = bInvert ? !bHasTags : bHasTags;
	return bResult ? 1.0f : 0.0f;
}

void UWFGameplayTagConsideration::OnBeginPlay_Implementation()
{
	Super::OnBeginPlay_Implementation();
	AbilitySystem = IsValid(GetControlledActor())
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetControlledActor())
		: nullptr;
}

void UWFGameplayTagConsideration::OnPossessionChanged_Implementation()
{
	Super::OnPossessionChanged_Implementation();
	AbilitySystem = IsValid(GetControlledActor())
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetControlledActor())
		: nullptr;
}
