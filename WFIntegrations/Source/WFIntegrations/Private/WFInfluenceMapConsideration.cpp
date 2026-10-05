// Copyright NoOpArmy 2024


#include "WFInfluenceMapConsideration.h"
#include "WFInfluenceMapsSubsystem.h"
#include "WFInfluenceMap.h"
#include "BehaviorTree/BlackboardComponent.h"

namespace
{
	bool MeetsCondition(float Value, ESearchCondition Condition, float SearchValue)
	{
		switch (Condition)
		{
		case ESearchCondition::Equal:          return Value == SearchValue;
		case ESearchCondition::NotEqual:       return Value != SearchValue;
		case ESearchCondition::Greater:        return Value > SearchValue;
		case ESearchCondition::GreaterOrEqual: return Value >= SearchValue;
		case ESearchCondition::Less:           return Value < SearchValue;
		case ESearchCondition::LessOrEqual:    return Value <= SearchValue;
		}
		return false;
	}
}

float UWFInfluenceMapConsideration::GetValue_Implementation(const AActor* InTargetActor)
{
	//For some reason WFInfluenceMaps subsystem is not available. should not happen
	if (Subsystem == nullptr)
	{
		return 0;
	}

	AWFInfluenceMap* Map = Subsystem->GetMap(MapName);
	if (Map == nullptr)//Map name is wrong
	{
		return 0;
	}

	FVector Result;
	FVector Center;
	if (SearchCenterMode == ECenterMode::ControlledActorLocation)
	{
		Center = GetControlledActor()->GetActorLocation();
	}
	else
	{
		check(Blackboard != nullptr);
		Center = Blackboard->GetValueAsVector(SearchCenterKey);
	}

	bool bFound = false;
	if (SearchMode == EWFInfluenceMapSearchMode::FirstMatch)
	{
		if (!bUseInterestSearch)
		{
			bFound = Map->SearchReturningWorldPosition(SearchValue
				, SearchCondition
				, Center
				, SearchRadius
				, Result);
		}
		else //interest based search
		{
			bFound = Map->SearchWithInterestReturningWorldPosition(SearchValue
				, SearchCondition
				, Center
				, SearchRadius
				, InterestTemplate
				, InterestMagnitude
				, SelfTemplate
				, SelfMagnitude
				, Result);
		}
	}
	else
	{
		// Find the best cell of the area, the one closest to the search center when several share
		// the best value, and then test that cell's value against the condition.
		const bool bHighest = SearchMode == EWFInfluenceMapSearchMode::HighestValue;
		float Value;
		if (!bUseInterestSearch)
		{
			Value = bHighest
				? Map->SearchForHighestValueClosestToPointReturningWorldPosition(Center, SearchRadius, Center, Result)
				: Map->SearchForLowestValueClosestToPointReturningWorldPosition(Center, SearchRadius, Center, Result);
		}
		else //interest based search
		{
			Value = bHighest
				? Map->SearchWithInterestForHighestValueClosestToPointReturningWorldPosition(Center, SearchRadius, Center, InterestTemplate, InterestMagnitude, SelfTemplate, SelfMagnitude, Result)
				: Map->SearchWithInterestForLowestValueClosestToPointReturningWorldPosition(Center, SearchRadius, Center, InterestTemplate, InterestMagnitude, SelfTemplate, SelfMagnitude, Result);
		}

		// The searches return these sentinels when the area is entirely outside the map.
		const bool bSearchedAnyCell = Value > TNumericLimits<float>::Lowest() && Value < TNumericLimits<float>::Max();
		bFound = bSearchedAnyCell && MeetsCondition(Value, SearchCondition, SearchValue);
	}

	if (!bFound)
	{
		return 0;
	}

	if (KeyNameForResult != NAME_None)
	{
		check(Blackboard != nullptr);
		Blackboard->SetValueAsVector(KeyNameForResult, Result);
	}
	return 1;
}

void UWFInfluenceMapConsideration::OnBeginPlay_Implementation()
{
	Subsystem = UWFInfluenceMapsSubsystem::Get(GetWorld());
	Blackboard = GetControlledActor()->FindComponentByClass<UBlackboardComponent>();
}
