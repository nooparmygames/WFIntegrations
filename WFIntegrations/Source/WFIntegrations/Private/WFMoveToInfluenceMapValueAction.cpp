// Copyright NoOpArmy 2026
#include "WFMoveToInfluenceMapValueAction.h"
#include "WFInfluenceMapsSubsystem.h"
#include "WFUtilities.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
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
}

bool UWFMoveToInfluenceMapValueAction::FindDestination(FVector& OutDestination) const
{
    AActor* Self = GetControlledActor();
    AAIController* Controller = GetAIController();
    APawn* Pawn = IsValid(Controller) ? Controller->GetPawn() : nullptr;
    if (!IsValid(Self) || !IsValid(Pawn) || Pawn != Self || MapName.IsNone()
        || !FMath::IsFinite(SearchValue) || SearchRadius < 0 || SearchRadius > MAX_int32 / 4
        || !FMath::IsFinite(AcceptanceRadius) || AcceptanceRadius < 0
        || NavigationProjectionExtent.ContainsNaN() || NavigationProjectionExtent.GetMin() < 0)
        return false;

    UWFInfluenceMapsSubsystem* Subsystem = UWFInfluenceMapsSubsystem::Get(Self->GetWorld());
    AWFInfluenceMap* Map = IsValid(Subsystem) ? Subsystem->GetMap(MapName) : nullptr;
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

    const FVector FirstCellCenter = Map->MapToWorldPosition(0, 0);
    if (!IsFiniteLocation(Center) || !IsFiniteLocation(Self->GetActorLocation()) || !IsFiniteLocation(FirstCellCenter))
        return false;
    // Validate in double precision before the map converts world coordinates to int32.
    const double CellX = (Center.X - FirstCellCenter.X) / Map->CellSize + 0.5;
    const double CellY = (Center.Y - FirstCellCenter.Y) / Map->CellSize + 0.5;
    if (!FMath::IsFinite(CellX) || !FMath::IsFinite(CellY)
        || CellX < MIN_int32 / 2 || CellY < MIN_int32 / 2
        || CellX > MAX_int32 / 2 || CellY > MAX_int32 / 2)
        return false;
    const FIntVector CenterCell = Map->WorldToMapPosition(Center);
    // First-match and interest searches expect a non-empty rectangle.
    if (int64(CenterCell.X) + SearchRadius < 0 || int64(CenterCell.Y) + SearchRadius < 0
        || int64(CenterCell.X) - SearchRadius >= Map->Width || int64(CenterCell.Y) - SearchRadius >= Map->Height)
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

    FVector Selected = Map->MapToWorldPosition(ResultCell);
    Selected.Z = Self->GetActorLocation().Z;
    if (!IsFiniteLocation(Selected)) return false;
    UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Self->GetWorld());
    const ANavigationData* NavData = Navigation
        ? Navigation->GetNavDataForProps(Pawn->GetNavAgentPropertiesRef(), Pawn->GetNavAgentLocation()) : nullptr;
    if (!IsValid(NavData)) return false;
    FNavLocation Projected;
    if (!Navigation->ProjectPointToNavigation(Selected, Projected, NavigationProjectionExtent, NavData)
        || !IsFiniteLocation(Projected.Location))
        return false;
    // Navigation projection must not silently choose a different influence value.
    const double ProjectedX = (Projected.Location.X - FirstCellCenter.X) / Map->CellSize + 0.5;
    const double ProjectedY = (Projected.Location.Y - FirstCellCenter.Y) / Map->CellSize + 0.5;
    if (!FMath::IsFinite(ProjectedX) || !FMath::IsFinite(ProjectedY)
        || ProjectedX < MIN_int32 / 2 || ProjectedY < MIN_int32 / 2
        || ProjectedX > MAX_int32 / 2 || ProjectedY > MAX_int32 / 2
        || Map->WorldToMapPosition(Projected.Location) != ResultCell)
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
    FAIMoveRequest Request(Destination);
    Request.SetUsePathfinding(true);
    Request.SetAllowPartialPath(false);
    Request.SetProjectGoalLocation(false);
    Request.SetAcceptanceRadius(AcceptanceRadius);
    Request.SetReachTestIncludesAgentRadius(bIncludeAgentRadius);
    Request.SetReachTestIncludesGoalRadius(false);
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

void UWFMoveToInfluenceMapValueAction::CleanupMove()
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
        PathFollowing->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, RequestID);
}

void UWFMoveToInfluenceMapValueAction::OnDeactivate_Implementation()
{
    ++ActivationGeneration;
    Super::OnDeactivate_Implementation();
    CleanupMove();
}

void UWFMoveToInfluenceMapValueAction::OnPossessionChanged_Implementation()
{
    ++ActivationGeneration;
    Super::OnPossessionChanged_Implementation();
    Finished();
    CleanupMove();
}
