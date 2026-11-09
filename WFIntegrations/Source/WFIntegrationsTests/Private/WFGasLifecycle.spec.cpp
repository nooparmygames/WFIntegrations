// Copyright NoOpArmy 2026
#include "WFGasLifecycleTestTypes.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "WFActivateAbilityAction.h"
#include "WFBrainComponent.h"
#include "BuiltIn/WFTestConsideration.h"
#include "WFCanActivateAbilityConsideration.h"
#include "WFGameplayAttributeConsideration.h"
#include "WFGameplayTagConsideration.h"

#if WITH_DEV_AUTOMATION_TESTS


BEGIN_DEFINE_SPEC(FWFGasLifecycleSpec, "AI.WiseFeline.Integrations.GAS",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
    FGameplayTag ReadyTag;
    UWorld* World = nullptr;
    AAIController* Controller = nullptr;
    UWFBrainComponent* Brain = nullptr;
    AWFGasLifecycleTestPawn* OldPawn = nullptr;
    AWFGasLifecycleTestPawn* NewPawn = nullptr;
    UWFActivateAbilityAction* Action = nullptr;

    UAbilitySystemComponent* AddAbilitySystem(AWFGasLifecycleTestPawn* Pawn, float Energy)
    {
        UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Pawn);
        Pawn->AbilitySystem = ASC;
        ASC->RegisterComponent();
        ASC->InitAbilityActorInfo(Pawn, Pawn);
        UWFGasLifecycleTestAttributes* Attributes = NewObject<UWFGasLifecycleTestAttributes>(Pawn);
        Attributes->Energy.SetBaseValue(Energy);
        Attributes->Energy.SetCurrentValue(Energy);
        ASC->AddAttributeSetSubobject(Attributes);
        return ASC;
    }

    void ActivateAction()
    {
        Brain->Think();
        Brain->TickComponent(0.0f, LEVELTICK_All, nullptr);
    }

    void ChangeActionOwner(AWFGasLifecycleTestPawn* Pawn)
    {
        Action->Finished();
        Controller->UnPossess();
        if (Pawn) { Controller->Possess(Pawn); }
        Brain->ControlledActor = Pawn;
        Action->SetControlledActor(Pawn);
        Action->SetAIController(Controller);
        Action->OnPossessionChanged();
    }
END_DEFINE_SPEC(FWFGasLifecycleSpec)

