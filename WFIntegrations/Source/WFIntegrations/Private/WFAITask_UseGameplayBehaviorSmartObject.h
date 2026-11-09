// Copyright NoOpArmy 2026
#pragma once

#include "AI/AITask_UseGameplayBehaviorSmartObject.h"
#include "WFAITask_UseGameplayBehaviorSmartObject.generated.h"

class APawn;

// The engine task frees its claim on destruction, but ExternalCancel does not abort
// its behavior. Keep that cleanup tied to the pawn which started the interaction.
UCLASS(Transient, NotBlueprintable)
class UWFAITask_UseGameplayBehaviorSmartObject : public UAITask_UseGameplayBehaviorSmartObject
{
    GENERATED_BODY()
public:
    static UWFAITask_UseGameplayBehaviorSmartObject* Create(AAIController& Controller,
        const FSmartObjectClaimHandle& Claim, bool bMoveToSlot, bool bLockAILogic);

protected:
    virtual void Activate() override;
    virtual void OnGameplayTaskDeactivated(UGameplayTask& Child) override;
    virtual void OnDestroy(bool bInOwnerFinished) override;

private:
    void AbortOwnedBehavior();
    void FinishDeferredDestruction();
    void OnClaimInvalidated(const FSmartObjectClaimHandle& Claim, ESmartObjectSlotState State);
    TWeakObjectPtr<APawn> OriginalPawn;
    bool bDestroying = false;
    bool bInsideEngineCallback = false;
    bool bDestroyRequested = false;
    bool bDeferredOwnerFinished = false;
};
