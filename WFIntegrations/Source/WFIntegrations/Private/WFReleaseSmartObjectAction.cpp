// Copyright NoOpArmy 2024


#include "WFReleaseSmartObjectAction.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectBlueprintFunctionLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"

void UWFReleaseSmartObjectAction::OnActivate_Implementation()
{
	Super::OnActivate_Implementation();

	AActor* Self = GetControlledActor();
	if (IsValid(Self) && IsValid(Blackboard) && ClaimHandleKey != NAME_None)
	{
		const FSmartObjectClaimHandle Claim = USmartObjectBlueprintFunctionLibrary::GetValueAsSOClaimHandle(Blackboard, ClaimHandleKey);
		if (Claim.IsValid())
		{
			USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsFree(Self, Claim);
			USmartObjectBlueprintFunctionLibrary::SetValueAsSOClaimHandle(Blackboard, ClaimHandleKey, FSmartObjectClaimHandle::InvalidHandle);
		}
	}

	Finished();
}

void UWFReleaseSmartObjectAction::OnBeginPlay_Implementation()
{
	Super::OnBeginPlay_Implementation();
	if (IsValid(GetControlledActor()))
		Blackboard = GetControlledActor()->FindComponentByClass<UBlackboardComponent>();
}
