// Copyright NoOpArmy 2026
#pragma once

#include "AbilitySystemInterface.h"
#include "Abilities/GameplayAbility.h"
#include "AttributeSet.h"
#include "GameFramework/Pawn.h"
#include "WFGasLifecycleTestTypes.generated.h"

class UAbilitySystemComponent;

UCLASS(Transient, NotBlueprintable)
class AWFGasLifecycleTestPawn : public APawn, public IAbilitySystemInterface
{
    GENERATED_BODY()
public:
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }

    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> AbilitySystem;
};

UCLASS(Transient)
class UWFGasLifecycleTestAttributes : public UAttributeSet
{
    GENERATED_BODY()
public:
    UPROPERTY()
    FGameplayAttributeData Energy;
};

UCLASS(Transient, NotBlueprintable)
class UWFGasLifecycleTestAbility : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UWFGasLifecycleTestAbility();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;
    void FinishForTest();
};
