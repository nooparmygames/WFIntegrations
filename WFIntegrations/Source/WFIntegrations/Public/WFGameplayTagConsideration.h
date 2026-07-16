// Copyright NoOpArmy 2024

#pragma once

#include "CoreMinimal.h"
#include "WFConsideration.h"
#include "GameplayTagContainer.h"
#include "WFGameplayTagConsideration.generated.h"

class UAbilitySystemComponent;

/**
 * Returns 1 when the controlled actor's AbilitySystemComponent owns the configured gameplay tags, 0 otherwise.
 * Set bRequireAll to require every tag, or clear it to match any of them. bInvert flips the result so you can gate
 * an action on the ABSENCE of a tag (e.g. "not stunned"). Returns 0 if there is no ability system or Tags is empty.
 */
UCLASS(Blueprintable, BlueprintType)
class WFINTEGRATIONS_API UWFGameplayTagConsideration : public UWFConsideration
{
	GENERATED_BODY()

public:

	virtual void OnBeginPlay_Implementation() override;
	virtual float GetValue_Implementation(const AActor* InTargetActor) override;

public:

	/** The gameplay tags to look for on the ability system component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|GAS")
	FGameplayTagContainer Tags;

	/** If true, all tags must be present; if false, any one of them matches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|GAS")
	bool bRequireAll = true;

	/** Invert the result (returns 1 when the tags are NOT matched). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|GAS")
	bool bInvert = false;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;
};
