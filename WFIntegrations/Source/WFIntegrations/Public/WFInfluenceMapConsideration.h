// Copyright NoOpArmy 2024

#pragma once

#include "CoreMinimal.h"
#include "WFConsideration.h"
#include "WFInfluenceMap.h"
#include "UObject/NameTypes.h"
#include "WFInfluenceMapConsideration.generated.h"

/**
 * Specifies how the search's center should be chosen
 */
UENUM(BlueprintType)
enum class ECenterMode : uint8
{
	ControlledActorLocation,
	BlackboardKey,
};

/**
 * Specifies which cell a successful search returns
 */
UENUM(BlueprintType)
enum class EWFInfluenceMapSearchMode : uint8
{
	/** The first cell that meets the condition, starting from a random cell of the area. Cheapest, but the cell is rarely the best or the closest one. */
	FirstMatch,
	/** The cell with the highest value in the area, if that value meets the condition. Among equal cells the one closest to the search center wins. */
	HighestValue,
	/** The cell with the lowest value in the area, if that value meets the condition. Among equal cells the one closest to the search center wins. */
	LowestValue,
};

/**
 * Searches the influence map for a value and if successful, puts the result in a blackboard key
 */
UCLASS()
class WFINTEGRATIONS_API UWFInfluenceMapConsideration : public UWFConsideration
{

	GENERATED_BODY()

public:

	virtual void OnBeginPlay_Implementation() override;
	float GetValue_Implementation(const AActor* InTargetActor) override;

public:

	/** Name of the map to search for */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline")
	FName MapName;

	/**
	 * @brief The value to search for
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline")
	float SearchValue = 1;

	/** The condition of the search */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline")
	ESearchCondition SearchCondition;

	/**
	 * Which of the cells meeting the condition to return. HighestValue finds the peak of the influence,
	 * which is where the actor that stamped it stands, instead of any cell inside its radius.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline")
	EWFInfluenceMapSearchMode SearchMode = EWFInfluenceMapSearchMode::FirstMatch;

	/** Radius of the search to conduct */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline")
	int32 SearchRadius = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline")
	ECenterMode SearchCenterMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline", meta = (EditCondition = "SearchCenterMode == ECenterMode::BlackboardKey", EditConditionHides))
	FName SearchCenterKey;

	/**
	 * @brief Should we use search methods with interest or the simple search functions?
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline")
	bool bUseInterestSearch = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline", meta = (EditCondition = "bUseInterestSearch ", EditConditionHides))
	class UWFInfluenceMapTemplate* InterestTemplate;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline", meta = (EditCondition = "bUseInterestSearch ", EditConditionHides))
	float InterestMagnitude = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline", meta = (EditCondition = "bUseInterestSearch ", EditConditionHides))
	class UWFInfluenceMapTemplate* SelfTemplate;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline", meta = (EditCondition = "bUseInterestSearch ", EditConditionHides))
	float SelfMagnitude = 1;

	/** If the search is successful, the result is stored in this key on the blackboard for the action to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WiseFeline")
	FName KeyNameForResult;

protected:
	UPROPERTY(Transient)
	class UWFInfluenceMapsSubsystem* Subsystem;
	UPROPERTY(Transient)
	class UBlackboardComponent* Blackboard;
};
