// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ReadyUpPlayerController.generated.h"


class UInputAction;
class UInputMappingContext;
struct FInputActionValue;


/*
player controller used specifically for the ready up/customisation lobby

each LocalPlayer gets their own instance so all physical controllers
can customise and ready independently
*/
UCLASS()
class GROUP2_MULTIPLAYER_API AReadyUpPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	virtual void BeginPlay() override;

	virtual void SetupInputComponent() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;


	// lobby controls
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Input")
	TObjectPtr<UInputMappingContext> LobbyMappingContext;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Input")
	TObjectPtr<UInputAction> ReadyAction;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Input")
	TObjectPtr<UInputAction> PreviousCosmeticAction;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Input")
	TObjectPtr<UInputAction> NextCosmeticAction;


	// left stick / D-Pad up and down
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Input")
	TObjectPtr<UInputAction> NavigateCustomisationAction;


private:

	void HandleReady();

	void HandlePreviousCosmetic();

	void HandleNextCosmetic();

	void HandleNavigateCustomisation(const FInputActionValue& Value);


	// find which session player owns this controller
	int32 GetPlayerSlot() const;
};