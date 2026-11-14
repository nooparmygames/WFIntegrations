// Copyright NoOpArmy 2026
#include "WFMoveToInfluenceMapValueAction.h"
#include "WFInfluenceMapQueryLibrary.h"
#include "WFInfluenceMapsSubsystem.h"
#include "WFUtilities.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationData.h"
#include "NavigationSystem.h"

namespace
{
    bool IsFiniteLocation(const FVector& Location)
    {
        return !Location.ContainsNaN() && FAISystem::IsValidLocation(Location);
    }

    bool MeetsMoveSearchCondition(float Value, ESearchCondition Condition, float Threshold)
    {
        switch (Condition)
        {
        case ESearchCondition::Equal: return Value == Threshold;
        case ESearchCondition::NotEqual: return Value != Threshold;
        case ESearchCondition::Greater: return Value > Threshold;
        case ESearchCondition::GreaterOrEqual: return Value >= Threshold;
        case ESearchCondition::Less: return Value < Threshold;
        case ESearchCondition::LessOrEqual: return Value <= Threshold;
        }
        return false;
    }

    bool IsUsableTemplate(UWFInfluenceMapTemplate* Template, const FIntVector& Center)
    {
        if (!Template) return true;
        if (!IsValid(Template) || !IsValid(Template->Curve) || Template->Radius < 0)
            return false;
        const int64 Side = int64(Template->Radius) * 2 + 1;
        if (Side > 46340 || Side * Side > MAX_int32
            || int64(Center.X) - Template->Radius < MIN_int32
            || int64(Center.Y) - Template->Radius < MIN_int32)
            return false;
        return Template->GetMap().Num() == Side * Side;
    }

    EWFInfluenceQueryThreshold ToStableThreshold(ESearchCondition Condition)
    {
        switch (Condition)
        {
        case ESearchCondition::Equal: return EWFInfluenceQueryThreshold::Equal;
        case ESearchCondition::NotEqual: return EWFInfluenceQueryThreshold::NotEqual;
        case ESearchCondition::Greater: return EWFInfluenceQueryThreshold::Greater;
        case ESearchCondition::GreaterOrEqual: return EWFInfluenceQueryThreshold::GreaterOrEqual;
        case ESearchCondition::Less: return EWFInfluenceQueryThreshold::Less;
        case ESearchCondition::LessOrEqual: return EWFInfluenceQueryThreshold::LessOrEqual;
        }
        return static_cast<EWFInfluenceQueryThreshold>(255);
    }

    FAIMoveRequest MakeInfluenceMoveRequest(const FVector& Destination, float AcceptanceRadius, bool bIncludeAgentRadius)
    {
        FAIMoveRequest Request(Destination);
        Request.SetUsePathfinding(true);
        Request.SetAllowPartialPath(false);
        Request.SetProjectGoalLocation(false);
        Request.SetAcceptanceRadius(AcceptanceRadius);
        Request.SetReachTestIncludesAgentRadius(bIncludeAgentRadius);
        Request.SetReachTestIncludesGoalRadius(false);
        return Request;
    }

    bool SameInfluenceGrid(const FWFInfluenceMapLayout& A, const FWFInfluenceMapLayout& B)
    {
        return A.bInitialized && B.bInitialized && A.Width == B.Width && A.Height == B.Height
            && A.CellSize == B.CellSize && A.AnchorXY == B.AnchorXY && A.AllocatedCellCount == B.AllocatedCellCount;
    }
}

