// Fill out your copyright notice in the Description page of Project Settings.

#include "Multiplayer/ReadyUpPlayerController.h"

#include "Multiplayer/MultiplayerGameInstance.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

#include "Engine/LocalPlayer.h"


void AReadyUpPlayerController::BeginPlay()
{
	Super::BeginPlay();
}


void AReadyUpPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();


	ULocalPlayer* LocalPlayer = GetLocalPlayer();


	if (!LocalPlayer)
	{
		return;
	}


	/*
	add lobby mapping here rather than BeginPlay because secondary
	local players are created dynamically
	*/
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();


	if (InputSubsystem && LobbyMappingContext)
	{
		InputSubsystem->RemoveMappingContext(LobbyMappingContext);


		InputSubsystem->AddMappingContext(LobbyMappingContext, 0);
	}


	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);


	if (!EnhancedInputComponent)
	{
		return;
	}


	if (ReadyAction)
	{
		EnhancedInputComponent->BindAction(ReadyAction, ETriggerEvent::Started, this, &AReadyUpPlayerController::HandleReady);
	}


	if (PreviousCosmeticAction)
	{
		EnhancedInputComponent->BindAction(PreviousCosmeticAction, ETriggerEvent::Started, this, &AReadyUpPlayerController::HandlePreviousCosmetic);
	}


	if (NextCosmeticAction)
	{
		EnhancedInputComponent->BindAction(NextCosmeticAction, ETriggerEvent::Started, this, &AReadyUpPlayerController::HandleNextCosmetic);
	}


	/*
	Started means the menu moves once when the stick crosses the deadzone,
	rather than scrolling every frame while the stick is held
	*/
	if (NavigateCustomisationAction)
	{
		EnhancedInputComponent->BindAction(NavigateCustomisationAction, ETriggerEvent::Started, this, &AReadyUpPlayerController::HandleNavigateCustomisation);
	}
}

void AReadyUpPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (LobbyMappingContext)
			{
				InputSubsystem->RemoveMappingContext(LobbyMappingContext);
			}
		}
	}


	Super::EndPlay(EndPlayReason);
}


int32 AReadyUpPlayerController::GetPlayerSlot() const
{
	const FPlatformUserId PlatformUserId = GetPlatformUserId();


	if (!PlatformUserId.IsValid())
	{
		return INDEX_NONE;
	}


	const UMultiplayerGameInstance* MultiplayerGameInstance = Cast<UMultiplayerGameInstance>(GetGameInstance());


	if (!MultiplayerGameInstance)
	{
		return INDEX_NONE;
	}


	return MultiplayerGameInstance->GetPlayerSlotForPlatformUser(PlatformUserId.GetInternalId());
}



void AReadyUpPlayerController::HandleReady()
{
	const int32 PlayerSlot = GetPlayerSlot();


	if (PlayerSlot == INDEX_NONE)
	{
		return;
	}


	UMultiplayerGameInstance* MultiplayerGameInstance = Cast<UMultiplayerGameInstance>(GetGameInstance());


	if (!MultiplayerGameInstance)
	{
		return;
	}


	MultiplayerGameInstance->ReadyPlayer(PlayerSlot);
}


void AReadyUpPlayerController::HandlePreviousCosmetic()
{
	const int32 PlayerSlot = GetPlayerSlot();


	if (PlayerSlot == INDEX_NONE)
	{
		return;
	}


	UMultiplayerGameInstance* MultiplayerGameInstance = Cast<UMultiplayerGameInstance>(GetGameInstance());


	if (!MultiplayerGameInstance)
	{
		return;
	}


	MultiplayerGameInstance->CycleCosmetic(PlayerSlot, -1);
}


void AReadyUpPlayerController::HandleNextCosmetic()
{
	const int32 PlayerSlot = GetPlayerSlot();


	if (PlayerSlot == INDEX_NONE)
	{
		return;
	}


	UMultiplayerGameInstance* MultiplayerGameInstance = Cast<UMultiplayerGameInstance>(GetGameInstance());


	if (!MultiplayerGameInstance)
	{
		return;
	}


	MultiplayerGameInstance->CycleCosmetic(PlayerSlot, 1);
}

void AReadyUpPlayerController::HandleNavigateCustomisation(const FInputActionValue& Value)
{
	const int32 PlayerSlot = GetPlayerSlot();


	if (PlayerSlot == INDEX_NONE)
	{
		return;
	}


	const float AxisValue = Value.Get<float>();


	if (FMath::IsNearlyZero(AxisValue))
	{
		return;
	}


	const int32 Direction = AxisValue > 0.0f ? -1 : 1;


	UMultiplayerGameInstance* MultiplayerGameInstance = Cast<UMultiplayerGameInstance>(GetGameInstance());


	if (!MultiplayerGameInstance)
	{
		return;
	}


	MultiplayerGameInstance->MoveCustomisationSelection(PlayerSlot, Direction);
}