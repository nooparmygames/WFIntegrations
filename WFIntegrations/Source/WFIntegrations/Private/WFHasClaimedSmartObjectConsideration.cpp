// Copyright NoOpArmy 2024


#include "WFHasClaimedSmartObjectConsideration.h"
#include "WFUtilities.h"
#include "WFSmartObjectUtilities.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectBlueprintFunctionLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"

float UWFHasClaimedSmartObjectConsideration::GetValue_Implementation(const AActor* InTargetActor)
{
	// Blackboard setup can finish after possession. Resolve it again before use.
	Blackboard = WFUtilities::FindBlackboardComponent(GetControlledActor());

	bool bHasClaim = false;

	if (IsValid(Blackboard) && ClaimHandleKey != NAME_None)
	{
		const FSmartObjectClaimHandle Claim = USmartObjectBlueprintFunctionLibrary::GetValueAsSOClaimHandle(Blackboard, ClaimHandleKey);
		bHasClaim = Claim.IsValid();

		// A handle can be non-empty but reference a slot that has since been invalidated.
		if (bHasClaim && bValidateWithSubsystem && IsValid(Subsystem))
			bHasClaim = WFSmartObjectUtilities::HasLiveSlot(Subsystem, Claim);
	}

	const bool bResult = bInvert ? !bHasClaim : bHasClaim;
	return bResult ? 1.0f : 0.0f;
}

void UWFHasClaimedSmartObjectConsideration::OnBeginPlay_Implementation()
{
	Super::OnBeginPlay_Implementation();
	Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	Blackboard = WFUtilities::FindBlackboardComponent(GetControlledActor());
}

void UWFHasClaimedSmartObjectConsideration::OnPossessionChanged_Implementation()
{
	Super::OnPossessionChanged_Implementation();
	Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	Blackboard = WFUtilities::FindBlackboardComponent(GetControlledActor());
}
