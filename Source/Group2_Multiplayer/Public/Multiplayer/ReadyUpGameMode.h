// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ReadyUpGameMode.generated.h"

// event for updating the countown UI, passes the remaining seconds to the widget
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLobbyCountdownChanged, int32, SecondsRemaining);

// event for telling UI the countdown has been cancelled
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLobbyCountdownCancelled);

/*
game mode used only for the ready up lobby

the gane instance stores the actual player information
this class andles the rules for starting the match
minimum number of player, checking if every connected player is ready, starting/cancelling the ocuntdown, travelling to gameplay level
*/
UCLASS()
class GROUP2_MULTIPLAYER_API AReadyUpGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	AReadyUpGameMode();

	// widget can bind to this to update countdown text
	UPROPERTY(BlueprintAssignable, Category = "Ready Up|Events")
	FOnLobbyCountdownChanged OnCountdownChanged;

	// widget can bind to this hide/reset the countdown if it gets cancelled
	UPROPERTY(BlueprintAssignable, Category = "Ready Up|Events")
	FOnLobbyCountdownCancelled OnCountdownCancelled;

	// return whether the countdown is current runnning
	UFUNCTION(BlueprintPure, Category = "Ready Up")
	bool IsCountdownActive() const;

	// returns the current number shown on the countdown
	UFUNCTION(BlueprintPure, Category = "Ready Up")
	int32 GetCountdownRemaining() const;

protected:

	virtual void BeginPlay() override;

	// min number of players required before the game is allowed to start
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ready Up", meta = (ClampMin = "2", ClampMax = "4"))
	int32 MinimumPlayers = 2;

	// number of second used for the ready up countdown
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ready Up", meta = (ClampMin = "1"))
	int32 CountdownDuration = 3;

	// how often the lobby checks for controller/player/ready states chanigng
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ready Up", meta = (ClampMin = "0.1"))
	float LobbyRefreshInterval = 0.25f;

	// name of the gameplay levle to open when the countdown finishes
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ready Up")
	FName GameplayLevelName;

private:

	// check connected players, ready state and decide whehtehr countdown should start or cancle
	void RefreshLobbyState();

	// start the countdon if its not already running
	void StartCountdown();

	// reduce countdown by one second and travel when it reaches zero
	void AdvanceCountdown();

	// stop/reset the countdown if the lobby i no longer ready
	void CancelCountdown();

	// open gameplay level after the countdown finishes
	void TravelToGameplayLevel();

	// tracks whether the countdown timer is curent ly active
	bool bCountdownActive = false;

	// current countdown value
	int32 CountdownRemaining = 0;

	// repeating timer used to keep checking the lobby state
	FTimerHandle LobbyRefreshTimerHandle;

	// repeating one second timer used for the actual countdown
	FTimerHandle CountdownTimerHandle;
};