void FWFGasLifecycleSpec::Define()
{
    BeforeEach([this]()
    {
        // GameplayAbilities registers this runtime tag; editor tests must not declare native tags.
        ReadyTag = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue"));
        const UWorld::InitializationValues Values = UWorld::InitializationValues()
            .AllowAudioPlayback(false).RequiresHitProxies(false).CreateNavigation(false)
            .CreateAISystem(false).ShouldSimulatePhysics(false).CreateFXSystem(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
            ERHIFeatureLevel::Num, &Values);
        check(World);
        OldPawn = World->SpawnActor<AWFGasLifecycleTestPawn>();
        NewPawn = World->SpawnActor<AWFGasLifecycleTestPawn>();
        Controller = World->SpawnActor<AAIController>();
        check(OldPawn && NewPawn && Controller);
        Controller->Possess(OldPawn);
        Brain = NewObject<UWFBrainComponent>(Controller);
        Brain->RegisterComponent();
        Brain->ControlledActor = OldPawn;
        Brain->AIController = Controller;
        Brain->bManualExecution = true;
        AddAbilitySystem(OldPawn, 20.0f);
        AddAbilitySystem(NewPawn, 80.0f);
        Action = NewObject<UWFActivateAbilityAction>(Brain);
        Action->AbilityClass = UWFGasLifecycleTestAbility::StaticClass();
        Brain->AddAction(Action);
        UWFConsideration* Score = NewObject<UWFTestConsideration>(Brain);
        Score->Curve.EditorCurveData.AddKey(0.0f, 1.0f);
        Score->Curve.EditorCurveData.AddKey(1.0f, 1.0f);
        Brain->AddConsideration(Action, Score);
    });

    AfterEach([this]()
    {
        Action->Finished();
        World->DestroyWorld(false);
        World = nullptr;
    });

    It("reads the new pawn's attributes, tags and granted abilities while the old pawn remains alive", [this]()
    {
        UWFGameplayAttributeConsideration* Attribute = NewObject<UWFGameplayAttributeConsideration>(Brain);
        Attribute->Attribute = FGameplayAttribute(FindFProperty<FProperty>(
            UWFGasLifecycleTestAttributes::StaticClass(), GET_MEMBER_NAME_CHECKED(UWFGasLifecycleTestAttributes, Energy)));
        Attribute->MaxValue = 100.0f;
        UWFGameplayTagConsideration* Tag = NewObject<UWFGameplayTagConsideration>(Brain);
        Tag->Tags.AddTag(ReadyTag);
        UWFCanActivateAbilityConsideration* CanActivate = NewObject<UWFCanActivateAbilityConsideration>(Brain);
        CanActivate->AbilityClass = UWFGasLifecycleTestAbility::StaticClass();
        OldPawn->AbilitySystem->AddLooseGameplayTag(ReadyTag);
        OldPawn->AbilitySystem->GiveAbility(FGameplayAbilitySpec(UWFGasLifecycleTestAbility::StaticClass()));
        for (UWFConsideration* Item : TArray<UWFConsideration*>{Attribute, Tag, CanActivate})
        {
            Item->Initialize(Brain, OldPawn, Controller);
            Item->OnBeginPlay();
        }
        TestEqual(TEXT("Old energy"), Attribute->GetValue(nullptr), 0.2f);
        TestEqual(TEXT("Old tag"), Tag->GetValue(nullptr), 1.0f);
        TestEqual(TEXT("Old granted ability"), CanActivate->GetValue(nullptr), 1.0f);
        ChangeActionOwner(NewPawn);
        for (UWFConsideration* Item : TArray<UWFConsideration*>{Attribute, Tag, CanActivate})
        {
            Item->Initialize(Brain, NewPawn, Controller);
            Item->OnPossessionChanged();
        }
        TestTrue(TEXT("Old pawn still alive"), IsValid(OldPawn));
        TestEqual(TEXT("New energy"), Attribute->GetValue(nullptr), 0.8f);
        TestEqual(TEXT("New tag absent"), Tag->GetValue(nullptr), 0.0f);
        TestEqual(TEXT("New ability absent"), CanActivate->GetValue(nullptr), 0.0f);
    });

    It("detaches old completion callbacks and waits only for the new actor's ability", [this]()
    {
        OldPawn->AbilitySystem->GiveAbility(FGameplayAbilitySpec(UWFGasLifecycleTestAbility::StaticClass()));
        NewPawn->AbilitySystem->GiveAbility(FGameplayAbilitySpec(UWFGasLifecycleTestAbility::StaticClass()));
        ActivateAction();
        TestTrue(TEXT("Old ability started"), Action->IsActive());
        UWFGasLifecycleTestAbility* OldAbility = Cast<UWFGasLifecycleTestAbility>(
            OldPawn->AbilitySystem->FindAbilitySpecFromClass(Action->AbilityClass)->GetPrimaryInstance());
        ChangeActionOwner(NewPawn);
        ActivateAction();
        TestTrue(TEXT("New action waits"), Action->IsActive());
        UWFGasLifecycleTestAbility* NewAbility = Cast<UWFGasLifecycleTestAbility>(
            NewPawn->AbilitySystem->FindAbilitySpecFromClass(Action->AbilityClass)->GetPrimaryInstance());
        if (!TestNotNull(TEXT("Old ability instance"), OldAbility)
            || !TestNotNull(TEXT("New ability instance"), NewAbility)) { return; }
        OldAbility->FinishForTest();
        TestTrue(TEXT("Old completion cannot finish the new action"), Action->IsActive());
        NewAbility->FinishForTest();
        TestFalse(TEXT("New completion finishes the action"), Action->IsActive());
    });

    It("ends an active wait when ownership is refreshed directly", [this]()
    {
        OldPawn->AbilitySystem->GiveAbility(FGameplayAbilitySpec(UWFGasLifecycleTestAbility::StaticClass()));
        ActivateAction();
        TestTrue(TEXT("Action initially waiting"), Action->IsActive());
        Action->SetControlledActor(NewPawn);
        Action->OnPossessionChanged();
        TestFalse(TEXT("Owner change ends old wait"), Action->IsActive());
        TestFalse(TEXT("Old callback detached"), OldPawn->AbilitySystem->AbilityEndedCallbacks.IsBoundToObject(Action));
    });

    It("clears missing ownership and finds an ability system added after possession", [this]()
    {
        UWFGameplayAttributeConsideration* Attribute = NewObject<UWFGameplayAttributeConsideration>(Brain);
        Attribute->Attribute = FGameplayAttribute(FindFProperty<FProperty>(
            UWFGasLifecycleTestAttributes::StaticClass(), GET_MEMBER_NAME_CHECKED(UWFGasLifecycleTestAttributes, Energy)));
        Attribute->MaxValue = 100.0f;
        Attribute->Initialize(Brain, OldPawn, Controller);
        Attribute->OnBeginPlay();
        Attribute->Initialize(Brain, nullptr, Controller);
        Attribute->OnPossessionChanged();
        TestEqual(TEXT("Unpossessed score"), Attribute->GetValue(nullptr), 0.0f);
        AWFGasLifecycleTestPawn* LatePawn = World->SpawnActor<AWFGasLifecycleTestPawn>();
        ChangeActionOwner(LatePawn);
        Attribute->Initialize(Brain, LatePawn, Controller);
        Attribute->OnPossessionChanged();
        TestEqual(TEXT("Missing component score"), Attribute->GetValue(nullptr), 0.0f);
        ActivateAction();
        TestFalse(TEXT("No old ability activation"), Action->IsActive());
        AddAbilitySystem(LatePawn, 50.0f)->GiveAbility(
            FGameplayAbilitySpec(UWFGasLifecycleTestAbility::StaticClass()));
        TestEqual(TEXT("Late component score"), Attribute->GetValue(nullptr), 0.5f);
        ActivateAction();
        TestTrue(TEXT("Late component can activate"), Action->IsActive());
    });
}
#endif
