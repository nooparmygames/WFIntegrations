// Copyright NoOpArmy 2024

#pragma once

#include "CoreMinimal.h"
#include "WFAction.h"
#include "GameplayTagContainer.h"
#include "SmartObjectTypes.h"
#include "WFClaimSmartObjectAction.generated.h"

class USmartObjectSubsystem;
class USmartObjectBehaviorDefinition;
class UBlackboardComponent;

/**
 * Finds a Smart Object near the controlled actor and reserves (claims) one of its slots, then finishes immediately.
 * The claim handle is stored in a blackboard key of type "SO Claim Handle" so a following UWFUseSmartObjectAction can
 * occupy it and a UWFReleaseSmartObjectAction can free it. Optionally the reserved slot's location is written to a
 * Vector key for a MoveTo action.
 *
 * Requires the blackboard to contain a key of type SOClaimHandle named ClaimHandleKey.
 */
UCLASS(Blueprintable, BlueprintType)
class WFINTEGRATIONS_API UWFClaimSmartObjectAction : public UWFAction
{
	GENERATED_BODY()

public:
	virtual void OnBeginPlay_Implementation() override;
	virtual void OnActivate_Implementation() override;

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

	/** Priority used when claiming a slot. Higher priority claims can steal lower priority ones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	ESmartObjectClaimPriority ClaimPriority = ESmartObjectClaimPriority::Normal;

	/** Blackboard key (type SO Claim Handle) the resulting claim is written to. Must be set for this action to be useful. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	FName ClaimHandleKey = NAME_None;

	/** Optional Vector blackboard key the reserved slot's world location is written to. None disables writing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	FName SlotLocationKey = NAME_None;

protected:
	UPROPERTY(Transient)
	TObjectPtr<USmartObjectSubsystem> Subsystem;

	UPROPERTY(Transient)
	TObjectPtr<UBlackboardComponent> Blackboard;
};
