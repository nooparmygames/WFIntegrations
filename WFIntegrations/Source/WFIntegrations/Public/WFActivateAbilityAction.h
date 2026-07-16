// Copyright NoOpArmy 2024

#pragma once

#include "CoreMinimal.h"
#include "WFAction.h"
#include "WFActivateAbilityAction.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;

/**
 * Activates a Gameplay Ability (by class) on the controlled actor's AbilitySystemComponent when selected.
 * If bFinishWhenAbilityEnds is true the action stays active until an ability of that class ends; otherwise it finishes
 * as soon as the activation is requested. Finishes immediately if there is no ability system, no class, or activation fails.
 */
UCLASS(Blueprintable, BlueprintType)
class WFINTEGRATIONS_API UWFActivateAbilityAction : public UWFAction
{
	GENERATED_BODY()

public:
	virtual void OnBeginPlay_Implementation() override;
	virtual void OnActivate_Implementation() override;
	virtual void OnDeactivate_Implementation() override;

public:
	/** The gameplay ability class to activate. Must already be granted to the ability system component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|GAS")
	TSubclassOf<UGameplayAbility> AbilityClass;

	/** Passed through to TryActivateAbilityByClass; allows the activation to be predicted/remotely activated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|GAS")
	bool bAllowRemoteActivation = true;

	/** If true, the action remains active until an ability of AbilityClass ends. If false, it finishes right after activating. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|GAS")
	bool bFinishWhenAbilityEnds = true;

protected:
	/** Called by the ability system when any ability ends; finishes the action when it is one of AbilityClass. */
	void OnAbilityEnded(UGameplayAbility* Ability);

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

private:
	FDelegateHandle AbilityEndedHandle;
};
