/* Copyright (c) 2023 NoOp Army. All rights reserved. */

#pragma once

#include "CoreMinimal.h"

class AWFInfluenceMap;

namespace WFInfluenceRoutePrototype
{
constexpr int32 MaxSamples = 8192;
constexpr int32 MaxRoutes = 4;

enum class EStatus : uint8
{
	InvalidInput,
	SampleBudgetExceeded,
	NoCoverage,
	Measured
};

struct FAssessment
{
	EStatus Status = EStatus::InvalidInput;
	double Length = 0.0;
	double CoveredLength = 0.0;
	double OutOfMapLength = 0.0;
	double NonfiniteLength = 0.0;
	double Integral = 0.0;
	double Mean = 0.0;
	float Peak = 0.0f;
	int32 Samples = 0;
	bool bHasFiniteSample = false;
	bool bThresholdExceeded = false;
};

/** Prototype: each subsegment contributes its midpoint value times its 3D length. Vertices also count for peak. */
FAssessment Measure(const AWFInfluenceMap* Map, TConstArrayView<FVector> Points, double IntervalCm, float Threshold);

/** Compares at most four complete routes supplied by the caller. Equal costs keep the earlier route. */
int32 PickRoute(TConstArrayView<FAssessment> Routes, double LengthWeight, double ExposureWeight, double MinimumCoverage);
}
