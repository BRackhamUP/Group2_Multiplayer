// Fill out your copyright notice in the Description page of Project Settings.

#include "Multiplayer/GameplayGameMode.h"

#include "Multiplayer/MultiplayerGameInstance.h"

#include "EngineUtils.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"

#include "Kismet/GameplayStatics.h"

// choose correct spawn point for each local player
AActor* AGameplayGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// the incoming controller should be player controller
	APlayerController* PlayerController = Cast<APlayerController>(Player);

	// if it is not fall bacl to unreal normal spawn selection as safety
	if (!PlayerController)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// get the multiplayer session so we know how many players entered the match
	const UMultiplayerGameInstance* MultiplayerGameInstance = Cast<UMultiplayerGameInstance>(GetGameInstance());

	if (!MultiplayerGameInstance)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	int32 PlayerSlot = INDEX_NONE;

	// find this playercontrollers position in unreals local controller list that becomes the index for the player slot
	for (int32 Index = 0; Index < MultiplayerGameInstance->GetSessionPlayerCount(); ++Index)
	{
		APlayerController* IndexedController = UGameplayStatics::GetPlayerController(this, Index);

		if (IndexedController == PlayerController)
		{
			PlayerSlot = Index;
			break;
		}
	}

	// if this controller could not be matched, fallback to unreal default spawn selection
	if (PlayerSlot == INDEX_NONE)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// buildthe player start tag by adding the 1. so Player slot index = 0 + 1 playerStart1
	const FName DesiredPlayerStartTag(*FString::Printf(TEXT("Player%d"), PlayerSlot + 1));

	// search through every PlayerStart placed in the level
	for (TActorIterator<APlayerStart> Iterate(GetWorld()); Iterate; ++Iterate)
	{
		APlayerStart* PlayerStart = *Iterate;

		if (!PlayerStart)
		{
			continue;
		}

		// use the playerStart matching this player's numbered slot
		if (PlayerStart->PlayerStartTag == DesiredPlayerStartTag)
		{
			return PlayerStart;
		}
	}

	// matching tagged PlayerStart was not found, use unreal's default selection
	return Super::ChoosePlayerStart_Implementation(Player);
}