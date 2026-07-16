// Copyright NoOpArmy 2024


#include "WFUseSmartObjectAction.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectBlueprintFunctionLibrary.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Actor.h"

UWFUseSmartObjectAction::UWFUseSmartObjectAction()
{
	bShouldTick = true;
}

void UWFUseSmartObjectAction::OnActivate_Implementation()
{
	Super::OnActivate_Implementation();

	bMovingToSlot = false;
	bUsing = false;
	bOwnsClaim = false;
	ClaimHandle = FSmartObjectClaimHandle::InvalidHandle;

	AActor* Self = GetControlledActor();
	if (!IsValid(Subsystem) || !IsValid(Self))
	{
		Finished();
		return;
	}

	// Reuse an existing claim from the blackboard if one is present and valid.
	if (ClaimHandleKey != NAME_None && IsValid(Blackboard))
	{
		const FSmartObjectClaimHandle Existing = USmartObjectBlueprintFunctionLibrary::GetValueAsSOClaimHandle(Blackboard, ClaimHandleKey);
		if (Existing.IsValid())
		{
			ClaimHandle = Existing;
			bOwnsClaim = false; // someone else reserved it; a Release action is responsible for freeing it
		}
	}

	// Otherwise find and claim our own slot.
	if (!ClaimHandle.IsValid())
	{
		if (SearchRadius <= 0.0f)
		{
			Finished();
			return;
		}

		FSmartObjectRequestFilter Filter;
		if (BehaviorDefinitionClass != nullptr)
			Filter.BehaviorDefinitionClasses.Add(BehaviorDefinitionClass);
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

		ClaimHandle = USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsClaimed(Self, Result.SlotHandle, Self, ClaimPriority);
		if (!ClaimHandle.IsValid())
		{
			Finished();
			return;
		}
		bOwnsClaim = true;

		if (ClaimHandleKey != NAME_None && IsValid(Blackboard))
			USmartObjectBlueprintFunctionLibrary::SetValueAsSOClaimHandle(Blackboard, ClaimHandleKey, ClaimHandle);
	}

	// Resolve where the slot is; fall back to using in place if we cannot get a location.
	if (!Subsystem->GetSlotLocation(ClaimHandle, SlotLocation))
	{
		BeginUsing();
		return;
	}

	AAIController* Controller = GetAIController();
	if (!bMoveToSlot || !IsValid(Controller) || Controller->GetPawn() == nullptr)
	{
		BeginUsing();
		return;
	}

	// MoveToLocation fails gracefully (no assert) when there is no navigation data.
	const EPathFollowingRequestResult::Type MoveResult = Controller->MoveToLocation(SlotLocation, AcceptanceRadius);
	if (MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		BeginUsing();
	}
	else if (MoveResult == EPathFollowingRequestResult::RequestSuccessful)
	{
		bMovingToSlot = true;
	}
	else // Failed - cannot reach the slot, so give up (OnDeactivate releases the claim).
	{
		Finished();
	}
}

void UWFUseSmartObjectAction::TickAction_Implementation(float DeltaTime)
{
	Super::TickAction_Implementation(DeltaTime);

	if (bMovingToSlot)
	{
		const AActor* Self = GetControlledActor();
		if (IsValid(Self) && FVector::Dist(Self->GetActorLocation(), SlotLocation) <= AcceptanceRadius)
		{
			bMovingToSlot = false;
			BeginUsing();
		}
		return;
	}

	if (bUsing && UseDuration > 0.0f)
	{
		RemainingUseTime -= DeltaTime;
		if (RemainingUseTime <= 0.0f)
			Finished();
	}
}

void UWFUseSmartObjectAction::OnDeactivate_Implementation()
{
	Super::OnDeactivate_Implementation();

	// Only free what we reserved ourselves; externally provided claims are released by a Release action.
	if (bOwnsClaim && ClaimHandle.IsValid())
	{
		USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsFree(GetControlledActor(), ClaimHandle);
		if (ClaimHandleKey != NAME_None && IsValid(Blackboard))
			USmartObjectBlueprintFunctionLibrary::SetValueAsSOClaimHandle(Blackboard, ClaimHandleKey, FSmartObjectClaimHandle::InvalidHandle);
	}

	ClaimHandle.Invalidate();
	bMovingToSlot = false;
	bUsing = false;
	bOwnsClaim = false;
}

void UWFUseSmartObjectAction::BeginUsing()
{
	if (BehaviorDefinitionClass != nullptr && ClaimHandle.IsValid())
		USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsOccupied(GetControlledActor(), ClaimHandle, BehaviorDefinitionClass);

	bUsing = true;
	RemainingUseTime = UseDuration;
}

void UWFUseSmartObjectAction::OnBeginPlay_Implementation()
{
	Super::OnBeginPlay_Implementation();
	Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	if (IsValid(GetControlledActor()))
		Blackboard = GetControlledActor()->FindComponentByClass<UBlackboardComponent>();
}
