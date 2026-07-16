// Copyright NoOpArmy 2024

#pragma once

#include "CoreMinimal.h"
#include "WFAction.h"
#include "WFReleaseSmartObjectAction.generated.h"

class UBlackboardComponent;

/**
 * Frees a Smart Object claim stored in a blackboard key of type "SO Claim Handle" and clears the key, then finishes.
 * Use this to give up a reservation made by a UWFClaimSmartObjectAction (or any other system) when the brain decides
 * it no longer wants the object. Does nothing (still succeeds) if the key holds no valid claim.
 */
UCLASS(Blueprintable, BlueprintType)
class WFINTEGRATIONS_API UWFReleaseSmartObjectAction : public UWFAction
{
	GENERATED_BODY()

public:
	virtual void OnBeginPlay_Implementation() override;
	virtual void OnActivate_Implementation() override;

public:
	/** Blackboard key (type SO Claim Handle) holding the claim to free. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|SmartObject")
	FName ClaimHandleKey = NAME_None;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UBlackboardComponent> Blackboard;
};
