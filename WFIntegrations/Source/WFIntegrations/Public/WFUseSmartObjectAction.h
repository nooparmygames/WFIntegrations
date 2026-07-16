// Copyright NoOpArmy 2024

#pragma once

#include "CoreMinimal.h"
#include "WFAction.h"
#include "GameplayTagContainer.h"
#include "SmartObjectTypes.h"
#include "SmartObjectRuntime.h"
#include "WFUseSmartObjectAction.generated.h"

class USmartObjectSubsystem;
class USmartObjectBehaviorDefinition;
class UBlackboardComponent;

/**
 * Full reserve -> move -> occupy -> wait -> release lifecycle for a Smart Object, driven by the utility brain.
 *
 * On activate it either reuses a valid claim already stored in ClaimHandleKey (e.g. from a UWFClaimSmartObjectAction)
 * or finds and claims its own slot. It then moves the pawn to the slot (needs an AIController) and marks the slot
 * occupied. It waits UseDuration seconds (0 = until the brain switches away or Finished is called), then releases.
 *
 * The claim is always freed in OnDeactivate when this action created it, so switching actions never leaks a reservation.
 * Requires the pawn to be possessed by an AIController when bMoveToSlot is true.
 */
UCLASS(Blueprintable, BlueprintType)
class WFINTEGRATIONS_API UWFUseSmartObjectAction : public UWFAction
{
	GENERATED_BODY()

public:
	UWFUseSmartObjectAction();

	virtual void OnBeginPlay_Implementation() override;
	virtual void OnActivate_Implementation() override;
	virtual void TickAction_Implementation(float DeltaTime) override;
	virtual void OnDeactivate_Implementation() override;

public:
	/** The behavior definition class used both to filter Smart Objects and to occupy the slot. None means any / reserve only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	TSubclassOf<USmartObjectBehaviorDefinition> BehaviorDefinitionClass;

	/** Optional activity/tag requirements the Smart Object must satisfy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	FGameplayTagQuery ActivityRequirements;

	/** Tags describing this user, matched against each Smart Object's user tag filter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	FGameplayTagContainer UserTags;

	/** Search half-extent (radius) around the controlled actor, in cm. Only used when this action finds its own object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	float SearchRadius = 1000.0f;

	/** Priority used when this action claims a slot itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	ESmartObjectClaimPriority ClaimPriority = ESmartObjectClaimPriority::Normal;

	/** Whether to move the pawn to the slot before occupying it. Needs an AIController. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	bool bMoveToSlot = true;

	/** How close (cm) the pawn must be to the slot to count as arrived. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	float AcceptanceRadius = 60.0f;

	/** How long to stay on the object once occupied. 0 means stay until the brain switches away / Finished is called. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	float UseDuration = 3.0f;

	/**
	 * Optional blackboard key of type SO Claim Handle. If it already holds a valid claim, this action uses that claim
	 * (and leaves freeing it to a Release action). Otherwise the action's own claim is written here while active.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	FName ClaimHandleKey = NAME_None;

protected:
	UPROPERTY(Transient)
	TObjectPtr<USmartObjectSubsystem> Subsystem;

	UPROPERTY(Transient)
	TObjectPtr<UBlackboardComponent> Blackboard;

private:
	/** Marks the slot occupied and starts the use timer. */
	void BeginUsing();

	/** The slot we hold. */
	FSmartObjectClaimHandle ClaimHandle;

	/** World location of the slot we are heading to / using. */
	FVector SlotLocation = FVector::ZeroVector;

	/** Time left before finishing when UseDuration > 0. */
	float RemainingUseTime = 0.0f;

	/** True while moving toward the slot. */
	bool bMovingToSlot = false;

	/** True once the slot is occupied and we are counting down UseDuration. */
	bool bUsing = false;

	/** True when this action created the claim itself and is therefore responsible for freeing it. */
	bool bOwnsClaim = false;
};
