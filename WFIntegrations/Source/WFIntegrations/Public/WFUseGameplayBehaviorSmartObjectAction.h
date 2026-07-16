// Copyright NoOpArmy 2024

#pragma once

#include "CoreMinimal.h"
#include "WFAction.h"
#include "GameplayTagContainer.h"
#include "SmartObjectTypes.h"
#include "SmartObjectRuntime.h"
#include "WFUseGameplayBehaviorSmartObjectAction.generated.h"

class USmartObjectSubsystem;
class USmartObjectBehaviorDefinition;
class UAITask_UseGameplayBehaviorSmartObject;

/**
 * Finds a Smart Object that exposes a GameplayBehavior definition, claims a slot, then runs the engine's
 * UAITask_UseGameplayBehaviorSmartObject so the pawn moves to the slot and actually performs the object's
 * GameplayBehavior (animation, montage, etc.). This is the "run the object's real behavior" counterpart to the
 * plain UWFUseSmartObjectAction which only reserves/occupies.
 *
 * The engine task owns the claim once started and frees it when the behavior finishes or the task is cancelled, so
 * this action never double-frees. Requires a pawn possessed by an AIController.
 */
UCLASS(Blueprintable, BlueprintType)
class WFINTEGRATIONS_API UWFUseGameplayBehaviorSmartObjectAction : public UWFAction
{
	GENERATED_BODY()

public:
	UWFUseGameplayBehaviorSmartObjectAction();

	virtual void OnBeginPlay_Implementation() override;
	virtual void OnActivate_Implementation() override;
	virtual void TickAction_Implementation(float DeltaTime) override;
	virtual void OnDeactivate_Implementation() override;

public:
	/** Behavior definition classes the Smart Object must provide. Defaults to GameplayBehavior definitions. Empty means any. */
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

	/** Priority used when claiming the slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	ESmartObjectClaimPriority ClaimPriority = ESmartObjectClaimPriority::Normal;

	/** If true the pawn moves to the slot before running the behavior; if false it runs on the spot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	bool bMoveToSlot = true;

	/** Adds the AI logic resource to the task's claimed resources, pausing other AI logic while the behavior runs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	bool bLockAILogic = true;

protected:
	UPROPERTY(Transient)
	TObjectPtr<USmartObjectSubsystem> Subsystem;

private:
	/** The engine task performing the move + gameplay behavior. */
	TWeakObjectPtr<UAITask_UseGameplayBehaviorSmartObject> Task;

	/** The slot we claimed. */
	FSmartObjectClaimHandle ClaimHandle;

	/** True while we still own the claim (before it is handed to the task). */
	bool bOwnsClaim = false;
};
