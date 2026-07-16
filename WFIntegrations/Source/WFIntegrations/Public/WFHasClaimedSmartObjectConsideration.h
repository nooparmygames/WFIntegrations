// Copyright NoOpArmy 2024

#pragma once

#include "CoreMinimal.h"
#include "WFConsideration.h"
#include "WFHasClaimedSmartObjectConsideration.generated.h"

class USmartObjectSubsystem;
class UBlackboardComponent;

/**
 * Returns 1 when the blackboard SO Claim Handle key holds a valid claim, 0 otherwise. Use it to gate the Smart Object
 * actions in a utility flow: score a UWFUseSmartObjectAction / UWFReleaseSmartObjectAction high only when a claim is
 * held, or set bInvert to score a UWFClaimSmartObjectAction high only when no claim is held yet.
 *
 * Requires the blackboard to contain a key of type SOClaimHandle named ClaimHandleKey.
 */
UCLASS(Blueprintable, BlueprintType)
class WFINTEGRATIONS_API UWFHasClaimedSmartObjectConsideration : public UWFConsideration
{
	GENERATED_BODY()

public:

	virtual void OnBeginPlay_Implementation() override;
	virtual float GetValue_Implementation(const AActor* InTargetActor) override;

public:

	/** Blackboard key (type SO Claim Handle) to check. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	FName ClaimHandleKey = NAME_None;

	/** Also confirm with the Smart Object subsystem that the claim is still live, not just a non-empty handle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	bool bValidateWithSubsystem = true;

	/** Invert the result: return 1 when there is NO valid claim. Handy for gating a Claim action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	bool bInvert = false;

protected:
	UPROPERTY(Transient)
	TObjectPtr<USmartObjectSubsystem> Subsystem;

	UPROPERTY(Transient)
	TObjectPtr<UBlackboardComponent> Blackboard;
};
