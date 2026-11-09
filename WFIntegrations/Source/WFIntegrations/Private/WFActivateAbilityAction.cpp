// Copyright NoOpArmy 2024


#include "WFActivateAbilityAction.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/Actor.h"

void UWFActivateAbilityAction::OnActivate_Implementation()
{
	Super::OnActivate_Implementation();

	RefreshAbilitySystem();

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
		UnbindAbilityEnded();
		Finished();
		return;
	}

	// Fire-and-forget: nothing to wait for.
	if (!bFinishWhenAbilityEnds)
		Finished();
}

void UWFActivateAbilityAction::OnAbilityEnded(UGameplayAbility* Ability)
{
	if (IsActive() && IsValid(Ability) && AbilityClass != nullptr && Ability->IsA(AbilityClass))
		Finished();
}

void UWFActivateAbilityAction::OnDeactivate_Implementation()
{
	Super::OnDeactivate_Implementation();
	UnbindAbilityEnded();
}

void UWFActivateAbilityAction::UnbindAbilityEnded()
{
	if (IsValid(AbilitySystem) && AbilityEndedHandle.IsValid())
		AbilitySystem->AbilityEndedCallbacks.Remove(AbilityEndedHandle);
	AbilityEndedHandle.Reset();
}

void UWFActivateAbilityAction::RefreshAbilitySystem()
{
	// The old actor can remain alive after possession changes. Detach from its component first.
	UnbindAbilityEnded();
	AbilitySystem = IsValid(GetControlledActor())
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetControlledActor())
		: nullptr;
}

void UWFActivateAbilityAction::OnBeginPlay_Implementation()
{
	Super::OnBeginPlay_Implementation();
	RefreshAbilitySystem();
}

void UWFActivateAbilityAction::OnPossessionChanged_Implementation()
{
	Super::OnPossessionChanged_Implementation();
	Finished();
	RefreshAbilitySystem();
}
