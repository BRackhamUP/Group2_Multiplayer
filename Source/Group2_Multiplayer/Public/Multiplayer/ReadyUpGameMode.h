#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Delegates/DelegateCombinations.h"
#include "ReadyUpGameMode.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCustomisationCountdownChanged, int32, SecondsRemaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCustomisationCountdownCancelled);

UCLASS()
class GROUP2_MULTIPLAYER_API AReadyUpGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AReadyUpGameMode();

	UPROPERTY(BlueprintAssignable, Category = "Customisation|Events")
	FOnCustomisationCountdownChanged OnCountdownChanged;

	UPROPERTY(BlueprintAssignable, Category = "Customisation|Events")
	FOnCustomisationCountdownCancelled OnCountdownCancelled;

	UFUNCTION(BlueprintPure, Category = "Customisation")
	bool IsCountdownActive() const;

	UFUNCTION(BlueprintPure, Category = "Customisation")
	int32 GetCountdownRemaining() const;

protected:
	virtual void BeginPlay() override;

	// Minimum players required to start.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customisation", meta = (ClampMin = "2", ClampMax = "4"))
	int32 MinimumPlayers = 2;

	// Countdown before gameplay starts.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customisation", meta = (ClampMin = "1"))
	int32 CountdownDuration = 3;

	// How often controller and ready states are checked.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customisation", meta = (ClampMin = "0.1"))
	float StateCheckInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customisation")
	FName GameplayLevelName;

private:
	void CheckStartConditions();
	void StartCountdown();
	void AdvanceCountdown();
	void CancelCountdown();
	void TravelToGameplayLevel();

	bool bCountdownActive = false;
	int32 CountdownRemaining = 0;

	FTimerHandle StateCheckTimerHandle;
	FTimerHandle CountdownTimerHandle;
};