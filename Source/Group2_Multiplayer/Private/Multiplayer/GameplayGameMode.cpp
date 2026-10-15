// Fill out your copyright notice in the Description page of Project Settings.

#include "Multiplayer/GameplayGameMode.h"

#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"

// choose correct spawn point for each local player
AActor* AGameplayGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// the incoming controller should be player controller
	APlayerController* PlayerController = Cast<APlayerController>(Player);

	// if it is not then fall bacl to unreal normal spawn selection as safety
	if (!PlayerController)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// get the localplayer that owns this controller
	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();

	// if no localplayer exists, fall back to unreal normal selection
	if (!LocalPlayer)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// local player indexes start from 0... obivously
	const int32 LocalPlayerIndex = LocalPlayer->GetLocalPlayerIndex();

	// build the playerstart tag we want to look for, index 0 = player1
	const FName DesiredPlayerStartTag(*FString::Printf(TEXT("Player%d"), LocalPlayerIndex + 1));

	// search through every playerstart placed in the level
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* PlayerStart = *It;

		if (!PlayerStart)
		{
			continue;
		}

		// if htis playerstart has the correct player start tag then use it
		if (PlayerStart->PlayerStartTag == DesiredPlayerStartTag)
		{
			return PlayerStart;
		}
	}

	// if no mathing tag playerstart was found the fall back to unreal default spawn selector
	return Super::ChoosePlayerStart_Implementation(Player);
}