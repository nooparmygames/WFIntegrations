// Copyright NoOpArmy 2026
#include "WFIntegrationsTestTypes.h"
#include "WFBrainComponent.h"
#include "WFAction.h"
#include "BuiltIn/WFTestConsideration.h"
#include "WFClaimSmartObjectAction.h"
#include "WFHasClaimedSmartObjectConsideration.h"
#include "WFInfluenceMapConsideration.h"
#include "WFInfluenceMapsSubsystem.h"
#include "WFReleaseSmartObjectAction.h"
#include "WFSmartObjectConsideration.h"
#include "WFUseSmartObjectAction.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BlackboardKeyType_SOClaimHandle.h"
#include "SmartObjectBlueprintFunctionLibrary.h"
#include "SmartObjectSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FWFIntegrationsSpec, "AI.WiseFeline.Integrations.BlackboardOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

    bool bSetupReady = false;
    UWorld* World = nullptr;
    ACharacter* Pawn = nullptr;
    AWFIntegrationTestController* Controller = nullptr;
    UWFBrainComponent* Brain = nullptr;
    UBlackboardData* Data = nullptr;
    UBlackboardComponent* Blackboard = nullptr;
    USmartObjectSubsystem* SmartObjects = nullptr;
    UWFUseSmartObjectAction* Use = nullptr;
    TArray<FSmartObjectSlotHandle> Slots;

    UBlackboardComponent* AddBlackboard(AActor* Owner)
    {
        auto* Result = NewObject<UBlackboardComponent>(Owner);
        Owner->AddInstanceComponent(Result);
        Result->RegisterComponent();
        Result->InitializeBlackboard(*Data);
        return Result;
    }

    UObject* Cached(UObject* Object, const FName Name = TEXT("Blackboard"))
    {
        const auto* Property = FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name);
        return Property ? Property->GetObjectPropertyValue_InContainer(Object) : nullptr;
    }

    void SetCached(UObject* Object, UObject* Value, const FName Name)
    {
        auto* Property = FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name);
        check(Property);
        Property->SetObjectPropertyValue_InContainer(Object, Value);
    }

    FSmartObjectClaimHandle ReadClaim(UBlackboardComponent* Board, FName Key = TEXT("Claim"))
    {
        return USmartObjectBlueprintFunctionLibrary::GetValueAsSOClaimHandle(Board, Key);
    }

    void WriteClaim(UBlackboardComponent* Board, const FSmartObjectClaimHandle& Claim, FName Key = TEXT("Claim"))
    {
        USmartObjectBlueprintFunctionLibrary::SetValueAsSOClaimHandle(Board, Key, Claim);
    }

    void ActivateAction(UWFAction* Action)
    {
        for (UWFAction* Other : Brain->GetActions()) Other->Weight = 0.0f;
        Action->Weight = 1.0f;
        if (Action->GetConsiderations().IsEmpty())
        {
            auto* Score = NewObject<UWFTestConsideration>(Brain);
            Score->TestScore = 1.0f;
            Score->ApplyCurvePreset();
            Brain->AddConsideration(Action, Score);
        }
        Brain->Think();
        Brain->TickComponent(0.0f, LEVELTICK_All, nullptr);
    }

    UWFUseSmartObjectAction* MakeUse(const bool bMove = false)
    {
        Use = NewObject<UWFUseSmartObjectAction>(Brain);
        Use->ClaimHandleKey = TEXT("Claim");
        Use->BehaviorDefinitionClass = UWFIntegrationTestBehaviorDefinition::StaticClass();
        Use->bMoveToSlot = bMove;
        Use->UseDuration = 2.0f;
        Brain->AddAction(Use);
        return Use;
    }

    TArray<UObject*> MakeReaders()
    {
        auto* Claim = NewObject<UWFClaimSmartObjectAction>(Brain);
        auto* Release = NewObject<UWFReleaseSmartObjectAction>(Brain);
        Brain->AddAction(Claim);
        Brain->AddAction(Release);
        MakeUse();
        auto* Available = NewObject<UWFSmartObjectConsideration>(Brain);
        auto* HasClaim = NewObject<UWFHasClaimedSmartObjectConsideration>(Brain);
        auto* Influence = NewObject<UWFInfluenceMapConsideration>(Brain);
        Brain->AddConsideration(Use, Available);
        Brain->AddConsideration(Use, HasClaim);
        Brain->AddConsideration(Use, Influence);
        return {Claim, Release, Use, Available, HasClaim, Influence};
    }

