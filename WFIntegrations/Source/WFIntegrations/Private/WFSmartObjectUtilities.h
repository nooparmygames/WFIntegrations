// Copyright NoOpArmy 2026

#pragma once

#include "SmartObjectSubsystem.h"
#include "SmartObjectBlueprintFunctionLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"

namespace WFSmartObjectUtilities
{
	// A claim action finishes immediately, so its publication follows the reservation's
	// lifetime independently of the action. Do not replace the slot's invalidation callback.
	inline void ClearPublicationOnRelease(USmartObjectSubsystem* Subsystem, const FSmartObjectClaimHandle& Claim,
		UBlackboardComponent* Blackboard, const FName Key)
	{
		FOnSmartObjectEvent* Events = Subsystem->GetSlotEventDelegate(Claim.SlotHandle);
		if (!Events)
			return;
		const TWeakObjectPtr<USmartObjectSubsystem> WeakSubsystem = Subsystem;
		const TWeakObjectPtr<UBlackboardComponent> WeakBlackboard = Blackboard;
		const TSharedRef<FDelegateHandle> Listener = MakeShared<FDelegateHandle>();
		*Listener = Events->AddWeakLambda(Blackboard, [WeakSubsystem, WeakBlackboard, Claim, Key, Listener](const FSmartObjectEventData& Event)
		{
			if (Event.SlotHandle != Claim.SlotHandle || Event.Reason != ESmartObjectChangeReason::OnReleased)
				return;
			if (WeakSubsystem.IsValid())
				if (FOnSmartObjectEvent* CurrentEvents = WeakSubsystem->GetSlotEventDelegate(Claim.SlotHandle))
					CurrentEvents->Remove(*Listener);
			if (WeakBlackboard.IsValid()
				&& USmartObjectBlueprintFunctionLibrary::GetValueAsSOClaimHandle(WeakBlackboard.Get(), Key) == Claim)
				USmartObjectBlueprintFunctionLibrary::SetValueAsSOClaimHandle(WeakBlackboard.Get(), Key, FSmartObjectClaimHandle::InvalidHandle);
		});
	}

	inline bool HasLiveSlot(const USmartObjectSubsystem* Subsystem, const FSmartObjectClaimHandle& Claim)
	{
		if (!IsValid(Subsystem) || !Claim.IsValid() || !Subsystem->IsClaimedSmartObjectValid(Claim))
			return false;
		// IsClaimedSmartObjectValid only checks the object in UE 5.8. A released slot
		// can still have a non-empty handle in a blackboard owned by another system.
		const ESmartObjectSlotState State = Subsystem->GetSlotState(Claim.SlotHandle);
		return State == ESmartObjectSlotState::Claimed || State == ESmartObjectSlotState::Occupied;
	}
}
