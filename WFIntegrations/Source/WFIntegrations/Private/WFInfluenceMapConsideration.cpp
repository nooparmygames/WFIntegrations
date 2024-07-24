// Copyright NoOpArmy 2024


#include "WFInfluenceMapConsideration.h"
#include "WFInfluenceMapsSubsystem.h"
#include "WFInfluenceMap.h"
#include "AIModule/Classes/BehaviorTree/BlackboardComponent.h"

float UWFInfluenceMapConsideration::GetValue_Implementation(const AActor* InTargetActor)
{
	if (Subsystem != nullptr)
	{
		AWFInfluenceMap* Map = Subsystem->GetMap(MapName);
		if (Map != nullptr)
		{
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
			if (!bUseInterestSearch)
			{
				if (Map->SearchReturningWorldPosition(SearchValue
					, SearchCondition
					, Center
					, SearchRadius
					, Result))
				{
					if (KeyNameForResult != NAME_None)
					{
						check(Blackboard != nullptr);
						Blackboard->SetValueAsVector(KeyNameForResult, Result);
					}
					return 1;
				}
				else
				{
					return 0;
				}
			}
			else //interest based search
			{
				if (Map->SearchWithInterestReturningWorldPosition(SearchValue
					, SearchCondition
					, Center
					, SearchRadius
					, InterestTemplate
					, InterestMagnitude
					, SelfTemplate
					, SelfMagnitude
					, Result))
				{
					if (KeyNameForResult != NAME_None)
					{
						check(Blackboard != nullptr);
						Blackboard->SetValueAsVector(KeyNameForResult, Result);
					}
					return 1;
				}
				else
				{
					return 0;
				}
			}
		}
		else//Map name is wrong and Map is nullptr
		{
			return 0;
		}
	}
	//For some reason WFInfluenceMaps subsystem is not available. should not happen
	return 0;
}

void UWFInfluenceMapConsideration::OnBeginPlay_Implementation()
{
	Subsystem = UWFInfluenceMapsSubsystem::Get(GetWorld());
	Blackboard = GetControlledActor()->FindComponentByClass<UBlackboardComponent>();
}
