// Copyright NoOpArmy 2026
#pragma once
#include "AIController.h"
#include "NavMesh/RecastNavMesh.h"
#include "Navigation/PathFollowingComponent.h"
#include "WFInfluenceMoveTestTypes.generated.h"

UCLASS(Transient, NotBlueprintable)
class AWFInfluenceMoveTestNavData : public ARecastNavMesh
{
    GENERATED_BODY()
public:
    bool bProject = true;
    FVector ProjectionOffset = FVector::ZeroVector;
    virtual bool ProjectPoint(const FVector& Point, FNavLocation& OutLocation, const FVector& Extent,
        FSharedConstNavQueryFilter Filter = nullptr, const UObject* Querier = nullptr) const override;
};

UCLASS(Transient, NotBlueprintable)
class UWFInfluenceMoveTestPathFollowing : public UPathFollowingComponent
{
    GENERATED_BODY()
public:
    bool bCompleteDuringRequest = false;
    bool bReplaceDuringRequest = false;
    virtual FAIRequestID RequestMove(const FAIMoveRequest& RequestData, FNavPathSharedPtr InPath) override;
};

UCLASS(Transient, NotBlueprintable)
class AWFInfluenceMoveTestController : public AAIController
{
    GENERATED_BODY()
public:
    AWFInfluenceMoveTestController(const FObjectInitializer& Initializer);
    bool bFindPath = true;
    mutable bool bAllowedPartialPath = true;
    virtual void FindPathForMoveRequest(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query,
        FNavPathSharedPtr& OutPath) const override;
};
