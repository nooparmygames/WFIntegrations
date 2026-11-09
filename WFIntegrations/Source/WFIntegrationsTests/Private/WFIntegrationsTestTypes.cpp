// Copyright NoOpArmy 2026
#include "WFIntegrationsTestTypes.h"
#include "NavigationData.h"
#include "NavigationSystemTypes.h"

bool AWFIntegrationTestNavData::ProjectPoint(const FVector& Point, FNavLocation& OutLocation,
    const FVector& Extent, FSharedConstNavQueryFilter Filter, const UObject* Querier) const
{
    OutLocation = FNavLocation(Point);
    return true;
}

void AWFIntegrationTestController::FindPathForMoveRequest(const FAIMoveRequest& MoveRequest,
    FPathFindingQuery& Query, FNavPathSharedPtr& OutPath) const
{
    OutPath = MakeShared<FNavigationPath, ESPMode::ThreadSafe>();
    OutPath->GetPathPoints().Add(FNavPathPoint(Query.StartLocation));
    OutPath->GetPathPoints().Add(FNavPathPoint(Query.EndLocation));
    OutPath->MarkReady();
    OutPath->DoneUpdating(ENavPathUpdateType::NavigationChanged);
}
