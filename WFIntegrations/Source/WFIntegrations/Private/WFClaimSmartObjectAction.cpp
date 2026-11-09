// Copyright NoOpArmy 2024


#include "WFClaimSmartObjectAction.h"
#include "WFUtilities.h"
#include "WFSmartObjectUtilities.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectBlueprintFunctionLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BlackboardKeyType_SOClaimHandle.h"
#include "GameFramework/Actor.h"

void UWFClaimSmartObjectAction::OnActivate_Implementation()
{
	// Blackboard setup can finish after possession. Resolve it again before use.
	Blackboard = WFUtilities::FindBlackboardComponent(GetControlledActor());

	Super::OnActivate_Implementation();

	AActor* Self = GetControlledActor();
	if (!IsValid(Subsystem) || !IsValid(Self) || SearchRadius <= 0.0f
		|| !IsValid(Blackboard) || ClaimHandleKey == NAME_None
		|| Blackboard->GetKeyType(Blackboard->GetKeyID(ClaimHandleKey)) != UBlackboardKeyType_SOClaimHandle::StaticClass())
	{
		Finished();
		return;
	}

	// Do not overwrite a live reservation and lose the only handle used to release it.
	const FSmartObjectClaimHandle Existing = USmartObjectBlueprintFunctionLibrary::GetValueAsSOClaimHandle(Blackboard, ClaimHandleKey);
	if (WFSmartObjectUtilities::HasLiveSlot(Subsystem, Existing))
	{
		Finished();
		return;
	}

	const FVector Center = Self->GetActorLocation();

	FSmartObjectRequestFilter Filter;
	Filter.BehaviorDefinitionClasses = BehaviorDefinitionClasses;
	Filter.ActivityRequirements = ActivityRequirements;
	Filter.UserTags = UserTags;
	Filter.ClaimPriority = ClaimPriority;

	const FSmartObjectRequest Request(FBox::BuildAABB(Center, FVector(SearchRadius)), Filter);
	const FSmartObjectRequestResult Result = Subsystem->FindSmartObject(Request, Self);
	if (!Result.IsValid())
	{
		Finished();
		return;
	}

	const FSmartObjectClaimHandle Claim = USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsClaimed(Self, Result.SlotHandle, Self, ClaimPriority);
	if (!Claim.IsValid())
	{
		Finished();
		return;
	}

	if (ClaimHandleKey != NAME_None && IsValid(Blackboard))
	{
		WFSmartObjectUtilities::ClearPublicationOnRelease(Subsystem, Claim, Blackboard, ClaimHandleKey);
		USmartObjectBlueprintFunctionLibrary::SetValueAsSOClaimHandle(Blackboard, ClaimHandleKey, Claim);
	}

	if (SlotLocationKey != NAME_None && IsValid(Blackboard))
	{
		FVector SlotLocation;
		if (Subsystem->GetSlotLocation(Claim, SlotLocation))
			Blackboard->SetValueAsVector(SlotLocationKey, SlotLocation);
	}

	Finished();
}

void UWFClaimSmartObjectAction::OnBeginPlay_Implementation()
{
	Super::OnBeginPlay_Implementation();
	Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	Blackboard = WFUtilities::FindBlackboardComponent(GetControlledActor());
}

void UWFClaimSmartObjectAction::OnPossessionChanged_Implementation()
{
	Super::OnPossessionChanged_Implementation();
	Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	Blackboard = WFUtilities::FindBlackboardComponent(GetControlledActor());
}
