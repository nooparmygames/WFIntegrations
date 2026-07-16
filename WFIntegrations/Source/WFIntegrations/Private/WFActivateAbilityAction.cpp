// Copyright NoOpArmy 2024


#include "WFActivateAbilityAction.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/Actor.h"

void UWFActivateAbilityAction::OnActivate_Implementation()
{
	Super::OnActivate_Implementation();

	if (!IsValid(AbilitySystem) && IsValid(GetControlledActor()))
		AbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetControlledActor());

	if (!IsValid(AbilitySystem) || AbilityClass == nullptr)
	{
		Finished();
		return;
	}

	// Bind before activating so an ability that ends synchronously is still caught.
	if (bFinishWhenAbilityEnds)
		AbilityEndedHandle = AbilitySystem->AbilityEndedCallbacks.AddUObject(this, &UWFActivateAbilityAction::OnAbilityEnded);

	const bool bActivated = AbilitySystem->TryActivateAbilityByClass(AbilityClass, bAllowRemoteActivation);
	if (!bActivated)
	{
		if (AbilityEndedHandle.IsValid())
		{
			AbilitySystem->AbilityEndedCallbacks.Remove(AbilityEndedHandle);
			AbilityEndedHandle.Reset();
		}
		Finished();
		return;
	}

	// Fire-and-forget: nothing to wait for.
	if (!bFinishWhenAbilityEnds)
		Finished();
}

void UWFActivateAbilityAction::OnAbilityEnded(UGameplayAbility* Ability)
{
	if (IsValid(Ability) && AbilityClass != nullptr && Ability->IsA(AbilityClass))
		Finished();
}

void UWFActivateAbilityAction::OnDeactivate_Implementation()
{
	Super::OnDeactivate_Implementation();

	if (IsValid(AbilitySystem) && AbilityEndedHandle.IsValid())
		AbilitySystem->AbilityEndedCallbacks.Remove(AbilityEndedHandle);
	AbilityEndedHandle.Reset();
}

void UWFActivateAbilityAction::OnBeginPlay_Implementation()
{
	Super::OnBeginPlay_Implementation();
	if (IsValid(GetControlledActor()))
		AbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetControlledActor());
}
