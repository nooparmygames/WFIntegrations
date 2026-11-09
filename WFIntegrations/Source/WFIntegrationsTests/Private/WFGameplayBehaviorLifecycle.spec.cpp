// Copyright NoOpArmy 2026
#include "WFGameplayBehaviorLifecycleTestTypes.h"
#include "WFIntegrationsTestTypes.h"
#include "WFUseGameplayBehaviorSmartObjectAction.h"
#include "WFBrainComponent.h"
#include "WFConsideration.h"
#include "BuiltIn/WFTestConsideration.h"
#include "AIController.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameplayBehaviorSmartObjectBehaviorDefinition.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FWFGameplayBehaviorLifecycleSpec, "AI.WiseFeline.Integrations.GameplayBehaviorLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    UWorld* World = nullptr;
    ACharacter* Pawn = nullptr;
    AAIController* Controller = nullptr;
    UWFBrainComponent* Brain = nullptr;
    UWFUseGameplayBehaviorSmartObjectAction* Action = nullptr;
    UWFTestStationBehavior* Behavior = nullptr;
    USmartObjectSubsystem* SmartObjects = nullptr;
    FSmartObjectSlotHandle Slot;
    int32 ReleaseCount = 0;

    void ActivateAction()
    {
        Brain->Think();
        Brain->TickComponent(0.0f, LEVELTICK_All, nullptr);
    }

    void StartInteraction()
    {
        ActivateAction();
        TestEqual(TEXT("The station behavior starts"), Behavior->Starts, 1);
        TestEqual(TEXT("The slot is occupied"), SmartObjects->GetSlotState(Slot), ESmartObjectSlotState::Occupied);
    }

    void CheckReleased()
    {
        TestEqual(TEXT("The slot was released once"), ReleaseCount, 1);
        TestEqual(TEXT("The slot is free"), SmartObjects->GetSlotState(Slot), ESmartObjectSlotState::Free);
    }
END_DEFINE_SPEC(FWFGameplayBehaviorLifecycleSpec)