bool UWFMoveToInfluenceMapValueAction::ResolveSearch(AActor*& Self, APawn*& Pawn, AWFInfluenceMap*& Map,
    FIntVector& CenterCell, FVector& FirstCellCenter) const
{
    Self = GetControlledActor();
    AAIController* Controller = GetAIController();
    Pawn = IsValid(Controller) ? Controller->GetPawn() : nullptr;
    if (!IsValid(Self) || !IsValid(Pawn) || Pawn != Self || MapName.IsNone()
        || !FMath::IsFinite(SearchValue) || SearchRadius < 0 || SearchRadius > MAX_int32 / 4
        || !FMath::IsFinite(AcceptanceRadius) || AcceptanceRadius < 0
        || NavigationProjectionExtent.ContainsNaN() || NavigationProjectionExtent.GetMin() < 0)
        return false;

    UWFInfluenceMapsSubsystem* Subsystem = UWFInfluenceMapsSubsystem::Get(Self->GetWorld());
    Map = IsValid(Subsystem) ? Subsystem->GetMap(MapName) : nullptr;
    if (!IsValid(Map) || Map->Width <= 0 || Map->Height <= 0
        || Map->Width > MAX_int32 / 2 || Map->Height > MAX_int32 / 2
        || !FMath::IsFinite(Map->CellSize) || Map->CellSize <= 0
        || int64(Map->Width) * Map->Height != Map->GetMap().Num())
        return false;

    FVector Center;
    if (SearchCenterMode == ECenterMode::ControlledActorLocation)
        Center = Self->GetActorLocation();
    else if (SearchCenterMode == ECenterMode::BlackboardKey)
    {
        UBlackboardComponent* Blackboard = WFUtilities::FindBlackboardComponent(Self);
        if (!IsValid(Blackboard) || SearchCenterKey.IsNone() || !Blackboard->IsVectorValueSet(SearchCenterKey))
            return false;
        Center = Blackboard->GetValueAsVector(SearchCenterKey);
    }
    else return false;

    FirstCellCenter = Map->MapToWorldPosition(0, 0);
    if (!IsFiniteLocation(Center) || !IsFiniteLocation(Self->GetActorLocation()) || !IsFiniteLocation(FirstCellCenter))
        return false;
    // Validate in double precision before the map converts world coordinates to int32.
    const double CellX = (Center.X - FirstCellCenter.X) / Map->CellSize + 0.5;
    const double CellY = (Center.Y - FirstCellCenter.Y) / Map->CellSize + 0.5;
    if (!FMath::IsFinite(CellX) || !FMath::IsFinite(CellY)
        || CellX < MIN_int32 / 2 || CellY < MIN_int32 / 2
        || CellX > MAX_int32 / 2 || CellY > MAX_int32 / 2)
        return false;
    CenterCell = Map->WorldToMapPosition(Center);
    // First-match and interest searches expect a non-empty rectangle.
    if (int64(CenterCell.X) + SearchRadius < 0 || int64(CenterCell.Y) + SearchRadius < 0
        || int64(CenterCell.X) - SearchRadius >= Map->Width || int64(CenterCell.Y) - SearchRadius >= Map->Height)
        return false;
    return true;
}

bool UWFMoveToInfluenceMapValueAction::FindDestination(FVector& OutDestination) const
{
    AActor* Self = nullptr;
    APawn* Pawn = nullptr;
    AWFInfluenceMap* Map = nullptr;
    FIntVector CenterCell;
    FVector FirstCellCenter;
    if (!ResolveSearch(Self, Pawn, Map, CenterCell, FirstCellCenter))
        return false;
    if (bUseInterestSearch && (!FMath::IsFinite(InterestMagnitude) || !FMath::IsFinite(SelfMagnitude)
        || !IsUsableTemplate(InterestTemplate, CenterCell) || !IsUsableTemplate(SelfTemplate, CenterCell)))
        return false;

    FIntVector ResultCell = FIntVector::ZeroValue;
    float Value = 0.0f;
    if (SearchMode == EWFInfluenceMapSearchMode::FirstMatch)
    {
        const bool bFound = bUseInterestSearch
            ? Map->SearchWithInterest(SearchValue, SearchCondition, CenterCell, SearchRadius,
                InterestTemplate, InterestMagnitude, SelfTemplate, SelfMagnitude, ResultCell)
            : Map->Search(SearchValue, SearchCondition, CenterCell, SearchRadius, ResultCell);
        if (!bFound) return false;
        Value = Map->GetCellValue(ResultCell.X, ResultCell.Y);
        if (bUseInterestSearch)
        {
            if (SelfTemplate)
                Value -= SelfTemplate->GetClampedTemplateValue(ResultCell.X - CenterCell.X + SelfTemplate->Radius,
                    ResultCell.Y - CenterCell.Y + SelfTemplate->Radius, SelfMagnitude);
            if (InterestTemplate)
                Value *= InterestTemplate->GetClampedTemplateValue(ResultCell.X - CenterCell.X + InterestTemplate->Radius,
                    ResultCell.Y - CenterCell.Y + InterestTemplate->Radius, InterestMagnitude);
        }
    }
    else if (SearchMode == EWFInfluenceMapSearchMode::HighestValue || SearchMode == EWFInfluenceMapSearchMode::LowestValue)
    {
        const bool bHighest = SearchMode == EWFInfluenceMapSearchMode::HighestValue;
        if (bUseInterestSearch)
            Value = bHighest
                ? Map->SearchWithInterestForHighestValueClosestToPoint(CenterCell, SearchRadius, CenterCell,
                    InterestTemplate, InterestMagnitude, SelfTemplate, SelfMagnitude, ResultCell)
                : Map->SearchWithInterestForLowestValueClosestToPoint(CenterCell, SearchRadius, CenterCell,
                    InterestTemplate, InterestMagnitude, SelfTemplate, SelfMagnitude, ResultCell);
        else
            Value = bHighest
                ? Map->SearchForHighestValueClosestToPoint(CenterCell, SearchRadius, CenterCell, ResultCell)
                : Map->SearchForLowestValueClosestToPoint(CenterCell, SearchRadius, CenterCell, ResultCell);
    }
    else return false;

    if (!FMath::IsFinite(Value) || Value <= TNumericLimits<float>::Lowest() || Value >= TNumericLimits<float>::Max()
        || !MeetsMoveSearchCondition(Value, SearchCondition, SearchValue)
        || ResultCell.X < 0 || ResultCell.X >= Map->Width || ResultCell.Y < 0 || ResultCell.Y >= Map->Height)
        return false;

    return ProjectDestinationCell(*Map, ResultCell, FirstCellCenter, *Self, *Pawn, OutDestination);
}

