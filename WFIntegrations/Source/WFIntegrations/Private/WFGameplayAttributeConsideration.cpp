// Copyright NoOpArmy 2024


#include "WFGameplayAttributeConsideration.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Actor.h"

float UWFGameplayAttributeConsideration::GetValue_Implementation(const AActor* InTargetActor)
{
	// Late-fetch the ASC in case it was not ready at begin play.
	if (!IsValid(AbilitySystem) && IsValid(GetControlledActor()))
		AbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetControlledActor());

	if (!IsValid(AbilitySystem) || !Attribute.IsValid())
		return 0.0f;

	bool bFound = false;
	const float Value = AbilitySystem->GetGameplayAttributeValue(Attribute, bFound);
	if (!bFound)
		return 0.0f;

	float Max = MaxValue;
	if (bNormalizeByAttribute)
	{
		if (!MaxAttribute.IsValid())
			return 0.0f;
		Max = AbilitySystem->GetNumericAttribute(MaxAttribute);
	}

	if (Max <= 0.0f)
		return 0.0f;

	const float Normalized = FMath::Clamp(Value / Max, 0.0f, 1.0f);
	return bInvert ? 1.0f - Normalized : Normalized;
}

void UWFGameplayAttributeConsideration::OnBeginPlay_Implementation()
{
	if (IsValid(GetControlledActor()))
		AbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetControlledActor());
}
