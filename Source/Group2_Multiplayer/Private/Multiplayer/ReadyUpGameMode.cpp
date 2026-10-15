// Fill out your copyright notice in the Description page of Project Settings.

#include "Multiplayer/ReadyUpGameMode.h"

#include "Multiplayer/MultiplayerGameInstance.h"

#include "Kismet/GameplayStatics.h"

#include "TimerManager.h"

AReadyUpGameMode::AReadyUpGameMode()
{
	// the lobby does not need gameplay pawns, players interact through their ready up player controller and lobby UI
	DefaultPawnClass = nullptr;
}

void AReadyUpGameMode::BeginPlay()
{
	Super::BeginPlay();

	// get shared multiplayer session data
	UMultiplayerGameInstance* MultiplayerGameInstance = Cast<UMultiplayerGameInstance>(GetGameInstance());

	if (MultiplayerGameInstance)
	{
		// reset ready/cosmetic state when entering the lobby
		MultiplayerGameInstance->ResetSession();
	}
	// repeatedly check the lobby, delay the first check because secondary players are created dynamically
	GetWorldTimerManager().SetTimer(LobbyRefreshTimerHandle, this, &AReadyUpGameMode::RefreshLobbyState, LobbyRefreshInterval, true, LobbyRefreshInterval);
}

// checking the lobby state
void AReadyUpGameMode::RefreshLobbyState()
{
	UMultiplayerGameInstance* MultiplayerGameInstance = Cast<UMultiplayerGameInstance>(GetGameInstance());

	if (!MultiplayerGameInstance)
	{
		return;
	}

	// update controller informaiton if anything has conneced/disconnected
	MultiplayerGameInstance->SynchroniseLobbyPlayers();

	// must have at least the min num of players
	const bool bEnoughPlayers = MultiplayerGameInstance->GetSessionPlayerCount() >= MinimumPlayers;

	// every player curently in the session must also be ready
	const bool bEveryoneReady = MultiplayerGameInstance->AreAllSessionPlayersReady();

	// both conditions must be true before the match can start
	const bool bCanStart = bEnoughPlayers && bEveryoneReady;

	if (bCanStart)
	{
		// only start a new countdown if one is not already running
		if (!bCountdownActive)
		{
			StartCountdown();
		}
	}
	else
	{
		// if something changes while counting down, cancel it
		if (bCountdownActive)
		{
			CancelCountdown();
		}
	}
}

// start countdown
void AReadyUpGameMode::StartCountdown()
{
	// prevent accidently creating multiple countdown timers
	if (bCountdownActive)
	{
		return;
	}

	bCountdownActive = true;

	// start from the configured countdown value
	CountdownRemaining = CountdownDuration;

	// immediately tell the UI to display the first num
	OnCountdownChanged.Broadcast(CountdownRemaining);

	// call advanceCountdown once every second
	GetWorldTimerManager().SetTimer(CountdownTimerHandle, this, &AReadyUpGameMode::AdvanceCountdown, 1.0f, true);
}

// countdown tick
void AReadyUpGameMode::AdvanceCountdown()
{
	if (!bCountdownActive)
	{
		return;
	}

	// reduce countdown by one each tie the timer fires
	--CountdownRemaining;

	// when countdown reaches zero, stop the timer and enter gameplay
	if (CountdownRemaining <= 0)
	{
		GetWorldTimerManager().ClearTimer(CountdownTimerHandle);

		bCountdownActive = false;

		TravelToGameplayLevel();

		return;
	}
	// update the lobby idget with the new number
	OnCountdownChanged.Broadcast(CountdownRemaining);
}

// cancel countdown
void AReadyUpGameMode::CancelCountdown()
{
	if (!bCountdownActive)
	{
		return;
	}

	// stop the one second countdown timer
	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);

	bCountdownActive = false;

	CountdownRemaining = 0;

	// tell the widget to hide/reset the countdown display
	OnCountdownCancelled.Broadcast();
}

// level travel
void AReadyUpGameMode::TravelToGameplayLevel()
{
	// do nothing if a gameplay level has not been assigned in the BP_GM_ReadyUp
	if (GameplayLevelName.IsNone())
	{
		return;
	}

	// open the gameplay map, GameInstance survives this level change
	UGameplayStatics::OpenLevel(this, GameplayLevelName);
}

// blueprint getters

bool AReadyUpGameMode::IsCountdownActive() const
{
	return bCountdownActive;
}

int32 AReadyUpGameMode::GetCountdownRemaining() const
{
	return CountdownRemaining;
}