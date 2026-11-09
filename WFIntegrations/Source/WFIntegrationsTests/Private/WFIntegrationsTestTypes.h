// Copyright NoOpArmy 2026
#pragma once

#include "AIController.h"
#include "NavMesh/RecastNavMesh.h"
#include "SmartObjectDefinition.h"
#include "WFInfluenceMap.h"
#include "WFIntegrationsTestTypes.generated.h"

UCLASS(Transient, NotBlueprintable)
class UWFIntegrationTestBehaviorDefinition : public USmartObjectBehaviorDefinition
{
    GENERATED_BODY()
};

// Supply deterministic paths while retaining the engine controller and path-following lifecycle.
UCLASS(Transient, NotBlueprintable)
class AWFIntegrationTestNavData : public ARecastNavMesh
{
    GENERATED_BODY()
public:
    virtual bool ProjectPoint(const FVector& Point, FNavLocation& OutLocation, const FVector& Extent,
        FSharedConstNavQueryFilter Filter = nullptr, const UObject* Querier = nullptr) const override;
};

UCLASS(Transient, NotBlueprintable)
class AWFIntegrationTestController : public AAIController
{
    GENERATED_BODY()
public:
    virtual void FindPathForMoveRequest(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query,
        FNavPathSharedPtr& OutPath) const override;
};

UCLASS(Transient, NotBlueprintable)
class AWFIntegrationTestInfluenceMap : public AWFInfluenceMap
{
    GENERATED_BODY()
public:
    // This isolated test world does not initialize every actor for play.
    void InitializeForTest() { PostInitializeComponents(); }
};
