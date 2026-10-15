// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayGameMode.generated.h"

/*
game mode used for the gameplay level

making sure each local olayer spawns at their correct tagged player start location
*/
UCLASS()
class GROUP2_MULTIPLAYER_API AGameplayGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	// override unreal default spawn selctino so each local player gets the playerstart that matches their player number
	virtual AActor* ChoosePlayerStart_Implementation(	AController* Player	) override;
};