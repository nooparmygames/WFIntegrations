// Copyright NoOpArmy 2024

#pragma once

#include "CoreMinimal.h"
#include "WFConsideration.h"
#include "GameplayTagContainer.h"
#include "WFSmartObjectConsideration.generated.h"

class USmartObjectSubsystem;
class USmartObjectBehaviorDefinition;
class UBlackboardComponent;

/**
 * Scores whether a usable Smart Object is available near the controlled actor.
 * Returns 0 when none is found. When one is found it returns 1, or a distance based score
 * (1 when on top of it, 0 at SearchRadius) if bScoreByDistance is set.
 *
 * This consideration is read-only: it does not claim/reserve anything. Use a
 * UWFClaimSmartObjectAction / UWFUseSmartObjectAction to actually reserve and use the object.
 * Optionally it writes the found slot's world location to a Vector blackboard key so a MoveTo action can use it.
 */
UCLASS(Blueprintable, BlueprintType)
class WFINTEGRATIONS_API UWFSmartObjectConsideration : public UWFConsideration
{
	GENERATED_BODY()

public:

	virtual void OnBeginPlay_Implementation() override;
	virtual float GetValue_Implementation(const AActor* InTargetActor) override;

public:

	/** Only Smart Objects that provide one of these behavior definition classes are considered. Empty means any. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	TArray<TSubclassOf<USmartObjectBehaviorDefinition>> BehaviorDefinitionClasses;

	/** Optional activity/tag requirements the Smart Object must satisfy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	FGameplayTagQuery ActivityRequirements;

	/** Tags describing this user, matched against each Smart Object's user tag filter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	FGameplayTagContainer UserTags;

	/** Search half-extent (radius) around the controlled actor, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	float SearchRadius = 1000.0f;

	/** If true the score scales with distance (closer is higher). If false it returns 1 whenever any object is found. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	bool bScoreByDistance = true;

	/** Optional Vector blackboard key the found slot location is written to, for a follow-up MoveTo. None disables writing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	FName ResultLocationKey = NAME_None;

protected:
	UPROPERTY(Transient)
	TObjectPtr<USmartObjectSubsystem> Subsystem;

	UPROPERTY(Transient)
	TObjectPtr<UBlackboardComponent> Blackboard;
};
