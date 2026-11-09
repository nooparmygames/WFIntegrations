// Copyright NoOpArmy 2024


#include "WFReleaseSmartObjectAction.h"
#include "WFUtilities.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectBlueprintFunctionLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"

void UWFReleaseSmartObjectAction::OnActivate_Implementation()
{
	// Blackboard setup can finish after possession. Resolve it again before use.
	Blackboard = WFUtilities::FindBlackboardComponent(GetControlledActor());

	Super::OnActivate_Implementation();

	AActor* Self = GetControlledActor();
	if (IsValid(Self) && IsValid(Blackboard) && ClaimHandleKey != NAME_None)
	{
		const FSmartObjectClaimHandle Claim = USmartObjectBlueprintFunctionLibrary::GetValueAsSOClaimHandle(Blackboard, ClaimHandleKey);
		if (Claim.IsValid())
		{
			USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsFree(Self, Claim);
			// Release listeners and blackboard observers can publish a new claim synchronously.
			if (USmartObjectBlueprintFunctionLibrary::GetValueAsSOClaimHandle(Blackboard, ClaimHandleKey) == Claim)
				USmartObjectBlueprintFunctionLibrary::SetValueAsSOClaimHandle(Blackboard, ClaimHandleKey, FSmartObjectClaimHandle::InvalidHandle);
		}
	}

	Finished();
}

void UWFReleaseSmartObjectAction::OnBeginPlay_Implementation()
{
	Super::OnBeginPlay_Implementation();
	Blackboard = WFUtilities::FindBlackboardComponent(GetControlledActor());
}

void UWFReleaseSmartObjectAction::OnPossessionChanged_Implementation()
{
	Super::OnPossessionChanged_Implementation();
	Blackboard = WFUtilities::FindBlackboardComponent(GetControlledActor());
}