bool UWFMoveToInfluenceMapValueAction::ProjectDestinationCell(AWFInfluenceMap& Map, const FIntVector& Cell,
    const FVector& FirstCellCenter, AActor& Self, APawn& Pawn, FVector& OutDestination) const
{
    FVector Selected = Map.MapToWorldPosition(Cell);
    Selected.Z = Self.GetActorLocation().Z;
    if (!IsFiniteLocation(Selected)) return false;
    UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Self.GetWorld());
    const ANavigationData* NavData = Navigation
        ? Navigation->GetNavDataForProps(Pawn.GetNavAgentPropertiesRef(), Pawn.GetNavAgentLocation()) : nullptr;
    if (!IsValid(NavData)) return false;
    FNavLocation Projected;
    if (!Navigation->ProjectPointToNavigation(Selected, Projected, NavigationProjectionExtent, NavData)
        || !IsFiniteLocation(Projected.Location) || !IsValid(&Map) || Map.IsActorBeingDestroyed())
        return false;
    // Navigation projection must not silently choose a different influence value.
    const double ProjectedX = (Projected.Location.X - FirstCellCenter.X) / Map.CellSize + 0.5;
    const double ProjectedY = (Projected.Location.Y - FirstCellCenter.Y) / Map.CellSize + 0.5;
    if (!FMath::IsFinite(ProjectedX) || !FMath::IsFinite(ProjectedY)
        || ProjectedX < MIN_int32 / 2 || ProjectedY < MIN_int32 / 2
        || ProjectedX > MAX_int32 / 2 || ProjectedY > MAX_int32 / 2
        || Map.WorldToMapPosition(Projected.Location) != Cell)
        return false;
    OutDestination = Projected.Location;
    return true;
}

