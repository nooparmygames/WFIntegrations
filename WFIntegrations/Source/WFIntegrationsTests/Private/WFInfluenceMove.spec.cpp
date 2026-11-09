// Copyright NoOpArmy 2026
#include "WFInfluenceMoveTestTypes.h"
#include "WFIntegrationsTestTypes.h"
#include "WFMoveToInfluenceMapValueAction.h"
#include "WFInfluenceMapsSubsystem.h"
#include "WFBrainComponent.h"
#include "BuiltIn/WFTestConsideration.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "Curves/CurveFloat.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
BEGIN_DEFINE_SPEC(FWFInfluenceMoveSpec, "AI.WiseFeline.Integrations.InfluenceMove",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    UGameInstance* GameInstance = nullptr;
    UWorld* World = nullptr;
    UNavigationSystemV1* Navigation = nullptr;
    AWFInfluenceMoveTestNavData* NavData = nullptr;
    AWFIntegrationTestInfluenceMap* Map = nullptr;
    ACharacter* Pawn = nullptr;
    AWFInfluenceMoveTestController* Controller = nullptr;
    UWFInfluenceMoveTestPathFollowing* Path = nullptr;
    UWFBrainComponent* Brain = nullptr;
    UWFMoveToInfluenceMapValueAction* Action = nullptr;
    bool bReady = false;

    void Activate()
    {
        Brain->Think();
        Brain->TickComponent(0.0f, LEVELTICK_All, nullptr);
    }

    void ResetMove()
    {
        Action->Finished();
        Controller->StopMovement();
    }

    void ExpectDestination(int32 X, int32 Y)
    {
        TestTrue(TEXT("A projected destination was selected"), Action->bHasDestination);
        TestEqual(TEXT("Selected map cell"), Map->WorldToMapPosition(Action->Destination), FIntVector(X, Y, 0));
    }

    UBlackboardComponent* AddBlackboard()
    {
        auto* Data = NewObject<UBlackboardData>(World);
        FBlackboardEntry Key;
        Key.EntryName = TEXT("Center");
        Key.KeyType = NewObject<UBlackboardKeyType_Vector>(Data);
        Data->Keys.Add(Key);
        UBlackboardComponent* Blackboard = nullptr;
        if (!TestTrue(TEXT("Controller blackboard initialized"), Controller->UseBlackboard(Data, Blackboard))) return nullptr;
        return Blackboard;
    }

    UWFInfluenceMapTemplate* MakeTemplate(float Constant)
    {
        auto* Template = NewObject<UWFInfluenceMapTemplate>(World);
        Template->Radius = 5;
        Template->Curve = NewObject<UCurveFloat>(Template);
        Template->Curve->FloatCurve.AddKey(0, Constant);
        Template->Curve->FloatCurve.AddKey(1, Constant);
        return Template;
    }
END_DEFINE_SPEC(FWFInfluenceMoveSpec)

void FWFInfluenceMoveSpec::Define()
{
    BeforeEach([this]()
    {
        bReady = false;
        Action = nullptr;
        World = nullptr;
        GameInstance = NewObject<UGameInstance>(GEngine);
        GameInstance->InitializeStandalone();
        World = GameInstance->GetWorld();
        if (!TestNotNull(TEXT("Isolated game world"), World)) return;
        World->CreateAISystem();
        Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
        if (!Navigation)
        {
            Navigation = NewObject<UNavigationSystemV1>(World);
            World->SetNavigationSystem(Navigation);
        }
        NavData = World->SpawnActor<AWFInfluenceMoveTestNavData>();
        Navigation->MainNavData = NavData;
        Map = World->SpawnActor<AWFIntegrationTestInfluenceMap>();
        Map->Width = 5;
        Map->Height = 5;
        Map->CellSize = 100;
        Map->MapName = TEXT("MoveTest");
        Map->InitializeForTest();
        Map->SetCellValue(3, 1, 10);
        Map->SetCellValue(1, 3, -10);
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Pawn = World->SpawnActor<ACharacter>(FVector(50, 50, 100), FRotator::ZeroRotator, Spawn);
        Controller = World->SpawnActor<AWFInfluenceMoveTestController>();
        if (!TestNotNull(TEXT("Pawn spawned"), Pawn) || !TestNotNull(TEXT("Controller spawned"), Controller)) return;
        Controller->Possess(Pawn);
        Path = Cast<UWFInfluenceMoveTestPathFollowing>(Controller->GetPathFollowingComponent());
        if (!TestNotNull(TEXT("Controlled path-following fixture"), Path)) return;
        Brain = NewObject<UWFBrainComponent>(Controller);
        Brain->RegisterComponent();
        Brain->ControlledActor = Pawn;
        Brain->AIController = Controller;
        Brain->bManualExecution = true;
        Brain->OnPossessedPawnChanged(nullptr, Pawn);
        Controller->OnPossessedPawnChanged.AddDynamic(Brain, &UWFBrainComponent::OnPossessedPawnChanged);
        Action = NewObject<UWFMoveToInfluenceMapValueAction>(Brain);
        Action->MapName = Map->MapName;
        Action->SearchRadius = 5;
        Action->AcceptanceRadius = 10;
        Action->bIncludeAgentRadius = false;
        Brain->AddAction(Action);
        auto* Score = NewObject<UWFTestConsideration>(Brain);
        Score->TestScore = 1;
        Score->ApplyCurvePreset();
        Brain->AddConsideration(Action, Score);
        bReady = TestEqual(TEXT("Map is registered in the actual game-instance subsystem"),
            UWFInfluenceMapsSubsystem::Get(World)->GetMap(Action->MapName), static_cast<AWFInfluenceMap*>(Map));
    });

    AfterEach([this]()
    {
        if (Action) Action->Finished();
        if (GameInstance) GameInstance->Shutdown();
        if (World)
        {
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
        World = nullptr;
        GameInstance = nullptr;
    });

    It("selects unique highest and lowest cells and requests a complete path", [this]()
    {
        if (!bReady) return;
        Activate();
        ExpectDestination(3, 1);
        TestTrue(TEXT("Highest destination is moving"), Action->IsActive());
        TestFalse(TEXT("Partial paths disabled"), Controller->bAllowedPartialPath);
        ResetMove();
        Action->SearchMode = EWFInfluenceMapSearchMode::LowestValue;
        Action->SearchCondition = ESearchCondition::LessOrEqual;
        Action->SearchValue = -1;
        Activate();
        ExpectDestination(1, 3);
    });

    It("supports first-match selection and rejects absent or failing conditions", [this]()
    {
        if (!bReady) return;
        Map->SetCellValue(2, 2, 7);
        Action->SearchMode = EWFInfluenceMapSearchMode::FirstMatch;
        Action->SearchCondition = ESearchCondition::Equal;
        Action->SearchValue = 7;
        Activate();
        ExpectDestination(2, 2);
        ResetMove();
        Action->SearchValue = 999;
        Activate();
        TestFalse(TEXT("No match produces no destination"), Action->bHasDestination);
        TestFalse(TEXT("No match finishes"), Action->IsActive());
        Action->SearchMode = EWFInfluenceMapSearchMode::HighestValue;
        Action->SearchCondition = ESearchCondition::Less;
        Action->SearchValue = 5;
        Activate();
        TestFalse(TEXT("Extreme is tested after selection"), Action->bHasDestination);
    });

    It("reads a set controller blackboard center and handles missing keys", [this]()
    {
        if (!bReady) return;
        auto* Blackboard = AddBlackboard();
        if (!Blackboard) return;
        Action->SearchCenterMode = ECenterMode::BlackboardKey;
        Action->SearchCenterKey = TEXT("Center");
        Action->SearchRadius = 0;
        Activate();
        TestFalse(TEXT("Unset vector rejected"), Action->bHasDestination);
        Map->SetCellValue(4, 4, 9);
        Blackboard->SetValueAsVector(TEXT("Center"), Map->MapToWorldPosition(4, 4));
        Activate();
        ExpectDestination(4, 4);
        ResetMove();
        Action->SearchCenterKey = TEXT("Missing");
        Activate();
        TestFalse(TEXT("Missing key rejected"), Action->bHasDestination);
    });

    It("applies self subtraction and interest multiplication before testing the value", [this]()
    {
        if (!bReady) return;
        Map->SetCellValue(2, 2, 8);
        Action->bUseInterestSearch = true;
        Action->SelfTemplate = MakeTemplate(0);
        Action->SelfTemplate->GetMap()[Action->SelfTemplate->CalculateArrayIndex(8, 6)] = 20;
        Action->InterestTemplate = MakeTemplate(1);
        Action->InterestMagnitude = 2;
        Action->SearchValue = 15;
        Activate();
        ExpectDestination(2, 2);
        ResetMove();
        Action->SearchMode = EWFInfluenceMapSearchMode::FirstMatch;
        Action->SearchCondition = ESearchCondition::Equal;
        Action->SearchValue = 16;
        Activate();
        ExpectDestination(2, 2);
    });

    It("retains one destination across map changes and reports arrival and failure", [this]()
    {
        if (!bReady) return;
        Activate();
        const FVector Destination = Action->Destination;
        const FAIRequestID RequestID = Path->GetCurrentRequestId();
        Map->SetCellValue(4, 4, 100);
        Brain->Think();
        Brain->TickComponent(1, LEVELTICK_All, nullptr);
        TestEqual(TEXT("Active destination is stable"), Action->Destination, Destination);
        TestTrue(TEXT("Active request was retained"), Path->GetCurrentRequestId() == RequestID);
        Path->OnPathFinished(FPathFollowingResult(FPathFollowingResultFlags::Success));
        TestFalse(TEXT("Successful completion finishes"), Action->IsActive());
        TestTrue(TEXT("Success is exposed"), Action->bMoveSucceeded);
        Activate();
        ExpectDestination(4, 4);
        Path->OnPathFinished(FPathFollowingResult(EPathFollowingResult::Blocked, 0));
        TestFalse(TEXT("Blocked completion finishes"), Action->IsActive());
        TestFalse(TEXT("Failure is exposed"), Action->bMoveSucceeded);
    });

    It("cancels its own request and preserves a newer external move", [this]()
    {
        if (!bReady) return;
        Activate();
        Action->Finished();
        TestEqual(TEXT("Owned request stopped"), Path->GetStatus(), EPathFollowingStatus::Idle);
        Activate();
        Controller->MoveToLocation(FVector(1600, 1600, 100), 20);
        const auto ExternalID = Path->GetCurrentRequestId();
        Action->Finished();
        TestTrue(TEXT("External move survives"), Path->GetCurrentRequestId() == ExternalID && Path->GetStatus() != EPathFollowingStatus::Idle);
    });

    It("cleans movement on possession change and can run again on the new pawn", [this]()
    {
        if (!bReady) return;
        Activate();
        Controller->UnPossess();
        TestFalse(TEXT("Unpossession finishes action"), Action->IsActive());
        TestEqual(TEXT("Old move stopped"), Path->GetStatus(), EPathFollowingStatus::Idle);
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* NewPawn = World->SpawnActor<ACharacter>(FVector(150, 50, 100), FRotator::ZeroRotator, Spawn);
        Controller->Possess(NewPawn);
        TestEqual(TEXT("Same action has new owner"), Action->GetControlledActor(), static_cast<AActor*>(NewPawn));
        Activate();
        TestTrue(TEXT("New owner can move"), Action->IsActive());
        Action->SetControlledActor(nullptr);
        Action->SetAIController(nullptr);
        Action->OnPossessionChanged();
        TestFalse(TEXT("Null refresh finishes"), Action->IsActive());
        TestEqual(TEXT("Null refresh cancels old controller request"), Path->GetStatus(), EPathFollowingStatus::Idle);
    });

    It("fails safely for missing map, controller, navigation, projection or path", [this]()
    {
        if (!bReady) return;
        Action->MapName = TEXT("Missing");
        Activate();
        TestFalse(TEXT("Missing map"), Action->IsActive());
        Action->MapName = Map->MapName;
        Action->SetAIController(nullptr);
        Activate();
        TestFalse(TEXT("Missing controller"), Action->IsActive());
        Action->SetAIController(Controller);
        Navigation->MainNavData = nullptr;
        Activate();
        TestFalse(TEXT("Missing navigation data"), Action->bHasDestination);
        Navigation->MainNavData = NavData;
        NavData->bProject = false;
        Activate();
        TestFalse(TEXT("Projection failure"), Action->bHasDestination);
        NavData->bProject = true;
        NavData->ProjectionOffset = FVector(100, 0, 0);
        Activate();
        TestFalse(TEXT("Projection into another cell rejected"), Action->bHasDestination);
        NavData->ProjectionOffset = FVector::ZeroVector;
        Controller->bFindPath = false;
        Activate();
        TestTrue(TEXT("Path failure follows a valid projected selection"), Action->bHasDestination);
        TestFalse(TEXT("Path failure finishes"), Action->IsActive());
        TestFalse(TEXT("Path failure is not success"), Action->bMoveSucceeded);
    });

    It("rejects nonfinite queries, unusable templates and invalid map storage", [this]()
    {
        if (!bReady) return;
        Action->SearchValue = std::numeric_limits<float>::quiet_NaN();
        Activate();
        TestFalse(TEXT("NaN threshold"), Action->bHasDestination);
        Action->SearchValue = 1;
        Action->SearchRadius = -1;
        Activate();
        TestFalse(TEXT("Negative radius"), Action->bHasDestination);
        Action->SearchRadius = MAX_int32;
        Activate();
        TestFalse(TEXT("Overflowing radius"), Action->bHasDestination);
        Action->SearchRadius = 5;
        Action->NavigationProjectionExtent.X = std::numeric_limits<double>::infinity();
        Activate();
        TestFalse(TEXT("Nonfinite projection extent"), Action->bHasDestination);
        Action->NavigationProjectionExtent = FVector(100, 100, 500);
        Action->bUseInterestSearch = true;
        Action->InterestTemplate = NewObject<UWFInfluenceMapTemplate>(World);
        Activate();
        TestFalse(TEXT("Template without curve fails without an ensure"), Action->bHasDestination);
        Action->bUseInterestSearch = false;
        ++Map->Width;
        Activate();
        TestFalse(TEXT("Uninitialized map dimensions"), Action->bHasDestination);
        --Map->Width;
        Action->SearchCenterMode = ECenterMode::BlackboardKey;
        Action->SearchCenterKey = TEXT("Center");
        auto* Blackboard = AddBlackboard();
        if (!Blackboard) return;
        Blackboard->SetValueAsVector(TEXT("Center"), FVector(1.e100, 0, 0));
        Activate();
        TestFalse(TEXT("Extreme coordinates rejected before int conversion"), Action->bHasDestination);
    });

    It("uses canonical cell boundaries at decimal map anchors", [this]()
    {
        if (!bReady) return;
        Map->SetActorLocation(FVector(123.456, 234.567, 0));
        Map->CellSize = 10.1f;
        Map->UpdateAnchorBasedOnActorPosition();
        Map->SetCellValue(0, 0, 10);
        Action->SearchCenterMode = ECenterMode::BlackboardKey;
        Action->SearchCenterKey = TEXT("Center");
        Action->SearchRadius = 0;
        auto* Blackboard = AddBlackboard();
        if (!Blackboard) return;
        Blackboard->SetValueAsVector(TEXT("Center"), Map->GetActorLocation());
        Activate();
        ExpectDestination(0, 0);
    });

    It("handles already-at-goal and dispatch-time completion without waiting forever", [this]()
    {
        if (!bReady) return;
        Pawn->SetActorLocation(FVector(350, 150, 100));
        Activate();
        TestFalse(TEXT("Already-at-goal finishes immediately"), Action->IsActive());
        TestTrue(TEXT("Already-at-goal is success"), Action->bMoveSucceeded);
        Pawn->SetActorLocation(FVector(50, 50, 100));
        Path->bCompleteDuringRequest = true;
        Activate();
        TestFalse(TEXT("Dispatch-time completion does not leave a waiting action"), Action->IsActive());
        Path->bCompleteDuringRequest = false;
        Path->bReplaceDuringRequest = true;
        Activate();
        TestFalse(TEXT("Dispatch-time replacement finishes action"), Action->IsActive());
        TestTrue(TEXT("Replacement request survives"), Path->GetStatus() != EPathFollowingStatus::Idle);
    });

    It("does not adopt a request when an older move callback cancels the activation", [this]()
    {
        if (!bReady) return;
        Controller->MoveToLocation(FVector(1800, 1800, 100), 20);
        const FAIRequestID OldID = Path->GetCurrentRequestId();
        const FDelegateHandle Handle = Path->OnRequestFinished.AddLambda([this, OldID](FAIRequestID ID, const FPathFollowingResult& Result)
        {
            if (ID == OldID) Action->Finished();
        });
        Activate();
        Path->OnRequestFinished.Remove(Handle);
        TestFalse(TEXT("Cancelled action stays inactive"), Action->IsActive());
        TestEqual(TEXT("Request created after cancellation is cleaned up"), Path->GetStatus(), EPathFollowingStatus::Idle);
    });
}
#endif
