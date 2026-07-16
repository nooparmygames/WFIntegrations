// Copyright NoOpArmy 2024


#include "WFGameplayTagConsideration.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Actor.h"

float UWFGameplayTagConsideration::GetValue_Implementation(const AActor* InTargetActor)
{
	// Late-fetch the ASC in case it was not ready at begin play.
	if (!IsValid(AbilitySystem) && IsValid(GetControlledActor()))
		AbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetControlledActor());

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
	if (IsValid(GetControlledActor()))
		AbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetControlledActor());
}
