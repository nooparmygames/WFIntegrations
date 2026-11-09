// Copyright NoOpArmy 2026
#pragma once

#include "GameplayBehavior.h"
#include "GameplayBehaviorConfig.h"
#include "WFGameplayBehaviorLifecycleTestTypes.generated.h"

UCLASS(Transient, NotBlueprintable)
class UWFTestStationBehavior : public UGameplayBehavior
{
    GENERATED_BODY()
public:
    int32 Starts = 0;
    int32 Ends = 0;
    int32 Interruptions = 0;
    bool bCompleteSynchronously = false;
    TWeakObjectPtr<AActor> EndedAvatar;
    TFunction<void()> DuringTrigger;
    TFunction<void()> DuringEnd;

    virtual bool Trigger(AActor& Avatar, const UGameplayBehaviorConfig* Config, AActor* SmartObjectOwner) override
    {
        ++Starts;
        TransientAvatar = &Avatar;
        if (DuringTrigger)
            DuringTrigger();
        if (bCompleteSynchronously)
        {
            EndBehavior(Avatar, false);
            return false;
        }
        return true;
    }

    virtual void EndBehavior(AActor& Avatar, bool bInterrupted = false) override
    {
        ++Ends;
        Interruptions += bInterrupted ? 1 : 0;
        EndedAvatar = &Avatar;
        if (DuringEnd)
            DuringEnd();
        Super::EndBehavior(Avatar, bInterrupted);
    }
};

UCLASS(Transient, NotBlueprintable)
class UWFTestStationBehaviorConfig : public UGameplayBehaviorConfig
{
    GENERATED_BODY()
public:
    UPROPERTY()
    TObjectPtr<UWFTestStationBehavior> Behavior;

    virtual UGameplayBehavior* GetBehavior(UWorld& World) const override { return Behavior; }
};