void UWFMoveToInfluenceMapValueAction::OnActivate_Implementation()
{
    const uint32 Generation = ++ActivationGeneration;
    CleanupMove();
    if (Generation != ActivationGeneration) return;
    Super::OnActivate_Implementation();
    bMoveSucceeded = false;
    bHasDestination = false;
    Destination = FVector::ZeroVector;
    CommittedMap.Reset();
    bStableActivation = bReplanWhileMoving;
    if (bStableActivation)
    {
        if (!FMath::IsFinite(ReplanInterval) || ReplanInterval < 0.01f
            || (SearchMode != EWFInfluenceMapSearchMode::HighestValue && SearchMode != EWFInfluenceMapSearchMode::LowestValue))
        {
            Finished();
            return;
        }
        bEnabledTickForReplan = bEnabledTickForReplan || !bShouldTick;
        bShouldTick = true;
        TimeToReplan = ReplanInterval;
        EvaluateStableMove(false);
        return;
    }
    if (!FindDestination(Destination))
    {
        Finished();
        return;
    }
    bHasDestination = true;
    AAIController* Controller = GetAIController();
    UPathFollowingComponent* PathFollowing = IsValid(Controller) ? Controller->GetPathFollowingComponent() : nullptr;
    if (!IsValid(PathFollowing))
    {
        Finished();
        return;
    }
    const FAIMoveRequest Request = MakeInfluenceMoveRequest(Destination, AcceptanceRadius, bIncludeAgentRadius);
    AActor* OriginalActor = GetControlledActor();
    const FPathFollowingRequestResult Result = Controller->MoveTo(Request);
    // Replacing an older move invokes user callbacks before MoveTo returns. They can
    // cancel this action or change its owner; never adopt a request from that activation.
    if (Generation != ActivationGeneration || !IsActive() || GetAIController() != Controller
        || GetControlledActor() != OriginalActor || Controller->GetPawn() != OriginalActor)
    {
        if (Result.Code == EPathFollowingRequestResult::RequestSuccessful && Result.MoveId.IsValid()
            && PathFollowing->GetCurrentRequestId() == Result.MoveId)
            PathFollowing->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, Result.MoveId);
        return;
    }
    if (Result.Code == EPathFollowingRequestResult::RequestSuccessful)
    {
        // A custom path-following callback can complete or replace the request during dispatch.
        if (PathFollowing->GetCurrentRequestId() != Result.MoveId || PathFollowing->GetStatus() == EPathFollowingStatus::Idle)
        {
            Finished();
            return;
        }
        MovePathFollowing = PathFollowing;
        MoveRequestID = Result.MoveId;
        MoveFinishedHandle = PathFollowing->OnRequestFinished.AddUObject(this, &UWFMoveToInfluenceMapValueAction::OnMoveCompleted);
    }
    else
    {
        bMoveSucceeded = Result.Code == EPathFollowingRequestResult::AlreadyAtGoal;
        Finished();
    }
}

void UWFMoveToInfluenceMapValueAction::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
    if (RequestID != MoveRequestID) return;
    bMoveSucceeded = Result.IsSuccess();
    CleanupMove();
    Finished();
}

void UWFMoveToInfluenceMapValueAction::TickAction_Implementation(float DeltaTime)
{
    Super::TickAction_Implementation(DeltaTime);
    if (!bStableActivation || !IsActive())
        return;
    TimeToReplan -= DeltaTime;
    if (TimeToReplan <= 0.0f)
    {
        if (!FMath::IsFinite(ReplanInterval) || ReplanInterval < 0.01f)
        {
            Finished();
            return;
        }
        TimeToReplan = ReplanInterval;
        EvaluateStableMove(true);
    }
}

