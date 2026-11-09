// Copyright NoOpArmy 2026
#include "WFInfluenceMoveTestTypes.h"
#include "NavigationData.h"
#include "NavigationSystemTypes.h"

bool AWFInfluenceMoveTestNavData::ProjectPoint(const FVector& Point, FNavLocation& OutLocation,
    const FVector& Extent, FSharedConstNavQueryFilter Filter, const UObject* Querier) const
{
    OutLocation = FNavLocation(Point + ProjectionOffset);
    return bProject;
}

AWFInfluenceMoveTestController::AWFInfluenceMoveTestController(const FObjectInitializer& Initializer)
    : Super(Initializer.SetDefaultSubobjectClass<UWFInfluenceMoveTestPathFollowing>(TEXT("PathFollowingComponent")))
{
}

void AWFInfluenceMoveTestController::FindPathForMoveRequest(const FAIMoveRequest& MoveRequest,
    FPathFindingQuery& Query, FNavPathSharedPtr& OutPath) const
{
    bAllowedPartialPath = MoveRequest.IsUsingPartialPaths();
    if (!bFindPath) return;
    OutPath = MakeShared<FNavigationPath, ESPMode::ThreadSafe>();
    OutPath->GetPathPoints().Add(FNavPathPoint(Query.StartLocation));
    OutPath->GetPathPoints().Add(FNavPathPoint(Query.EndLocation));
    OutPath->MarkReady();
    OutPath->DoneUpdating(ENavPathUpdateType::NavigationChanged);
}

FAIRequestID UWFInfluenceMoveTestPathFollowing::RequestMove(const FAIMoveRequest& RequestData, FNavPathSharedPtr InPath)
{
    const FAIRequestID Result = Super::RequestMove(RequestData, InPath);
    if (bCompleteDuringRequest)
        OnPathFinished(FPathFollowingResult(FPathFollowingResultFlags::Success));
    if (bReplaceDuringRequest)
    {
        bReplaceDuringRequest = false;
        CastChecked<AAIController>(GetOwner())->MoveToLocation(FVector(1800, 1800, 0), 20.0f);
    }
    return Result;
}
