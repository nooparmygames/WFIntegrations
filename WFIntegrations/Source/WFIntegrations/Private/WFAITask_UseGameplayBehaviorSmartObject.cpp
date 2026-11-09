// Copyright NoOpArmy 2026
#include "WFAITask_UseGameplayBehaviorSmartObject.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "GameplayBehavior.h"
#include "SmartObjectSubsystem.h"

UWFAITask_UseGameplayBehaviorSmartObject* UWFAITask_UseGameplayBehaviorSmartObject::Create(
    AAIController& Controller, const FSmartObjectClaimHandle& Claim, bool bMoveToSlot, bool bLockAILogic)
{
    auto* NewTask = NewAITask<UWFAITask_UseGameplayBehaviorSmartObject>(Controller, EAITaskPriority::High);
    NewTask->OriginalPawn = Controller.GetPawn();
    NewTask->SetClaimHandle(Claim);
    NewTask->SetShouldReachSlotLocation(bMoveToSlot);
    if (bLockAILogic)
        NewTask->RequestAILogicLocking();
    return NewTask;
}

void UWFAITask_UseGameplayBehaviorSmartObject::Activate()
{
    // A queued task must not start on a replacement pawn.
    if (!OriginalPawn.IsValid() || !IsValid(OwnerController) || OwnerController->GetPawn() != OriginalPawn.Get())
    {
        EndTask();
        return;
    }
    {
        TGuardValue<bool> Guard(bInsideEngineCallback, true);
        Super::Activate();
    }
    FinishDeferredDestruction();
    if (!IsFinished() && ClaimedHandle.IsValid())
    {
        if (USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld()))
            Subsystem->RegisterSlotInvalidationCallback(ClaimedHandle,
                FOnSlotInvalidated::CreateUObject(this, &UWFAITask_UseGameplayBehaviorSmartObject::OnClaimInvalidated));
    }
}

void UWFAITask_UseGameplayBehaviorSmartObject::OnClaimInvalidated(const FSmartObjectClaimHandle& Claim, ESmartObjectSlotState State)
{
    // The subsystem is already releasing this claim. The engine task's normal
    // OnDestroy would free it again before that release returns.
    if (USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld()))
        Subsystem->UnregisterSlotInvalidationCallback(Claim);
    ClaimedHandle.Invalidate();
    ExternalCancel();
}

void UWFAITask_UseGameplayBehaviorSmartObject::OnGameplayTaskDeactivated(UGameplayTask& Child)
{
    if (IsFinished())
    {
        // A late child callback must not restart interaction on a finished task.
        MoveToTask = nullptr;
        Super::OnGameplayTaskDeactivated(Child);
        return;
    }
    if (!IsFinished() && (!OriginalPawn.IsValid() || !IsValid(OwnerController)
        || OwnerController->GetPawn() != OriginalPawn.Get()))
    {
        // Do not let the engine's arrival handler start an interaction on a new pawn.
        MoveToTask = nullptr;
        Super::OnGameplayTaskDeactivated(Child);
        EndTask();
        return;
    }
    {
        TGuardValue<bool> Guard(bInsideEngineCallback, true);
        Super::OnGameplayTaskDeactivated(Child);
    }
    FinishDeferredDestruction();
}

void UWFAITask_UseGameplayBehaviorSmartObject::FinishDeferredDestruction()
{
    if (bDestroyRequested && !bInsideEngineCallback)
        OnDestroy(bDeferredOwnerFinished);
}

void UWFAITask_UseGameplayBehaviorSmartObject::AbortOwnedBehavior()
{
    if (!IsValid(GameplayBehavior) || !OnBehaviorFinishedNotifyHandle.IsValid())
        return;

    // Engine Abort removes this binding without resetting its handle. Only abort
    // when the binding was still ours, otherwise slot invalidation would abort twice.
    const bool bWasBound = GameplayBehavior->GetOnBehaviorFinishedDelegate().Remove(OnBehaviorFinishedNotifyHandle);
    OnBehaviorFinishedNotifyHandle.Reset();
    if (bWasBound && !bBehaviorFinished && OriginalPawn.IsValid())
        GameplayBehavior->AbortBehavior(*OriginalPawn.Get());
}

void UWFAITask_UseGameplayBehaviorSmartObject::OnDestroy(bool bInOwnerFinished)
{
    // AbortBehavior invokes user code, which may try to cancel this task again.
    if (bDestroying)
        return;
    if (bInsideEngineCallback)
    {
        // Trigger can cancel the action before the engine binds its completion
        // callback. Keep the task alive until that call returns, then abort the
        // behavior through the completed binding and let the engine free its claim.
        bDestroyRequested = true;
        bDeferredOwnerFinished |= bInOwnerFinished;
        return;
    }
    bDestroying = true;
    AbortOwnedBehavior();
    // The engine remains the sole owner of releasing the claim and ending child tasks.
    Super::OnDestroy(bInOwnerFinished);
}
