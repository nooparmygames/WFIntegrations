// Copyright NoOpArmy 2026
#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "WFAction.h"
#include "WFInfluenceMapConsideration.h"
#include "WFMoveToInfluenceMapValueAction.generated.h"

class UPathFollowingComponent;
struct FPathFollowingResult;

/**
 * Selects one influence-map destination and follows a complete navigation path to it.
 * The selection is retained until the move ends; map changes do not restart the move.
 * Influence affects destination selection only. The navigation path can cross other values,
 * so choosing a low-danger destination does not make the route avoid danger.
 */
UCLASS(Blueprintable, BlueprintType)
class WFINTEGRATIONS_API UWFMoveToInfluenceMapValueAction : public UWFAction
{
    GENERATED_BODY()
public:
    virtual void OnActivate_Implementation() override;
    virtual void OnDeactivate_Implementation() override;
    virtual void OnPossessionChanged_Implementation() override;

    /** Registered name of the influence map to search. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|InfluenceMap")
    FName MapName = NAME_None;

    /** Highest/Lowest finds the area's extreme first, then tests SearchCondition.
     * Equal extreme values use the map's closest-to-center tie breaking. FirstMatch can choose any matching cell. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|InfluenceMap")
    EWFInfluenceMapSearchMode SearchMode = EWFInfluenceMapSearchMode::HighestValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|InfluenceMap")
    ESearchCondition SearchCondition = ESearchCondition::GreaterOrEqual;

    /** Required value or threshold, after optional self subtraction and interest multiplication. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|InfluenceMap")
    float SearchValue = 1.0f;

    /** Half-extent of the square search area, in map cells. Zero searches only the center cell. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|InfluenceMap", meta = (ClampMin = "0"))
    int32 SearchRadius = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|InfluenceMap")
    ECenterMode SearchCenterMode = ECenterMode::ControlledActorLocation;

    /** A set Vector key on the pawn's or controller's blackboard when using BlackboardKey mode. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|InfluenceMap", meta = (EditCondition = "SearchCenterMode == ECenterMode::BlackboardKey", EditConditionHides))
    FName SearchCenterKey = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|InfluenceMap")
    bool bUseInterestSearch = false;

    /** Optional multiplier centered on the search center. Requires a valid curve when assigned. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|InfluenceMap", meta = (EditCondition = "bUseInterestSearch"))
    TObjectPtr<UWFInfluenceMapTemplate> InterestTemplate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|InfluenceMap", meta = (EditCondition = "bUseInterestSearch"))
    float InterestMagnitude = 1.0f;

    /** Optional self influence subtracted before interest multiplication, centered on the search center. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|InfluenceMap", meta = (EditCondition = "bUseInterestSearch"))
    TObjectPtr<UWFInfluenceMapTemplate> SelfTemplate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|InfluenceMap", meta = (EditCondition = "bUseInterestSearch"))
    float SelfMagnitude = 1.0f;

    /** Navigation projection half-extent in cm. Projection starts at the selected cell's XY and
     * the controlled actor's Z, and must remain inside the selected cell. No fallback cell is tried. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|Movement", meta = (ClampMin = "0"))
    FVector NavigationProjectionExtent = FVector(100.0, 100.0, 500.0);

    /** Distance in cm accepted by path following. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|Movement", meta = (ClampMin = "0"))
    float AcceptanceRadius = 50.0f;

    /** Include the pawn's collision radius in the arrival test. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline|Movement")
    bool bIncludeAgentRadius = true;

    /** Last projected destination selected during this activation. Read only when bHasDestination is true. */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "WiseFeline|Movement")
    FVector Destination = FVector::ZeroVector;

    /** True once a matching cell projects onto navigation. A later path request can still fail. */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "WiseFeline|Movement")
    bool bHasDestination = false;

    /** True only after successful arrival, including AlreadyAtGoal. Reset at the next activation. */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "WiseFeline|Movement")
    bool bMoveSucceeded = false;

private:
    bool FindDestination(FVector& OutDestination) const;
    void CleanupMove();
    void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result);

    TWeakObjectPtr<UPathFollowingComponent> MovePathFollowing;
    FAIRequestID MoveRequestID = FAIRequestID::InvalidRequest;
    FDelegateHandle MoveFinishedHandle;
    uint32 ActivationGeneration = 0;
};
