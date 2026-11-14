/* Copyright (c) 2023 NoOp Army. All rights reserved. */

#include "WFInfluenceRoutePrototype.h"

#include "WFInfluenceMap.h"

namespace WFInfluenceRoutePrototype
{
FAssessment Measure(const AWFInfluenceMap* Map, TConstArrayView<FVector> Points, double IntervalCm, float Threshold)
{
	FAssessment Result;
	if (!IsValid(Map) || Map->IsActorBeingDestroyed() || Points.IsEmpty() || !FMath::IsFinite(IntervalCm)
		|| IntervalCm <= 0.0 || !FMath::IsFinite(Threshold) || !Map->ValidateMapLayout(Map).bCompatible)
	{
		return Result;
	}
	if (Points.Num() > MaxSamples)
	{
		Result.Status = EStatus::SampleBudgetExceeded;
		return Result;
	}
	for (const FVector& Point : Points)
	{
		if (Point.ContainsNaN())
		{
			return Result;
		}
	}

	// Preflight the complete route so a budget failure never returns partial exposure.
	TArray<int32, TInlineAllocator<32>> Parts;
	Parts.Reserve(Points.Num() - 1);
	int32 Remaining = MaxSamples - 1;
	double Length = 0.0;
	for (int32 Segment = 1; Segment < Points.Num(); ++Segment)
	{
		const double SegmentLength = FVector::Distance(Points[Segment - 1], Points[Segment]);
		Length += SegmentLength;
		if (!FMath::IsFinite(Length))
		{
			return Result;
		}
		if (SegmentLength == 0.0)
		{
			Parts.Add(0);
			continue;
		}
		const double PartCount = FMath::Max(1.0, FMath::CeilToDouble(SegmentLength / IntervalCm));
		if (PartCount > Remaining - 1)
		{
			Result.Status = EStatus::SampleBudgetExceeded;
			return Result;
		}
		const int32 Count = static_cast<int32>(PartCount);
		Parts.Add(Count);
		Remaining -= Count + 1;
	}
	Result.Length = Length;

	const FWFInfluenceMapLayout Layout = Map->GetGridLayout();
	const auto Read = [&](const FVector& Position, double Distance)
	{
		++Result.Samples;
		const double X = (Position.X - Layout.AnchorXY.X) / Layout.CellSize;
		const double Y = (Position.Y - Layout.AnchorXY.Y) / Layout.CellSize;
		if (!(X >= 0 && X < Layout.Width && Y >= 0 && Y < Layout.Height))
		{
			Result.OutOfMapLength += Distance;
			return;
		}
		const float Value = Map->GetCellValue(FMath::FloorToInt(X), FMath::FloorToInt(Y));
		if (!FMath::IsFinite(Value))
		{
			Result.NonfiniteLength += Distance;
			return;
		}
		Result.Peak = Result.bHasFiniteSample ? FMath::Max(Result.Peak, Value) : Value;
		Result.bHasFiniteSample = true;
		Result.bThresholdExceeded |= Value > Threshold;
		Result.CoveredLength += Distance;
		Result.Integral += Distance * Value;
	};

	Read(Points[0], 0.0);
	for (int32 Segment = 1; Segment < Points.Num(); ++Segment)
	{
		const int32 Count = Parts[Segment - 1];
		if (Count == 0)
		{
			continue;
		}
		const FVector& Start = Points[Segment - 1];
		const FVector& End = Points[Segment];
		const double Distance = FVector::Distance(Start, End) / Count;
		for (int32 Part = 0; Part < Count; ++Part)
		{
			Read(FMath::Lerp(Start, End, (Part + 0.5) / Count), Distance);
		}
		Read(End, 0.0);
	}

	if (Result.CoveredLength > 0.0)
	{
		Result.Status = EStatus::Measured;
		Result.Mean = Result.Integral / Result.CoveredLength;
	}
	else if (Result.Length == 0.0 && Result.bHasFiniteSample)
	{
		Result.Status = EStatus::Measured;
		Result.Mean = Result.Peak;
	}
	else
	{
		Result.Status = EStatus::NoCoverage;
	}
	return Result;
}

int32 PickRoute(TConstArrayView<FAssessment> Routes, double LengthWeight, double ExposureWeight, double MinimumCoverage)
{
	if (Routes.Num() > MaxRoutes || !FMath::IsFinite(LengthWeight) || LengthWeight < 0.0
		|| !FMath::IsFinite(ExposureWeight) || ExposureWeight < 0.0
		|| !(MinimumCoverage >= 0.0 && MinimumCoverage <= 1.0))
	{
		return INDEX_NONE;
	}
	int32 Best = INDEX_NONE;
	double BestCost = 0.0;
	for (int32 Index = 0; Index < Routes.Num(); ++Index)
	{
		const FAssessment& Route = Routes[Index];
		if (Route.Status != EStatus::Measured || Route.bThresholdExceeded)
		{
			continue;
		}
		// Measured pieces partition the length. At full coverage, tiny roundoff must not veto a route.
		const double Coverage = Route.Length > 0.0 && (Route.OutOfMapLength > 0.0 || Route.NonfiniteLength > 0.0)
			? Route.CoveredLength / Route.Length : 1.0;
		const double Cost = LengthWeight * Route.Length + ExposureWeight * Route.Integral;
		if (Coverage >= MinimumCoverage && FMath::IsFinite(Cost) && (Best == INDEX_NONE || Cost < BestCost))
		{
			Best = Index;
			BestCost = Cost;
		}
	}
	return Best;
}
}