void FWFGameplayBehaviorLifecycleSpec::Define()
{
    BeforeEach([this]()
    {
        const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
            .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).CreateFXSystem(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
            ERHIFeatureLevel::Num, &Values);
        check(World);
        auto* Nav = NewObject<UNavigationSystemV1>(World);
        World->SetNavigationSystem(Nav);
        Nav->MainNavData = World->SpawnActor<AWFIntegrationTestNavData>();
        SmartObjects = USmartObjectSubsystem::GetCurrent(World);
        check(SmartObjects);
        World->UpdateWorldComponents(false, false);
        static_cast<UWorldSubsystem*>(SmartObjects)->OnWorldBeginPlay(*World);

        auto* Definition = NewObject<USmartObjectDefinition>(World);
        auto* StationDefinition = NewObject<UGameplayBehaviorSmartObjectBehaviorDefinition>(Definition);
        auto* Config = NewObject<UWFTestStationBehaviorConfig>(StationDefinition);
        Behavior = NewObject<UWFTestStationBehavior>(Config);
        Config->Behavior = Behavior;
        StationDefinition->GameplayBehaviorConfig = Config;
        Definition->DebugAddSlot().BehaviorDefinitions.Add(StationDefinition);
        const auto ObjectHandle = SmartObjects->CreateSmartObject(*Definition, FTransform(FVector(500, 0, 0)), {});
        TArray<FSmartObjectSlotHandle> Slots;
        SmartObjects->GetAllSlots(ObjectHandle, Slots);
        check(Slots.Num() == 1);
        Slot = Slots[0];
        ReleaseCount = 0;
        SmartObjects->GetSlotEventDelegate(Slot)->AddLambda([this](const FSmartObjectEventData& Event)
        {
            if (Event.Reason == ESmartObjectChangeReason::OnReleased)
                ++ReleaseCount;
        });

        Pawn = World->SpawnActor<ACharacter>();
        Controller = World->SpawnActor<AWFIntegrationTestController>();
        Controller->Possess(Pawn);
        Brain = NewObject<UWFBrainComponent>(Controller);
        Brain->RegisterComponent();
        Brain->ControlledActor = Pawn;
        Brain->AIController = Controller;
        Brain->bManualExecution = true;
        Brain->OnPossessedPawnChanged(nullptr, Pawn);
        Controller->OnPossessedPawnChanged.AddDynamic(Brain, &UWFBrainComponent::OnPossessedPawnChanged);
        Action = NewObject<UWFUseGameplayBehaviorSmartObjectAction>(Brain);
        Action->bMoveToSlot = false;
        Action->bLockAILogic = false;
        Brain->AddAction(Action);
        auto* Score = NewObject<UWFTestConsideration>(Brain);
        Score->TestScore = 1.0f;
        Score->ApplyCurvePreset();
        Brain->AddConsideration(Action, Score);
    });

    AfterEach([this]()
    {
        Behavior->DuringTrigger = nullptr;
        Behavior->DuringEnd = nullptr;
        Action->Finished();
        World->DestroyWorld(false);
        World = nullptr;
    });

    It("aborts the active behavior once when its action is interrupted", [this]()
    {
        StartInteraction();
        Behavior->DuringEnd = [this]() { Action->Finished(); };
        Action->Finished();
        Action->Finished();
        TestEqual(TEXT("Behavior ended once despite reentrant action cleanup"), Behavior->Ends, 1);
        TestEqual(TEXT("Behavior was interrupted"), Behavior->Interruptions, 1);
        TestEqual(TEXT("Original avatar was aborted"), Behavior->EndedAvatar.Get(), static_cast<AActor*>(Pawn));
        CheckReleased();
    });

    It("leaves an equal-priority reservation with its original owner", [this]()
    {
        const auto Reservation = SmartObjects->MarkSlotAsClaimed(Slot, ESmartObjectClaimPriority::Normal);
        ActivateAction();
        TestFalse(TEXT("Equal priority cannot start the action"), Action->IsActive());
        TestEqual(TEXT("No competing behavior started"), Behavior->Starts, 0);
        TestEqual(TEXT("Reservation was not released"), ReleaseCount, 0);
        TestTrue(TEXT("Original owner can release the reservation"), SmartObjects->MarkSlotAsFree(Reservation));
        CheckReleased();
    });

    It("takes a lower-priority reservation and cleans its behavior and replacement claim once", [this]()
    {
        const auto Reservation = SmartObjects->MarkSlotAsClaimed(Slot, ESmartObjectClaimPriority::Normal);
        TestTrue(TEXT("Original reservation created"), Reservation.IsValid());
        Action->ClaimPriority = ESmartObjectClaimPriority::High;
        StartInteraction();
        TestEqual(TEXT("Takeover released the original reservation once"), ReleaseCount, 1);
        Action->Finished();
        Action->Finished();
        TestEqual(TEXT("Replacement behavior interrupted once"), Behavior->Interruptions, 1);
        TestEqual(TEXT("Each of the two owners released once"), ReleaseCount, 2);
        TestEqual(TEXT("Replacement slot is free"), SmartObjects->GetSlotState(Slot), ESmartObjectSlotState::Free);
    });

    It("cannot replace an occupied slot even at higher priority", [this]()
    {
        const auto Occupant = SmartObjects->MarkSlotAsClaimed(Slot, ESmartObjectClaimPriority::Normal);
        TestNotNull(TEXT("Original owner occupies the slot"),
            SmartObjects->MarkSlotAsOccupied<UGameplayBehaviorSmartObjectBehaviorDefinition>(Occupant));
        Action->ClaimPriority = ESmartObjectClaimPriority::High;
        ActivateAction();
        TestFalse(TEXT("Higher priority cannot start the action"), Action->IsActive());
        TestEqual(TEXT("No competing behavior started"), Behavior->Starts, 0);
        TestEqual(TEXT("Occupant was not released"), ReleaseCount, 0);
        TestEqual(TEXT("Slot remains occupied"), SmartObjects->GetSlotState(Slot), ESmartObjectSlotState::Occupied);
        TestTrue(TEXT("Original occupant can still release"), SmartObjects->MarkSlotAsFree(Occupant));
        CheckReleased();
    });

    It("stops a displaced gameplay-behavior move before starting its behavior", [this]()
    {
        Action->bMoveToSlot = true;
        ActivateAction();
        TestEqual(TEXT("Moving action has reserved the slot"), SmartObjects->GetSlotState(Slot), ESmartObjectSlotState::Claimed);
        const auto Replacement = SmartObjects->MarkSlotAsClaimed(Slot, ESmartObjectClaimPriority::High);
        Action->TickAction(0.0f);
        TestTrue(TEXT("Higher priority receives the replacement"), Replacement.IsValid());
        TestFalse(TEXT("Displaced action finishes"), Action->IsActive());
        TestEqual(TEXT("Displaced move is stopped"), Controller->GetPathFollowingComponent()->GetStatus(), EPathFollowingStatus::Idle);
        TestEqual(TEXT("Displaced behavior never starts"), Behavior->Starts, 0);
        TestEqual(TEXT("Original reservation released once"), ReleaseCount, 1);
        TestNotNull(TEXT("Replacement remains usable"),
            SmartObjects->MarkSlotAsOccupied<UGameplayBehaviorSmartObjectBehaviorDefinition>(Replacement));
        TestTrue(TEXT("Replacement owner releases its slot"), SmartObjects->MarkSlotAsFree(Replacement));
        TestEqual(TEXT("Each owner released once"), ReleaseCount, 2);
    });

    It("preserves natural completion without a second behavior end", [this]()
    {
        StartInteraction();
        Behavior->EndBehavior(*Pawn, false);
        Action->TickAction(0.0f);
        TestFalse(TEXT("Action observes the completed task"), Action->IsActive());
        TestEqual(TEXT("Behavior ended once"), Behavior->Ends, 1);
        TestEqual(TEXT("Natural completion was not interrupted"), Behavior->Interruptions, 0);
        CheckReleased();
    });

    It("does not abort a behavior which completes synchronously during Trigger", [this]()
    {
        Behavior->bCompleteSynchronously = true;
        ActivateAction();
        Action->TickAction(0.0f);
        TestEqual(TEXT("Behavior ended once"), Behavior->Ends, 1);
        TestEqual(TEXT("Synchronous completion was not interrupted"), Behavior->Interruptions, 0);
        CheckReleased();
    });

    It("aborts the original avatar after real controller unpossession", [this]()
    {
        StartInteraction();
        Controller->UnPossess();
        TestNull(TEXT("Controller no longer has a pawn"), Controller->GetPawn());
        TestFalse(TEXT("Brain deactivated the action"), Action->IsActive());
        TestEqual(TEXT("Original behavior was interrupted"), Behavior->Interruptions, 1);
        TestEqual(TEXT("Cleanup retained the original avatar"), Behavior->EndedAvatar.Get(), static_cast<AActor*>(Pawn));
        CheckReleased();
    });

    It("cleans the old avatar when action ownership has already changed", [this]()
    {
        StartInteraction();
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* NextPawn = World->SpawnActor<ACharacter>(FVector(3000, 0, 0), FRotator::ZeroRotator, Spawn);
        auto* NextController = World->SpawnActor<AAIController>();
        NextController->Possess(NextPawn);
        Action->SetControlledActor(NextPawn);
        Action->SetAIController(NextController);
        Action->OnPossessionChanged();
        TestEqual(TEXT("Only the original avatar was aborted"), Behavior->EndedAvatar.Get(), static_cast<AActor*>(Pawn));
        TestEqual(TEXT("Behavior interrupted once"), Behavior->Interruptions, 1);
        TestFalse(TEXT("Action stopped before ownership refresh"), Action->IsActive());
        CheckReleased();
    });

    It("cleans a behavior which cancels its action synchronously during Trigger", [this]()
    {
        Behavior->DuringTrigger = [this]() { Action->Finished(); };
        ActivateAction();
        TestFalse(TEXT("The action remains cancelled"), Action->IsActive());
        TestEqual(TEXT("The behavior was aborted after Trigger returned"), Behavior->Interruptions, 1);
        TestEqual(TEXT("Behavior ended once"), Behavior->Ends, 1);
        CheckReleased();
    });

    It("does not abort twice when the occupied slot is invalidated", [this]()
    {
        StartInteraction();
        SmartObjects->DestroySmartObject(Slot.GetSmartObjectHandle());
        Action->TickAction(0.0f);
        TestFalse(TEXT("Invalidation finished the action"), Action->IsActive());
        TestEqual(TEXT("Invalidated behavior ended once"), Behavior->Ends, 1);
        TestEqual(TEXT("Invalidated behavior interrupted once"), Behavior->Interruptions, 1);
        TestEqual(TEXT("Original avatar was aborted"), Behavior->EndedAvatar.Get(), static_cast<AActor*>(Pawn));
    });

    It("starts the behavior after the engine move task reaches its slot", [this]()
    {
        Action->bMoveToSlot = true;
        ActivateAction();
        auto* Path = Controller->GetPathFollowingComponent();
        TestTrue(TEXT("Movement is active"), Path->GetStatus() != EPathFollowingStatus::Idle);
        TestEqual(TEXT("Behavior waits for arrival"), Behavior->Starts, 0);
        Path->OnPathFinished(FPathFollowingResult(FPathFollowingResultFlags::Success));
        TestEqual(TEXT("Arrival starts the behavior"), Behavior->Starts, 1);
        TestEqual(TEXT("Arrival occupies the slot"), SmartObjects->GetSlotState(Slot), ESmartObjectSlotState::Occupied);
        Behavior->EndBehavior(*Pawn, false);
        Action->TickAction(0.0f);
        TestFalse(TEXT("Action observes successful completion"), Action->IsActive());
        TestEqual(TEXT("Natural completion after arrival was not interrupted"), Behavior->Interruptions, 0);
        CheckReleased();
    });

    It("cleans synchronous Trigger cancellation when arrival starts the behavior", [this]()
    {
        Action->bMoveToSlot = true;
        Behavior->DuringTrigger = [this]() { Action->Finished(); };
        ActivateAction();
        Controller->GetPathFollowingComponent()->OnPathFinished(FPathFollowingResult(FPathFollowingResultFlags::Success));
        TestEqual(TEXT("Arrival invoked Trigger"), Behavior->Starts, 1);
        TestFalse(TEXT("The action remains cancelled"), Action->IsActive());
        TestEqual(TEXT("The arrived behavior was aborted once"), Behavior->Interruptions, 1);
        TestEqual(TEXT("Behavior ended once"), Behavior->Ends, 1);
        CheckReleased();
    });
}
#endif
