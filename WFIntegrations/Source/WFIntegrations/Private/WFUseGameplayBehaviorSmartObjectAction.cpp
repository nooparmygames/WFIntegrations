// Copyright NoOpArmy 2024


#include "WFUseGameplayBehaviorSmartObjectAction.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectBlueprintFunctionLibrary.h"
#include "AI/AITask_UseGameplayBehaviorSmartObject.h"
#include "GameplayBehaviorSmartObjectBehaviorDefinition.h"
#include "AIController.h"
#include "GameFramework/Actor.h"

UWFUseGameplayBehaviorSmartObjectAction::UWFUseGameplayBehaviorSmartObjectAction()
{
	bShouldTick = true;
	// Only Smart Objects that expose a GameplayBehavior can actually run one, so filter for it by default.
	BehaviorDefinitionClasses.Add(UGameplayBehaviorSmartObjectBehaviorDefinition::StaticClass());
}

void UWFUseGameplayBehaviorSmartObjectAction::OnActivate_Implementation()
{
	Super::OnActivate_Implementation();

	bOwnsClaim = false;
	ClaimHandle = FSmartObjectClaimHandle::InvalidHandle;
	Task.Reset();

	AAIController* Controller = GetAIController();
	AActor* Self = GetControlledActor();
	if (!IsValid(Subsystem) || !IsValid(Controller) || Controller->GetPawn() == nullptr || !IsValid(Self) || SearchRadius <= 0.0f)
	{
		Finished();
		return;
	}

	// Find a matching Smart Object.
	FSmartObjectRequestFilter Filter;
	Filter.BehaviorDefinitionClasses = BehaviorDefinitionClasses;
	Filter.ActivityRequirements = ActivityRequirements;
	Filter.UserTags = UserTags;

	const FVector Center = Self->GetActorLocation();
	const FSmartObjectRequest Request(FBox::BuildAABB(Center, FVector(SearchRadius)), Filter);
	const FSmartObjectRequestResult Result = Subsystem->FindSmartObject(Request, Self);
	if (!Result.IsValid())
	{
		Finished();
		return;
	}

	// Claim a slot; the engine task takes ownership of this handle once created.
	ClaimHandle = USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsClaimed(Self, Result.SlotHandle, Self, ClaimPriority);
	if (!ClaimHandle.IsValid())
	{
		Finished();
		return;
	}
	bOwnsClaim = true;

	UAITask_UseGameplayBehaviorSmartObject* NewTask = bMoveToSlot
		? UAITask_UseGameplayBehaviorSmartObject::MoveToAndUseSmartObjectWithGameplayBehavior(Controller, ClaimHandle, bLockAILogic, ClaimPriority)
		: UAITask_UseGameplayBehaviorSmartObject::UseSmartObjectWithGameplayBehavior(Controller, ClaimHandle, bLockAILogic, ClaimPriority);

	if (!IsValid(NewTask))
	{
		// Task could not be created, so we still own the claim and must free it ourselves.
		USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsFree(Self, ClaimHandle);
		ClaimHandle.Invalidate();
		bOwnsClaim = false;
		Finished();
		return;
	}

	// The task now owns the claim and frees it in its OnDestroy, so we must not free it too.
	Task = NewTask;
	bOwnsClaim = false;
	NewTask->ReadyForActivation();
}

void UWFUseGameplayBehaviorSmartObjectAction::TickAction_Implementation(float DeltaTime)
{
	Super::TickAction_Implementation(DeltaTime);

	if (!Task.IsValid() || Task->IsFinished())
		Finished();
}

void UWFUseGameplayBehaviorSmartObjectAction::OnDeactivate_Implementation()
{
	Super::OnDeactivate_Implementation();

	// Cancelling the task triggers its OnDestroy, which frees the claim it owns.
	if (Task.IsValid() && !Task->IsFinished())
		Task->ExternalCancel();
	Task.Reset();

	// Only free the claim ourselves if the task never took ownership of it.
	if (bOwnsClaim && ClaimHandle.IsValid())
		USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsFree(GetControlledActor(), ClaimHandle);
	ClaimHandle.Invalidate();
	bOwnsClaim = false;
}

void UWFUseGameplayBehaviorSmartObjectAction::OnBeginPlay_Implementation()
{
	Super::OnBeginPlay_Implementation();
	Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
}