END_DEFINE_SPEC(FWFIntegrationsSpec)

void FWFIntegrationsSpec::Define()
{
    BeforeEach([this]()
    {
        bSetupReady = false;
        World = nullptr;
        Use = nullptr;
        const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
            .CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false).CreateFXSystem(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
            ERHIFeatureLevel::Num, &Values);
        if (!TestNotNull(TEXT("Test world created"), World)) return;
        auto* Nav = NewObject<UNavigationSystemV1>(World);
        World->SetNavigationSystem(Nav);
        Nav->MainNavData = World->SpawnActor<AWFIntegrationTestNavData>();
        SmartObjects = USmartObjectSubsystem::GetCurrent(World);
        if (!TestNotNull(TEXT("Smart Object subsystem created"), SmartObjects)) return;
        World->UpdateWorldComponents(false, false);
        // Initialize the real world subsystem without starting unrelated game-instance systems.
        static_cast<UWorldSubsystem*>(SmartObjects)->OnWorldBeginPlay(*World);
        auto* Definition = NewObject<USmartObjectDefinition>(World);
        for (int32 Index = 0; Index < 2; ++Index)
            Definition->DebugAddSlot().BehaviorDefinitions.Add(NewObject<UWFIntegrationTestBehaviorDefinition>(Definition));
        const auto Handle = SmartObjects->CreateSmartObject(*Definition, FTransform(FVector(500, 0, 0)), {});
        Slots.Reset();
        SmartObjects->GetAllSlots(Handle, Slots);
        if (!TestEqual(TEXT("Two test slots created"), Slots.Num(), 2)) return;

        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Pawn = World->SpawnActor<ACharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
        Controller = World->SpawnActor<AWFIntegrationTestController>();
        Controller->Possess(Pawn);
        Data = NewObject<UBlackboardData>(World);
        for (const FName Name : {FName(TEXT("Claim")), FName(TEXT("OtherClaim"))})
        {
            FBlackboardEntry Entry;
            Entry.EntryName = Name;
            Entry.KeyType = NewObject<UBlackboardKeyType_SOClaimHandle>(Data);
            Data->Keys.Add(Entry);
        }
        for (const FName Name : {FName(TEXT("Result")), FName(TEXT("Center"))})
        {
            FBlackboardEntry Entry;
            Entry.EntryName = Name;
            Entry.KeyType = NewObject<UBlackboardKeyType_Vector>(Data);
            Data->Keys.Add(Entry);
        }
        if (!TestTrue(TEXT("Controller blackboard initialized"), Controller->UseBlackboard(Data, Blackboard))) return;
        Brain = NewObject<UWFBrainComponent>(Controller);
        Brain->RegisterComponent();
        Brain->ControlledActor = Pawn;
        Brain->AIController = Controller;
        Brain->bManualExecution = true;
        Brain->OnPossessedPawnChanged(nullptr, Pawn);
        Controller->OnPossessedPawnChanged.AddDynamic(Brain, &UWFBrainComponent::OnPossessedPawnChanged);
        Use = nullptr;
        bSetupReady = true;
    });

    AfterEach([this]()
    {
        if (Use) Use->Finished();
        if (World) World->DestroyWorld(false);
        World = nullptr;
    });

    It("discovers controller blackboards and refreshes all six instances when the pawn changes", [this]()
    {
        if (!bSetupReady) return;
        const auto Readers = MakeReaders();
        for (UObject* Reader : Readers)
            TestEqual(*Reader->GetClass()->GetName(), Cached(Reader), static_cast<UObject*>(Blackboard));
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* NextPawn = World->SpawnActor<ACharacter>(FVector(2000, 0, 0), FRotator::ZeroRotator, Spawn);
        auto* NextBoard = AddBlackboard(NextPawn);
        Controller->Possess(NextPawn);
        for (UObject* Reader : Readers)
        {
            TestEqual(TEXT("Same instance now caches pawn blackboard, ahead of controller"), Cached(Reader), static_cast<UObject*>(NextBoard));
            if (auto* Action = Cast<UWFAction>(Reader))
                TestEqual(TEXT("Action now belongs to next pawn"), Action->GetControlledActor(), static_cast<AActor*>(NextPawn));
            else
                TestEqual(TEXT("Consideration now belongs to next pawn"), Cast<UWFConsideration>(Reader)->GetControlledActor(), static_cast<AActor*>(NextPawn));
        }
        TestEqual(TEXT("Actions were not duplicated"), Brain->GetActions().Num(), 3);
    });

    It("refreshes a pawn-owned brain when a different controller takes possession", [this]()
    {
        if (!bSetupReady) return;
        Brain = NewObject<UWFBrainComponent>(Pawn);
        Brain->RegisterComponent();
        Brain->ControlledActor = Pawn;
        Brain->AIController = Controller;
        Brain->bManualExecution = true;
        Brain->OnControllerChanged(Pawn, nullptr, Controller);
        Pawn->ReceiveControllerChangedDelegate.AddDynamic(Brain, &UWFBrainComponent::OnControllerChanged);
        const auto Readers = MakeReaders();
        auto* NextController = World->SpawnActor<AWFIntegrationTestController>();
        UBlackboardComponent* NextBoard = nullptr;
        TestTrue(TEXT("New controller blackboard initialized"), NextController->UseBlackboard(Data, NextBoard));
        NextController->Possess(Pawn);
        for (UObject* Reader : Readers)
            TestEqual(TEXT("New controller blackboard replaced still-valid old cache"), Cached(Reader), static_cast<UObject*>(NextBoard));
        TestEqual(TEXT("Old controller blackboard remains valid"), Controller->GetBlackboardComponent(), Blackboard);
        TestEqual(TEXT("Actions were not duplicated"), Brain->GetActions().Num(), 3);
    });

    It("publishes smart-object search results to the controller blackboard", [this]()
    {
        if (!bSetupReady) return;
        auto* Available = NewObject<UWFSmartObjectConsideration>(Brain);
        Available->Initialize(Brain, Pawn, Controller);
        Available->ResultLocationKey = TEXT("Result");
        Available->bScoreByDistance = false;
        Available->OnBeginPlay();
        TestEqual(TEXT("Available slot scores one"), Available->GetValue(nullptr), 1.0f);
        TestEqual(TEXT("Slot location reached controller blackboard"), Blackboard->GetValueAsVector(TEXT("Result")), FVector(500, 0, 0));
    });

    It("clears all six caches on a null ownership refresh and tolerates missing owners", [this]()
    {
        if (!bSetupReady) return;
        for (UObject* Reader : MakeReaders())
        {
            if (auto* Action = Cast<UWFAction>(Reader))
            {
                Action->SetControlledActor(nullptr);
                Action->SetAIController(nullptr);
                Action->OnPossessionChanged();
                Action->OnActivate();
            }
            else
            {
                auto* Consideration = Cast<UWFConsideration>(Reader);
                Consideration->Initialize(Brain, nullptr, nullptr);
                Consideration->OnPossessionChanged();
                TestEqual(TEXT("Missing owner scores zero"), Consideration->GetValue(nullptr), 0.0f);
            }
            TestNull(TEXT("No old blackboard retained"), Cached(Reader));
        }
    });

    It("scores reservations at matching priority but never offers occupied slots", [this]()
    {
        if (!bSetupReady) return;
        TArray<FSmartObjectClaimHandle> Reservations;
        for (const auto Slot : Slots)
            Reservations.Add(SmartObjects->MarkSlotAsClaimed(Slot, ESmartObjectClaimPriority::Normal));
        auto* Available = NewObject<UWFSmartObjectConsideration>(Brain);
        Available->Initialize(Brain, Pawn, Controller);
        Available->bScoreByDistance = false;
        Available->OnBeginPlay();
        TestEqual(TEXT("Equal priority cannot offer another user's reservation"), Available->GetValue(nullptr), 0.0f);
        Available->ClaimPriority = ESmartObjectClaimPriority::High;
        TestEqual(TEXT("Higher priority can discover a reservation"), Available->GetValue(nullptr), 1.0f);
        for (const auto& Reservation : Reservations)
            TestNotNull(TEXT("Read-only scoring leaves the original claim usable"),
                SmartObjects->MarkSlotAsOccupied<UWFIntegrationTestBehaviorDefinition>(Reservation));
        TestEqual(TEXT("Higher priority cannot offer occupied slots"), Available->GetValue(nullptr), 0.0f);
        for (const auto& Reservation : Reservations)
            TestTrue(TEXT("Original occupant can release its slot"), SmartObjects->MarkSlotAsFree(Reservation));
    });

    It("claims a lower-priority reservation and preserves claims when no slot is available", [this]()
    {
        if (!bSetupReady) return;
        TArray<FSmartObjectClaimHandle> Reservations;
        for (const auto Slot : Slots)
            Reservations.Add(SmartObjects->MarkSlotAsClaimed(Slot, ESmartObjectClaimPriority::Normal));
        auto* Claim = NewObject<UWFClaimSmartObjectAction>(Brain);
        Claim->ClaimHandleKey = TEXT("Claim");
        Brain->AddAction(Claim);
        ActivateAction(Claim);
        TestFalse(TEXT("Equal priority publishes no claim"), ReadClaim(Blackboard).IsValid());
        Claim->ClaimPriority = ESmartObjectClaimPriority::High;
        ActivateAction(Claim);
        const auto Winner = ReadClaim(Blackboard);
        if (!TestTrue(TEXT("High priority discovers and claims the reserved slot"), Winner.IsValid())) return;
        TestFalse(TEXT("Replacement has a new user handle"), Reservations.Contains(Winner));
        TestNotNull(TEXT("Replacement can occupy the slot"), SmartObjects->MarkSlotAsOccupied<UWFIntegrationTestBehaviorDefinition>(Winner));
        ActivateAction(Claim);
        TestTrue(TEXT("Repeated claim preserves the current owner's publication"), ReadClaim(Blackboard) == Winner);
        for (const auto& Reservation : Reservations)
        {
            if (Reservation.SlotHandle != Winner.SlotHandle)
                TestNotNull(TEXT("Other reservation remains usable"), SmartObjects->MarkSlotAsOccupied<UWFIntegrationTestBehaviorDefinition>(Reservation));
        }
        Blackboard->ClearValue(TEXT("Claim"));
        ActivateAction(Claim);
        TestFalse(TEXT("High priority cannot claim either occupied slot"), ReadClaim(Blackboard).IsValid());
        TestTrue(TEXT("Winning owner releases its claim"), SmartObjects->MarkSlotAsFree(Winner));
    });

    It("uses a lower-priority reservation and releases only its replacement claim", [this]()
    {
        if (!bSetupReady) return;
        TArray<FSmartObjectClaimHandle> Reservations;
        for (const auto Slot : Slots)
            Reservations.Add(SmartObjects->MarkSlotAsClaimed(Slot, ESmartObjectClaimPriority::Normal));
        MakeUse();
        ActivateAction(Use);
        TestFalse(TEXT("Equal priority cannot activate use"), Use->IsActive());
        TestFalse(TEXT("Equal priority publishes no claim"), ReadClaim(Blackboard).IsValid());
        Use->ClaimPriority = ESmartObjectClaimPriority::High;
        ActivateAction(Use);
        const auto Winner = ReadClaim(Blackboard);
        if (!TestTrue(TEXT("High priority claims the reserved slot"), Winner.IsValid())) return;
        TestEqual(TEXT("Replacement occupies the slot"), SmartObjects->GetSlotState(Winner.SlotHandle), ESmartObjectSlotState::Occupied);
        Use->Finished();
        Use->Finished();
        TestEqual(TEXT("Interruption releases the replacement"), SmartObjects->GetSlotState(Winner.SlotHandle), ESmartObjectSlotState::Free);
        TestFalse(TEXT("Interruption clears the replacement publication"), ReadClaim(Blackboard).IsValid());
        for (const auto Slot : Slots)
        {
            const auto Occupant = Slot == Winner.SlotHandle
                ? SmartObjects->MarkSlotAsClaimed(Slot, ESmartObjectClaimPriority::Normal)
                : Reservations[Slots.IndexOfByKey(Slot)];
            TestNotNull(TEXT("Each occupied slot has a live owner"), SmartObjects->MarkSlotAsOccupied<UWFIntegrationTestBehaviorDefinition>(Occupant));
        }
        ActivateAction(Use);
        TestFalse(TEXT("High priority cannot use either occupied slot"), Use->IsActive());
        for (const auto Slot : Slots)
            TestEqual(TEXT("Failed use preserves the occupant"), SmartObjects->GetSlotState(Slot), ESmartObjectSlotState::Occupied);
    });

    It("finds a blackboard created after initialization before every read or activation", [this]()
    {
        if (!bSetupReady) return;
        AActor* Owner = World->SpawnActor<AActor>();
        const auto Readers = MakeReaders();
        for (UObject* Reader : Readers)
        {
            if (auto* Action = Cast<UWFAction>(Reader))
            {
                Action->SetControlledActor(Owner);
                Action->SetAIController(nullptr);
                Action->OnPossessionChanged();
            }
            else
            {
                auto* Consideration = Cast<UWFConsideration>(Reader);
                Consideration->Initialize(Brain, Owner, nullptr);
                Consideration->OnPossessionChanged();
            }
            TestNull(TEXT("Starts without a blackboard"), Cached(Reader));
        }
        auto* LateBoard = AddBlackboard(Owner);
        for (UObject* Reader : Readers)
        {
            if (auto* Action = Cast<UWFAction>(Reader)) { Action->OnActivate(); Action->OnDeactivate(); }
            else Cast<UWFConsideration>(Reader)->GetValue(nullptr);
            TestEqual(TEXT("Late-created blackboard resolved"), Cached(Reader), static_cast<UObject*>(LateBoard));
        }
    });

    It("clears a displaced claim publication without clearing a replacement blackboard value", [this]()
    {
        if (!bSetupReady) return;
        auto* Claim = NewObject<UWFClaimSmartObjectAction>(Brain);
        Claim->ClaimHandleKey = TEXT("Claim");
        Brain->AddAction(Claim);
        ActivateAction(Claim);
        const auto Original = ReadClaim(Blackboard);
        const auto Replacement = SmartObjects->MarkSlotAsClaimed(Original.SlotHandle, ESmartObjectClaimPriority::High);
        TestTrue(TEXT("Higher priority replaces the published claim"), Replacement.IsValid());
        TestFalse(TEXT("Displaced publication is cleared immediately"), ReadClaim(Blackboard).IsValid());
        ActivateAction(Claim);
        const auto Next = ReadClaim(Blackboard);
        TestTrue(TEXT("Original action can claim the other slot"), Next.IsValid());
        WriteClaim(Blackboard, Replacement);
        TestTrue(TEXT("Next claim can be released"), SmartObjects->MarkSlotAsFree(Next));
        TestTrue(TEXT("Release callback preserves a newer publication"), ReadClaim(Blackboard) == Replacement);
        TestNotNull(TEXT("Winning claim remains usable"), SmartObjects->MarkSlotAsOccupied<UWFIntegrationTestBehaviorDefinition>(Replacement));
    });

    It("stops a displaced owned move without releasing the new reservation", [this]()
    {
        if (!bSetupReady) return;
        ActivateAction(MakeUse(true));
        const auto Original = ReadClaim(Blackboard);
        const auto Replacement = SmartObjects->MarkSlotAsClaimed(Original.SlotHandle, ESmartObjectClaimPriority::High);
        TestTrue(TEXT("Higher priority takes the moving user's reservation"), Replacement.IsValid());
        TestFalse(TEXT("Displaced action finishes immediately"), Use->IsActive());
        TestEqual(TEXT("Displaced movement is stopped"), Controller->GetPathFollowingComponent()->GetStatus(), EPathFollowingStatus::Idle);
        TestFalse(TEXT("Displaced publication is cleared"), ReadClaim(Blackboard).IsValid());
        Use->Finished();
        TestNotNull(TEXT("Repeated cleanup leaves replacement usable"), SmartObjects->MarkSlotAsOccupied<UWFIntegrationTestBehaviorDefinition>(Replacement));
    });

    It("preserves a replacement published synchronously while releasing a claim", [this]()
    {
        if (!bSetupReady) return;
        auto* Claim = NewObject<UWFClaimSmartObjectAction>(Brain);
        Claim->ClaimHandleKey = TEXT("Claim");
        Brain->AddAction(Claim);
        ActivateAction(Claim);
        const auto Original = ReadClaim(Blackboard);
        FSmartObjectClaimHandle Replacement;
        const auto Observer = Blackboard->RegisterObserver(Blackboard->GetKeyID(TEXT("Claim")), Brain,
            FOnBlackboardChangeNotification::CreateLambda([this, Original, &Replacement](const UBlackboardComponent&, FBlackboard::FKey)
            {
                if (!Replacement.IsValid() && !ReadClaim(Blackboard).IsValid())
                {
                    Replacement = SmartObjects->MarkSlotAsClaimed(Original.SlotHandle, ESmartObjectClaimPriority::Normal);
                    WriteClaim(Blackboard, Replacement);
                }
                return EBlackboardNotificationResult::ContinueObserving;
            }));
        auto* Release = NewObject<UWFReleaseSmartObjectAction>(Brain);
        Release->ClaimHandleKey = TEXT("Claim");
        Brain->AddAction(Release);
        ActivateAction(Release);
        Blackboard->UnregisterObserver(Blackboard->GetKeyID(TEXT("Claim")), Observer);
        TestTrue(TEXT("Release observer claimed the freed slot"), Replacement.IsValid());
        TestTrue(TEXT("Release action preserved the replacement publication"), ReadClaim(Blackboard) == Replacement);
        TestNotNull(TEXT("Replacement remains usable"), SmartObjects->MarkSlotAsOccupied<UWFIntegrationTestBehaviorDefinition>(Replacement));
    });

    It("observes a borrowed reservation release without replacing its invalidation callback", [this]()
    {
        if (!bSetupReady) return;
        const auto Original = SmartObjects->MarkSlotAsClaimed(Slots[0], ESmartObjectClaimPriority::Normal);
        WriteClaim(Blackboard, Original);
        int32 Invalidations = 0;
        SmartObjects->RegisterSlotInvalidationCallback(Original, FOnSlotInvalidated::CreateLambda(
            [&Invalidations](const FSmartObjectClaimHandle&, ESmartObjectSlotState) { ++Invalidations; }));
        ActivateAction(MakeUse(true));
        const auto Replacement = SmartObjects->MarkSlotAsClaimed(Original.SlotHandle, ESmartObjectClaimPriority::High);
        TestEqual(TEXT("Original invalidation callback remains installed"), Invalidations, 1);
        TestFalse(TEXT("Borrowing action stops when its reservation is lost"), Use->IsActive());
        TestTrue(TEXT("Borrowed publication remains its original owner's responsibility"), ReadClaim(Blackboard) == Original);
        TestNotNull(TEXT("Replacement remains usable"), SmartObjects->MarkSlotAsOccupied<UWFIntegrationTestBehaviorDefinition>(Replacement));
        SmartObjects->UnregisterSlotInvalidationCallback(Replacement);
    });

    It("claims and releases through the controller blackboard without leaking an overwritten reservation", [this]()
    {
        if (!bSetupReady) return;
        auto* Claim = NewObject<UWFClaimSmartObjectAction>(Brain);
        Claim->ClaimHandleKey = TEXT("Claim");
        Brain->AddAction(Claim);
        ActivateAction(Claim);
        const auto First = ReadClaim(Blackboard);
        TestTrue(TEXT("Controller blackboard received a claim"), First.IsValid());
        ActivateAction(Claim);
        TestTrue(TEXT("Repeated activation preserves the live claim"), ReadClaim(Blackboard) == First);
        auto* Has = NewObject<UWFHasClaimedSmartObjectConsideration>(Brain);
        Has->ClaimHandleKey = TEXT("Claim");
        Has->Initialize(Brain, Pawn, Controller);
        Has->OnBeginPlay();
        TestEqual(TEXT("Claim reader sees reservation"), Has->GetValue(nullptr), 1.0f);
        SmartObjects->MarkSlotAsFree(First);
        TestEqual(TEXT("Released handle left in blackboard is not live"), Has->GetValue(nullptr), 0.0f);
        ActivateAction(Claim);
        const auto Second = ReadClaim(Blackboard);
        TestTrue(TEXT("Stale handle does not block a new claim"), Second.IsValid() && Second != First);
        auto* Release = NewObject<UWFReleaseSmartObjectAction>(Brain);
        Release->ClaimHandleKey = TEXT("Claim");
        Brain->AddAction(Release);
        ActivateAction(Release);
        TestFalse(TEXT("Release clears controller key"), ReadClaim(Blackboard).IsValid());
        TestEqual(TEXT("Release frees slot"), SmartObjects->GetSlotState(Second.SlotHandle), ESmartObjectSlotState::Free);
    });

    It("does not reserve an unreachable claim without a correctly typed destination key", [this]()
    {
        if (!bSetupReady) return;
        auto* Claim = NewObject<UWFClaimSmartObjectAction>(Brain);
        Brain->AddAction(Claim);
        for (const FName Key : {FName(NAME_None), FName(TEXT("Missing")), FName(TEXT("Result"))})
        {
            Claim->ClaimHandleKey = Key;
            ActivateAction(Claim);
            for (const auto Slot : Slots)
                TestEqual(TEXT("No slot leaked"), SmartObjects->GetSlotState(Slot), ESmartObjectSlotState::Free);
        }
    });

    It("cleans an owned claim using its old publication after ownership and key change", [this]()
    {
        if (!bSetupReady) return;
        ActivateAction(MakeUse());
        const auto Claim = ReadClaim(Blackboard);
        TestTrue(TEXT("Use claimed a slot"), Claim.IsValid());
        auto* NewOwner = World->SpawnActor<AActor>();
        auto* NewBoard = AddBlackboard(NewOwner);
        const auto Replacement = SmartObjects->MarkSlotAsClaimed(Slots[Claim.SlotHandle == Slots[0] ? 1 : 0], ESmartObjectClaimPriority::Normal);
        WriteClaim(NewBoard, Replacement);
        Use->ClaimHandleKey = TEXT("OtherClaim");
        Use->SetControlledActor(NewOwner);
        Use->SetAIController(nullptr);
        Use->OnPossessionChanged();
        TestFalse(TEXT("Refresh deactivates old use"), Use->IsActive());
        TestFalse(TEXT("Old publication cleared"), ReadClaim(Blackboard).IsValid());
        TestTrue(TEXT("New owner's claim preserved"), ReadClaim(NewBoard) == Replacement);
        TestEqual(TEXT("Old slot freed"), SmartObjects->GetSlotState(Claim.SlotHandle), ESmartObjectSlotState::Free);
        TestEqual(TEXT("Cache now belongs to new owner"), Cached(Use), static_cast<UObject*>(NewBoard));
    });

    It("preserves a replacement blackboard claim and a borrowed reservation during teardown", [this]()
    {
        if (!bSetupReady) return;
        ActivateAction(MakeUse());
        const auto Owned = ReadClaim(Blackboard);
        const auto Replacement = SmartObjects->MarkSlotAsClaimed(Slots[Owned.SlotHandle == Slots[0] ? 1 : 0], ESmartObjectClaimPriority::Normal);
        WriteClaim(Blackboard, Replacement);
        Use->Finished();
        TestTrue(TEXT("Replacement key preserved"), ReadClaim(Blackboard) == Replacement);
        ActivateAction(Use);
        Use->Finished();
        TestTrue(TEXT("Borrowed key preserved"), ReadClaim(Blackboard) == Replacement);
        TestEqual(TEXT("Borrowed slot remains occupied"), SmartObjects->GetSlotState(Replacement.SlotHandle), ESmartObjectSlotState::Occupied);
    });

    It("enters use when the owned asynchronous move succeeds and finishes after its duration", [this]()
    {
        if (!bSetupReady) return;
        ActivateAction(MakeUse(true));
        auto* Path = Controller->GetPathFollowingComponent();
        TestTrue(TEXT("Asynchronous request started"), Path->GetStatus() != EPathFollowingStatus::Idle);
        TestTrue(TEXT("Action waits for movement"), Use->IsActive());
        const auto Claim = ReadClaim(Blackboard);
        // The engine completion result is authoritative, including overlap-based arrival.
        Path->OnPathFinished(FPathFollowingResult(FPathFollowingResultFlags::Success));
        TestTrue(TEXT("Successful arrival keeps action active"), Use->IsActive());
        TestEqual(TEXT("Arrival occupies slot"), SmartObjects->GetSlotState(Claim.SlotHandle), ESmartObjectSlotState::Occupied);
        Use->TickAction(3.0f);
        TestFalse(TEXT("Use timer finished"), Use->IsActive());
        TestEqual(TEXT("Timer completion releases slot"), SmartObjects->GetSlotState(Claim.SlotHandle), ESmartObjectSlotState::Free);
    });

    It("cancels only its own move and detaches from newer external movement", [this]()
    {
        if (!bSetupReady) return;
        ActivateAction(MakeUse(true));
        auto* Path = Controller->GetPathFollowingComponent();
        Use->Finished();
        TestEqual(TEXT("Owned movement stopped"), Path->GetStatus(), EPathFollowingStatus::Idle);
        ActivateAction(Use);
        TestEqual(TEXT("Replacement request starts"), Controller->MoveToLocation(FVector(1500, 0, 0), 20.0f), EPathFollowingRequestResult::RequestSuccessful);
        const auto ExternalID = Path->GetCurrentRequestId();
        Use->Finished();
        TestTrue(TEXT("New external move still active"), Path->GetStatus() != EPathFollowingStatus::Idle);
        TestTrue(TEXT("New request preserved"), Path->GetCurrentRequestId() == ExternalID);
    });

    It("scores influence searches safely with missing owners or blackboards", [this]()
    {
        if (!bSetupReady) return;
        auto* Influence = NewObject<UWFInfluenceMapConsideration>(Brain);
        Influence->Initialize(Brain, Pawn, Controller);
        Influence->OnBeginPlay();
        auto* GameInstance = NewObject<UGameInstance>();
        auto* Maps = NewObject<UWFInfluenceMapsSubsystem>(GameInstance);
        auto* Map = World->SpawnActor<AWFIntegrationTestInfluenceMap>();
        Map->Width = 4;
        Map->Height = 4;
        Map->InitializeForTest();
        Map->MapName = TEXT("Test");
        Maps->AddMap(Map);
        Influence->MapName = Map->MapName;
        Influence->SearchCondition = ESearchCondition::Equal;
        Influence->SearchValue = 0;
        Influence->SearchRadius = 1;
        Influence->SearchMode = EWFInfluenceMapSearchMode::HighestValue;
        SetCached(Influence, Maps, TEXT("Subsystem"));
        Influence->KeyNameForResult = TEXT("Result");
        TestEqual(TEXT("Controller blackboard receives search"), Influence->GetValue(nullptr), 1.0f);
        TestTrue(TEXT("Result was published"), Blackboard->IsVectorValueSet(TEXT("Result")));
        AActor* NoBoard = World->SpawnActor<AActor>();
        Influence->Initialize(Brain, NoBoard, nullptr);
        TestEqual(TEXT("Missing result blackboard scores zero"), Influence->GetValue(nullptr), 0.0f);
        Influence->KeyNameForResult = NAME_None;
        Influence->SearchCenterMode = ECenterMode::BlackboardKey;
        TestEqual(TEXT("Missing center blackboard scores zero"), Influence->GetValue(nullptr), 0.0f);
        Influence->SearchCenterMode = ECenterMode::ControlledActorLocation;
        TestEqual(TEXT("No-output actor search works without blackboard"), Influence->GetValue(nullptr), 1.0f);
        Influence->Initialize(Brain, nullptr, nullptr);
        TestEqual(TEXT("Missing actor scores zero"), Influence->GetValue(nullptr), 0.0f);
    });
}
#endif
