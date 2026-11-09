// Copyright NoOpArmy 2024


#include "WFSmartObjectConsideration.h"
#include "WFUtilities.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectRequestTypes.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Actor.h"

float UWFSmartObjectConsideration::GetValue_Implementation(const AActor* InTargetActor)
{
	// Blackboard setup can finish after possession. Resolve it again before use.
	Blackboard = WFUtilities::FindBlackboardComponent(GetControlledActor());

	const AActor* Self = GetControlledActor();
	if (!IsValid(Subsystem) || !IsValid(Self) || SearchRadius <= 0.0f)
		return 0.0f;

	const FVector Center = Self->GetActorLocation();

	FSmartObjectRequestFilter Filter;
	Filter.BehaviorDefinitionClasses = BehaviorDefinitionClasses;
	Filter.ActivityRequirements = ActivityRequirements;
	Filter.UserTags = UserTags;
	Filter.ClaimPriority = ClaimPriority;

	const FSmartObjectRequest Request(FBox::BuildAABB(Center, FVector(SearchRadius)), Filter);
	const FSmartObjectRequestResult Result = Subsystem->FindSmartObject(Request, Self);
	if (!Result.IsValid())
		return 0.0f;

	const TOptional<FVector> SlotLocation = Subsystem->GetSlotLocation(Result);

	// Publish the location so a follow-up MoveTo action can head there (mirrors the influence map consideration).
	if (ResultLocationKey != NAME_None && IsValid(Blackboard) && SlotLocation.IsSet())
		Blackboard->SetValueAsVector(ResultLocationKey, SlotLocation.GetValue());

	if (!bScoreByDistance || !SlotLocation.IsSet())
		return 1.0f;

	const float Distance = FVector::Distance(Center, SlotLocation.GetValue());
	return FMath::Clamp(1.0f - (Distance / SearchRadius), 0.0f, 1.0f);
}

void UWFSmartObjectConsideration::OnBeginPlay_Implementation()
{
	Super::OnBeginPlay_Implementation();
	Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	Blackboard = WFUtilities::FindBlackboardComponent(GetControlledActor());
}

void UWFSmartObjectConsideration::OnPossessionChanged_Implementation()
{
	Super::OnPossessionChanged_Implementation();
	Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
	Blackboard = WFUtilities::FindBlackboardComponent(GetControlledActor());
}