void UWFMoveToInfluenceMapValueAction::EvaluateStableMove(bool bReplacing)
{
    const uint32 Generation = ActivationGeneration;
    AActor* OriginalActor = GetControlledActor();
    AAIController* Controller = GetAIController();
    UPathFollowingComponent* PathFollowing = IsValid(Controller) ? Controller->GetPathFollowingComponent() : nullptr;
    const FAIRequestID OriginalRequest = MoveRequestID;
    const auto StillActive = [&]
    {
        return Generation == ActivationGeneration && IsActive() && IsValid(OriginalActor) && IsValid(Controller)
            && IsValid(PathFollowing) && GetControlledActor() == OriginalActor && GetAIController() == Controller
            && Controller->GetPawn() == OriginalActor && Controller->GetPathFollowingComponent() == PathFollowing;
    };
    const auto StillOwnsRequest = [&]
    {
        return StillActive() && MovePathFollowing.Get() == PathFollowing && MoveRequestID == OriginalRequest
            && OriginalRequest.IsValid() && PathFollowing->GetCurrentRequestId() == OriginalRequest
            && PathFollowing->GetStatus() != EPathFollowingStatus::Idle;
    };
    const auto EndIfCurrentActivation = [&]
    {
        if (Generation == ActivationGeneration && IsActive())
            Finished();
    };
    if (!StillActive() || (bReplacing ? !StillOwnsRequest() : PathFollowing->GetStatus() != EPathFollowingStatus::Idle))
    {
        Finished();
        return;
    }
    if (SearchMode != EWFInfluenceMapSearchMode::HighestValue && SearchMode != EWFInfluenceMapSearchMode::LowestValue)
    {
        Finished();
        return;
    }

    AActor* Self = nullptr;
    APawn* Pawn = nullptr;
    AWFInfluenceMap* Map = nullptr;
    FIntVector Center;
    FVector FirstCellCenter;
    if (!ResolveSearch(Self, Pawn, Map, Center, FirstCellCenter))
    {
        Finished();
        return;
    }
    FWFInfluenceQueryRegion Region;
    Region.CenterCell = FIntPoint(Center.X, Center.Y);
    Region.Radius = SearchRadius;
    Region.bHasCurrentGoal = bReplacing && CommittedMap.Get() == Map && SameInfluenceGrid(CommittedLayout, Map->GetGridLayout());
    Region.CurrentGoal = CommittedCell;
    Region.bRecordForInspector = bRecordStableQueries;
    FWFInfluenceQuerySettings Settings;
    Settings.Mode = SearchMode == EWFInfluenceMapSearchMode::HighestValue ? EWFInfluenceQueryMode::Highest : EWFInfluenceQueryMode::Lowest;
    Settings.Threshold = ToStableThreshold(SearchCondition);
    Settings.ThresholdValue = SearchValue;
    Settings.MinimumImprovement = MinimumReplanImprovement;
    Settings.MinimumDisplacement = MinimumReplanDisplacement;
    FWFInfluenceQueryTemplates Templates;
    if (bUseInterestSearch)
    {
        Templates.Interest = InterestTemplate;
        Templates.InterestMagnitude = InterestMagnitude;
        Templates.SelfToRemove = SelfTemplate;
        Templates.SelfMagnitude = SelfMagnitude;
    }
    const FWFInfluenceQueryResult Query = UWFInfluenceMapQueryLibrary::QueryStableDestination(Map, Region, Settings, Templates);
    if (!StillActive() || (bReplacing && !StillOwnsRequest()))
    {
        EndIfCurrentActivation();
        return;
    }
    if (!Query.bSuccess || !IsValid(Map) || Map->IsActorBeingDestroyed() || !SameInfluenceGrid(Query.Layout, Map->GetGridLayout()))
    {
        Finished();
        return;
    }
    if (Query.Status == EWFInfluenceQueryStatus::RetainedCurrent)
        return;

    const FIntVector Cell(Query.Chosen.Cell.X, Query.Chosen.Cell.Y, 0);
    FVector Projected;
    const bool bProjected = ProjectDestinationCell(*Map, Cell, FirstCellCenter, *Self, *Pawn, Projected);
    if (!StillActive() || (bReplacing && !StillOwnsRequest()))
    {
        EndIfCurrentActivation();
        return;
    }
    if (!IsValid(Map) || Map->IsActorBeingDestroyed() || !SameInfluenceGrid(Query.Layout, Map->GetGridLayout()))
    {
        Finished();
        return;
    }
    const auto RefuseCandidate = [&]
    {
        if (!bReplacing || !Query.Current.bValid)
            Finished();
    };
    if (!bProjected)
    {
        RefuseCandidate();
        return;
    }

    const auto CommitDestination = [&]
    {
        Destination = Projected;
        bHasDestination = true;
        CommittedMap = Map;
        CommittedLayout = Query.Layout;
        CommittedCell = Query.Chosen.Cell;
    };
    if (bReplacing && Destination.Equals(Projected, UE_KINDA_SMALL_NUMBER))
    {
        CommitDestination();
        return;
    }
    const FAIMoveRequest Request = MakeInfluenceMoveRequest(Projected, AcceptanceRadius, bIncludeAgentRadius);
    const bool bReached = PathFollowing->HasReached(Request);
    if (!StillActive() || (bReplacing && !StillOwnsRequest()))
    {
        EndIfCurrentActivation();
        return;
    }
    if (bReached)
    {
        CommitDestination();
        bMoveSucceeded = true;
        Finished();
        return;
    }
    FPathFindingQuery PathQuery;
    FNavPathSharedPtr NewPath;
    const bool bCanFindPath = Controller->BuildPathfindingQuery(Request, PathQuery);
    // BuildPathfindingQuery invokes the path-following component's virtual query hook.
    if (!StillActive() || (bReplacing && !StillOwnsRequest()))
    {
        EndIfCurrentActivation();
        return;
    }
    if (bCanFindPath)
        Controller->FindPathForMoveRequest(Request, PathQuery, NewPath);
    if (!StillActive() || (bReplacing && !StillOwnsRequest()))
    {
        EndIfCurrentActivation();
        return;
    }
    if (!NewPath.IsValid() || !NewPath->IsValid() || NewPath->IsPartial())
    {
        RefuseCandidate();
        return;
    }
    if (!IsValid(Map) || Map->IsActorBeingDestroyed() || !SameInfluenceGrid(Query.Layout, Map->GetGridLayout()))
    {
        Finished();
        return;
    }
    if (bReplacing)
    {
        // Aborting explicitly lets us observe foreign movement started by an old request's
        // completion callback. RequestMove's internal replacement would overwrite it.
        CleanupMove(true);
    }
    if (!StillActive())
    {
        EndIfCurrentActivation();
        return;
    }
    if (PathFollowing->GetStatus() != EPathFollowingStatus::Idle
        || !IsValid(Map) || Map->IsActorBeingDestroyed() || !SameInfluenceGrid(Query.Layout, Map->GetGridLayout()))
    {
        Finished();
        return;
    }

    struct FDispatchCompletion
    {
        FAIRequestID Id = FAIRequestID::InvalidRequest;
        bool bSucceeded = false;
    };
    const TSharedRef<FDispatchCompletion> Completion = MakeShared<FDispatchCompletion>();
    const FDelegateHandle DuringDispatch = PathFollowing->OnRequestFinished.AddLambda(
        [Completion](FAIRequestID ID, const FPathFollowingResult& Result)
        {
            Completion->Id = ID;
            Completion->bSucceeded = Result.IsSuccess();
        });
    const FAIRequestID NewRequest = Controller->RequestMove(Request, NewPath);
    if (IsValid(PathFollowing))
        PathFollowing->OnRequestFinished.Remove(DuringDispatch);
    if (!StillActive())
    {
        if (IsValid(PathFollowing) && NewRequest.IsValid() && PathFollowing->GetCurrentRequestId() == NewRequest
            && PathFollowing->GetStatus() != EPathFollowingStatus::Idle)
        {
            PathFollowing->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, NewRequest);
        }
        EndIfCurrentActivation();
        return;
    }
    if (NewRequest.IsValid() && PathFollowing->GetCurrentRequestId() == NewRequest
        && PathFollowing->GetStatus() != EPathFollowingStatus::Idle)
    {
        CommitDestination();
        Controller->bAllowStrafe = Request.CanStrafe();
        MovePathFollowing = PathFollowing;
        MoveRequestID = NewRequest;
        MoveFinishedHandle = PathFollowing->OnRequestFinished.AddUObject(this, &UWFMoveToInfluenceMapValueAction::OnMoveCompleted);
    }
    else
    {
        if (NewRequest.IsValid() && NewRequest == Completion->Id)
        {
            CommitDestination();
            bMoveSucceeded = Completion->bSucceeded;
        }
        Finished();
    }
}

