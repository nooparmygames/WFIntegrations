// Copyright NoOpArmy 2024

#pragma once

#include "CoreMinimal.h"
#include "WFConsideration.h"
#include "WFCanActivateAbilityConsideration.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;

/**
 * Returns 1 when a Gameplay Ability (by class) can currently be activated on the controlled actor's
 * AbilitySystemComponent - i.e. it is granted and passes its cost, cooldown, and activation-tag checks - and 0
 * otherwise. Pair it with a UWFActivateAbilityAction so the brain only picks the action when the ability is ready.
 * Set bInvert to score high when the ability CANNOT be activated (e.g. to pick a different action while on cooldown).
 */
UCLASS(Blueprintable, BlueprintType)
class WFINTEGRATIONS_API UWFCanActivateAbilityConsideration : public UWFConsideration
{
	GENERATED_BODY()

public:

	virtual void OnBeginPlay_Implementation() override;
	virtual float GetValue_Implementation(const AActor* InTargetActor) override;

public:

	/** The gameplay ability class to test. Must be granted to the ability system component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|GAS")
	TSubclassOf<UGameplayAbility> AbilityClass;

	/** Invert the result (returns 1 when the ability can NOT be activated). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|GAS")
	bool bInvert = false;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;
};
