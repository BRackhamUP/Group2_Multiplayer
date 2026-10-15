// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ReadyUpPlayerController.generated.h"

// forward decs
class UInputAction;
class UInputMappingContext;

/*
this player controller is used specifically for when the players are in the ready up lobby before gameplay

local players get their own instance of this controller, allowing the controllers to respond independantly

responsible for: adding the enhanced input mapping context
listen for ready / previous cosmetic / next cosmetic inputs
work out which session player owns this controller
pass those action to multiplayerGameInstance where the session data is stored

*/
UCLASS()
class GROUP2_MULTIPLAYER_API AReadyUpPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	virtual void BeginPlay() override;

	// creating the controller input bindings
	virtual void SetupInputComponent() override;

	// call when controllers disconnect or are removed
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// enhanced input mapping and input actions containing the controls used in the ready up lobby
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Input")TObjectPtr<UInputMappingContext>LobbyMappingContext;

	// input action for ready up
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Input")TObjectPtr<UInputAction>ReadyAction;

	// inout action for previous choice
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Input")TObjectPtr<UInputAction>PreviousCosmeticAction;

	// input action for next choice
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Input")TObjectPtr<UInputAction>NextCosmeticAction;

	//still NEED to add more for cycling up and down between cosmetic choices beyond clour...

private:

	// input handlers for button presses
	void HandleReady();
	void HandlePreviousCosmetic();
	void HandleNextCosmetic();


	// find the session slot that belongs to 'this' controller, not assumoing localplayer 0 = session player0 etc...
	// use the platform user ID instead to com with the game instance where the session player belongs
	int32 GetPlayerSlot() const;
};