void UWFMoveToInfluenceMapValueAction::CleanupMove(bool bKeepVelocity)
{
    UPathFollowingComponent* PathFollowing = MovePathFollowing.Get();
    const FAIRequestID RequestID = MoveRequestID;
    if (IsValid(PathFollowing))
        PathFollowing->OnRequestFinished.Remove(MoveFinishedHandle);
    MoveFinishedHandle.Reset();
    MovePathFollowing.Reset();
    MoveRequestID = FAIRequestID::InvalidRequest;
    if (IsValid(PathFollowing) && RequestID.IsValid() && PathFollowing->GetCurrentRequestId() == RequestID
        && PathFollowing->GetStatus() != EPathFollowingStatus::Idle)
        PathFollowing->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, RequestID,
            bKeepVelocity ? EPathFollowingVelocityMode::Keep : EPathFollowingVelocityMode::Reset);
}

void UWFMoveToInfluenceMapValueAction::OnDeactivate_Implementation()
{
    ++ActivationGeneration;
    bStableActivation = false;
    if (bEnabledTickForReplan)
    {
        bShouldTick = false;
        bEnabledTickForReplan = false;
    }
    CommittedMap.Reset();
    Super::OnDeactivate_Implementation();
    CleanupMove();
}

void UWFMoveToInfluenceMapValueAction::OnPossessionChanged_Implementation()
{
    const uint32 Generation = ++ActivationGeneration;
    Super::OnPossessionChanged_Implementation();
    if (Generation != ActivationGeneration)
        return;
    if (IsActive())
        Finished();
    else
        CleanupMove();
}
