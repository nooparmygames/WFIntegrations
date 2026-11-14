/* Copyright (c) 2023 NoOp Army. All rights reserved. */

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "WFInfluenceMap.h"
#include "WFInfluenceRoutePrototype.h"

BEGIN_DEFINE_SPEC(FWFInfluenceRoutePrototypeSpec, "AI.WiseFeline.Integrations.RoutePrototype",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	UWorld* World = nullptr;
	AWFInfluenceMap* Map = nullptr;
END_DEFINE_SPEC(FWFInfluenceRoutePrototypeSpec)

void FWFInfluenceRoutePrototypeSpec::Define()
{
	using namespace WFInfluenceRoutePrototype;
	BeforeEach([this]()
	{
		const UWorld::InitializationValues Values = UWorld::InitializationValues()
			.AllowAudioPlayback(false).RequiresHitProxies(false).CreateNavigation(false)
			.CreateAISystem(false).ShouldSimulatePhysics(false).CreateFXSystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		World->InitializeActorsForPlay(FURL());
		Map = World->SpawnActorDeferred<AWFInfluenceMap>(AWFInfluenceMap::StaticClass(),
			FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Map->Width = 4;
		Map->Height = 3;
		Map->FinishSpawning(FTransform::Identity);
	});
	AfterEach([this]() { World->DestroyWorld(false); });

	It("measures a known step field and can prefer a longer low exposure route", [this]()
	{
		Map->SetCellValue(1, 0, 8.0f);
		Map->SetCellValue(2, 0, 8.0f);
		const TArray<FVector> Direct{FVector(50, 50, 0), FVector(350, 50, 0)};
		const TArray<FVector> Bypass{FVector(50, 50, 0), FVector(50, 150, 0), FVector(350, 150, 0), FVector(350, 50, 0)};
		const FAssessment A = Measure(Map, Direct, 25.0, 9.0f);
		const FAssessment B = Measure(Map, Bypass, 25.0, 9.0f);
		TestEqual("Direct length", A.Length, 300.0);
		TestEqual("Direct exposure is 200 cm times 8", A.Integral, 1600.0);
		TestEqual("Peak threat", A.Peak, 8.0f);
		TestEqual("Bypass length", B.Length, 500.0);
		TestEqual("Bypass is outside the high cells", B.Integral, 0.0);
		const TArray<FAssessment> Routes{A, B};
		TestEqual("Length alone favors the direct route", PickRoute(Routes, 1.0, 0.0, 1.0), 0);
		TestEqual("Exposure can favor the bypass", PickRoute(Routes, 1.0, 0.2, 1.0), 1);
		const TArray<FAssessment> Veto{Measure(Map, Direct, 25.0, 5.0f), B};
		TestEqual("Peak threshold vetoes the direct route", PickRoute(Veto, 1.0, 0.0, 1.0), 1);
	});

	It("weights by distance, reports out of map coverage and handles a stationary point", [this]()
	{
		Map->SetMap(TArray<float>({-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2}));
		const TArray<FVector> Outside{FVector(-50, 50, 0), FVector(150, 50, 0)};
		const auto Result = Measure(Map, Outside, 25.0, 0.0f);
		TestEqual("Out of map coverage", Result.OutOfMapLength, 50.0);
		TestEqual("Covered length", Result.CoveredLength, 150.0);
		TestEqual("Signed integral", Result.Integral, -300.0);
		TestEqual("Mean over covered length", Result.Mean, -2.0);
		TestEqual("Negative-only field keeps a negative peak", Result.Peak, -2.0f);
		const TArray<FAssessment> Routes{Result};
		TestEqual("Can reject incomplete map coverage", PickRoute(Routes, 1.0, 1.0, 1.0), INDEX_NONE);
		TestEqual("Can accept a configured coverage limit", PickRoute(Routes, 1.0, 1.0, 0.75), 0);
		const TArray<FVector> Subdivided{FVector(-50, 50, 0), FVector(50, 50, 0), FVector(150, 50, 0)};
		TestEqual("Adding a vertex on this constant field does not change the result", Measure(Map, Subdivided, 25.0, 0.0f).Integral,
			Result.Integral);
		const TArray<FVector> Stationary{FVector(25, 25, 0), FVector(25, 25, 0)};
		const auto Point = Measure(Map, Stationary, 100.0, -3.0f);
		TestEqual("Stationary result still samples its point", Point.Samples, 1);
		TestEqual("Stationary point has zero integral", Point.Integral, 0.0);
		TestTrue("Stationary point can exceed threshold", Point.bThresholdExceeded);
	});

	It("shows coarse sampling can miss a narrow hazard and refuses over-budget paths", [this]()
	{
		const TArray<FVector> Route{FVector(50, 50, 0), FVector(350, 50, 0)};
		const auto Before = Measure(Map, Route, 25.0, 5.0f);
		Map->SetCellValue(1, 0, 10.0f);
		TestFalse("Coarse midpoint samples can miss a cell", Measure(Map, Route, 400.0, 5.0f).bThresholdExceeded);
		TestTrue("A finer interval finds that cell", Measure(Map, Route, 25.0, 5.0f).bThresholdExceeded);
		TestFalse("Old assessment is a snapshot of the old field", Before.bThresholdExceeded);
		const auto OverBudget = Measure(Map, Route, 0.001, 5.0f);
		TestEqual("Rejects over-budget request", OverBudget.Status, EStatus::SampleBudgetExceeded);
		TestEqual("Does not return a partial measurement", OverBudget.Samples, 0);
	});
}
#endif
