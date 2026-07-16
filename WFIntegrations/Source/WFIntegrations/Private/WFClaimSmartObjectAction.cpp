// Copyright NoOpArmy 2024


#include "WFClaimSmartObjectAction.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectBlueprintFunctionLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Actor.h"

void UWFClaimSmartObjectAction::OnActivate_Implementation()
{
	Super::OnActivate_Implementation();

	AActor* Self = GetControlledActor();
	if (!IsValid(Subsystem) || !IsValid(Self) || SearchRadius <= 0.0f)
	{
		Finished();
		return;
	}

	const FVector Center = Self->GetActorLocation();

	FSmartObjectRequestFilter Filter;
	Filter.BehaviorDefinitionClasses = BehaviorDefinitionClasses;
	Filter.ActivityRequirements = ActivityRequirements;
	Filter.UserTags = UserTags;

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
		USmartObjectBlueprintFunctionLibrary::SetValueAsSOClaimHandle(Blackboard, ClaimHandleKey, Claim);

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
	if (IsValid(GetControlledActor()))
		Blackboard = GetControlledActor()->FindComponentByClass<UBlackboardComponent>();
}
