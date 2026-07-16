// Copyright NoOpArmy 2024

#pragma once

#include "CoreMinimal.h"
#include "WFConsideration.h"
#include "AttributeSet.h"
#include "WFGameplayAttributeConsideration.generated.h"

class UAbilitySystemComponent;

/**
 * Scores based on a Gameplay Ability System attribute on the controlled actor's AbilitySystemComponent.
 * The attribute's current value is normalized to 0-1 by dividing it either by a fixed MaxValue or by the value of
 * another attribute (e.g. Health / MaxHealth). Returns 0 if there is no ability system or the attribute is missing.
 */
UCLASS(Blueprintable, BlueprintType)
class WFINTEGRATIONS_API UWFGameplayAttributeConsideration : public UWFConsideration
{
	GENERATED_BODY()

public:

	virtual void OnBeginPlay_Implementation() override;
	virtual float GetValue_Implementation(const AActor* InTargetActor) override;

public:

	/** The attribute whose current value is scored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|GAS")
	FGameplayAttribute Attribute;

	/** If true, the value is divided by MaxAttribute's value (e.g. Health/MaxHealth). If false it is divided by MaxValue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|GAS")
	bool bNormalizeByAttribute = false;

	/** The attribute used as the denominator when bNormalizeByAttribute is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|GAS", meta = (EditCondition = "bNormalizeByAttribute", EditConditionHides))
	FGameplayAttribute MaxAttribute;

	/** The value that maps to a score of 1 when bNormalizeByAttribute is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|GAS", meta = (EditCondition = "!bNormalizeByAttribute", EditConditionHides))
	float MaxValue = 1.0f;

	/** Invert the score (e.g. to make low health score high). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|GAS")
	bool bInvert = false;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;
};
