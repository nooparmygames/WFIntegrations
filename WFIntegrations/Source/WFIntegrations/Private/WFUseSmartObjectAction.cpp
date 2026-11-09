// Copyright NoOpArmy 2024


#include "WFUseSmartObjectAction.h"
#include "WFUtilities.h"
#include "WFSmartObjectUtilities.h"
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
	Cleanup();

	// Blackboard setup can finish after possession. Resolve it again before use.
	Blackboard = WFUtilities::FindBlackboardComponent(GetControlledActor());

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
		if (WFSmartObjectUtilities::HasLiveSlot(Subsystem, Existing))
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
		Filter.ClaimPriority = ClaimPriority;

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

	}

	// Observe releases for both owned and borrowed claims without replacing another
	// owner's single-cast invalidation callback.
	if (FOnSmartObjectEvent* Events = Subsystem->GetSlotEventDelegate(ClaimHandle.SlotHandle))
		SlotEventHandle = Events->AddUObject(this, &UWFUseSmartObjectAction::OnSlotEvent);

	if (bOwnsClaim && ClaimHandleKey != NAME_None && IsValid(Blackboard))
	{
		ClaimBlackboard = Blackboard;
		PublishedClaimKey = ClaimHandleKey;
		USmartObjectBlueprintFunctionLibrary::SetValueAsSOClaimHandle(Blackboard, ClaimHandleKey, ClaimHandle);
		if (!IsActive())
			return;
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
		MovePathFollowing = Controller->GetPathFollowingComponent();
		MoveRequestID = MovePathFollowing.IsValid() ? MovePathFollowing->GetCurrentRequestId() : FAIRequestID::InvalidRequest;
		bMovingToSlot = true;
		if (MovePathFollowing.IsValid())
			MoveFinishedHandle = MovePathFollowing->OnRequestFinished.AddUObject(this, &UWFUseSmartObjectAction::OnMoveCompleted);
	}
	else // Failed - cannot reach the slot, so give up (OnDeactivate releases the claim).
	{
		Finished();
	}
}

void UWFUseSmartObjectAction::TickAction_Implementation(float DeltaTime)
{
	Super::TickAction_Implementation(DeltaTime);

	if (!WFSmartObjectUtilities::HasLiveSlot(Subsystem, ClaimHandle))
	{
		Finished();
		return;
	}

	if (bMovingToSlot)
		return;

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

	Cleanup();
}

void UWFUseSmartObjectAction::StopOwnedMovement()
{
	UPathFollowingComponent* PathFollowing = MovePathFollowing.Get();
	const FAIRequestID RequestID = MoveRequestID;
	if (IsValid(PathFollowing))
		PathFollowing->OnRequestFinished.Remove(MoveFinishedHandle);
	MoveFinishedHandle.Reset();
	MovePathFollowing.Reset();
	MoveRequestID = FAIRequestID::InvalidRequest;
	bMovingToSlot = false;
	if (IsValid(PathFollowing) && RequestID.IsValid() && PathFollowing->GetCurrentRequestId() == RequestID
		&& PathFollowing->GetStatus() != EPathFollowingStatus::Idle)
		PathFollowing->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, RequestID);
}

void UWFUseSmartObjectAction::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (RequestID != MoveRequestID)
		return;

	// Use the navigation result, whose arrival test includes the pawn's radius.
	StopOwnedMovement();
	if (Result.IsSuccess())
		BeginUsing();
	else
		Finished();
}

void UWFUseSmartObjectAction::OnSlotEvent(const FSmartObjectEventData& Event)
{
	if (Event.SlotHandle == ClaimHandle.SlotHandle && Event.Reason == ESmartObjectChangeReason::OnReleased)
	{
		// Release has already happened. Cleanup must not try to release it again or
		// touch the replacement owner's claim during priority takeover.
		bOwnsClaim = false;
		Finished();
	}
}

void UWFUseSmartObjectAction::Cleanup()
{
	// Use the subsystem and publication destination from the old activation. Initialize may
	// already have changed the controlled actor when OnPossessionChanged reaches us.
	USmartObjectSubsystem* OldSubsystem = Subsystem;
	const FSmartObjectClaimHandle OldClaim = ClaimHandle;
	const bool bReleaseClaim = bOwnsClaim;
	UBlackboardComponent* OldBlackboard = ClaimBlackboard.Get();
	const FName OldKey = PublishedClaimKey;
	if (SlotEventHandle.IsValid() && IsValid(OldSubsystem) && OldSubsystem->IsSmartObjectValid(OldClaim.SmartObjectHandle))
		if (FOnSmartObjectEvent* Events = OldSubsystem->GetSlotEventDelegate(OldClaim.SlotHandle))
			Events->Remove(SlotEventHandle);
	SlotEventHandle.Reset();
	ClaimHandle.Invalidate();
	bOwnsClaim = false;
	bUsing = false;
	RemainingUseTime = 0.0f;
	ClaimBlackboard.Reset();
	PublishedClaimKey = NAME_None;

	StopOwnedMovement();
	if (OldClaim.IsValid())
	{
		if (bReleaseClaim && WFSmartObjectUtilities::HasLiveSlot(OldSubsystem, OldClaim))
			OldSubsystem->MarkSlotAsFree(OldClaim);
		// Another system may have replaced the key while this action was active.
		if (IsValid(OldBlackboard) && OldKey != NAME_None
			&& USmartObjectBlueprintFunctionLibrary::GetValueAsSOClaimHandle(OldBlackboard, OldKey) == OldClaim)
			USmartObjectBlueprintFunctionLibrary::SetValueAsSOClaimHandle(OldBlackboard, OldKey, FSmartObjectClaimHandle::InvalidHandle);
	}
}

void UWFUseSmartObjectAction::BeginUsing()
{
	StopOwnedMovement();
	if (BehaviorDefinitionClass != nullptr && ClaimHandle.IsValid())
	{
		if (!USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsOccupied(GetControlledActor(), ClaimHandle, BehaviorDefinitionClass))
		{
			Finished();
			return;
		}
	}

	bUsing = true;
	RemainingUseTime = UseDuration;
}

void UWFUseSmartObjectAction::OnBeginPlay_Implementation()
{
	Super::OnBeginPlay_Implementation();
	Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	Blackboard = WFUtilities::FindBlackboardComponent(GetControlledActor());
}

void UWFUseSmartObjectAction::OnPossessionChanged_Implementation()
{
	Super::OnPossessionChanged_Implementation();
	Finished();
	Cleanup();
	Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	Blackboard = WFUtilities::FindBlackboardComponent(GetControlledActor());
}
