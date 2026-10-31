#include "Multiplayer/ReadyUpGameMode.h"

#include "Multiplayer/MultiplayerGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AReadyUpGameMode::AReadyUpGameMode()
{
	DefaultPawnClass = nullptr;
}

void AReadyUpGameMode::BeginPlay()
{
	Super::BeginPlay();

	UMultiplayerGameInstance* MultiplayerGameInstance = Cast<UMultiplayerGameInstance>(GetGameInstance());

	if (MultiplayerGameInstance)
	{
		MultiplayerGameInstance->ResetSession();
	}

	CheckStartConditions();
	GetWorldTimerManager().SetTimer(StateCheckTimerHandle, this, &AReadyUpGameMode::CheckStartConditions, StateCheckInterval, true);
}

void AReadyUpGameMode::CheckStartConditions()
{
	UMultiplayerGameInstance* MultiplayerGameInstance = Cast<UMultiplayerGameInstance>(GetGameInstance());

	if (!MultiplayerGameInstance)
	{
		return;
	}

	MultiplayerGameInstance->SynchroniseControllers();

	const bool bEnoughPlayers = MultiplayerGameInstance->GetJoinedPlayerCount() >= MinimumPlayers;
	const bool bEveryoneReady = MultiplayerGameInstance->AreAllJoinedPlayersReady();

	if (bEnoughPlayers && bEveryoneReady)
	{
		if (!bCountdownActive)
		{
			StartCountdown();
		}

		return;
	}

	if (bCountdownActive)
	{
		CancelCountdown();
	}
}

void AReadyUpGameMode::StartCountdown()
{
	if (bCountdownActive)
	{
		return;
	}

	bCountdownActive = true;
	CountdownRemaining = CountdownDuration;

	OnCountdownChanged.Broadcast(CountdownRemaining);
	GetWorldTimerManager().SetTimer(CountdownTimerHandle, this, &AReadyUpGameMode::AdvanceCountdown, 1.0f, true);
}

void AReadyUpGameMode::AdvanceCountdown()
{
	if (!bCountdownActive)
	{
		return;
	}

	--CountdownRemaining;

	if (CountdownRemaining <= 0)
	{
		GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
		bCountdownActive = false;
		TravelToGameplayLevel();
		return;
	}

	OnCountdownChanged.Broadcast(CountdownRemaining);
}

void AReadyUpGameMode::CancelCountdown()
{
	if (!bCountdownActive)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);

	bCountdownActive = false;
	CountdownRemaining = 0;

	OnCountdownCancelled.Broadcast();
}

void AReadyUpGameMode::TravelToGameplayLevel()
{
	if (!GameplayLevelName.IsNone())
	{
		UGameplayStatics::OpenLevel(this, GameplayLevelName);
	}
}

bool AReadyUpGameMode::IsCountdownActive() const
{
	return bCountdownActive;
}

int32 AReadyUpGameMode::GetCountdownRemaining() const
{
	return CountdownRemaining;
}