// Fill out your copyright notice in the Description page of Project Settings.

#include "Multiplayer/GameplayGameMode.h"

#include "Multiplayer/MultiplayerGameInstance.h"

#include "EngineUtils.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"

// choose correct spawn point for each session player
AActor* AGameplayGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// the incoming controller should be a player controller
	APlayerController* PlayerController = Cast<APlayerController>(Player);

	// if it is not, fall back to unreal's normal spawn selection
	if (!PlayerController)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// get the multiplayer session
	const UMultiplayerGameInstance* MultiplayerGameInstance =
		Cast<UMultiplayerGameInstance>(GetGameInstance());

	if (!MultiplayerGameInstance)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// get the actual current session slot owned by this controller
	const int32 PlayerSlot =
		MultiplayerGameInstance->GetPlayerNumberForController(PlayerController);

	// if this controller is not part of the current session, use unreal's default spawn selection
	if (PlayerSlot == INDEX_NONE)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// player slot 0 uses Player1, player slot 1 uses Player2 etc.
	const FName DesiredPlayerStartTag(
		*FString::Printf(TEXT("Player%d"), PlayerSlot + 1));

	// search through every PlayerStart placed in the level
	for (TActorIterator<APlayerStart> Iterate(GetWorld()); Iterate; ++Iterate)
	{
		APlayerStart* PlayerStart = *Iterate;

		if (!PlayerStart)
		{
			continue;
		}

		// use the PlayerStart matching this player's current session slot
		if (PlayerStart->PlayerStartTag == DesiredPlayerStartTag)
		{
			return PlayerStart;
		}
	}

	// matching tagged PlayerStart was not found
	return Super::ChoosePlayerStart_Implementation(Player);
}