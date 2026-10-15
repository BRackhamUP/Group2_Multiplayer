// Fill out your copyright notice in the Description page of Project Settings.

#include "Multiplayer/ReadyUpPlayerController.h"

#include "Multiplayer/MultiplayerGameInstance.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

#include "Engine/LocalPlayer.h"


void AReadyUpPlayerController::BeginPlay()
{
	Super::BeginPlay();
}


// setting up the input system
void AReadyUpPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// reference getter for the local player that owns 'this' controller, each controller gets it own local player
	ULocalPlayer* LocalPlayer = GetLocalPlayer();

	// adding enhanced input mapping not in begin play so that the other players joining dynamically can still get acces to the inputs
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();


	if (InputSubsystem && LobbyMappingContext)
	{
		// preventing duplicate copies if input is reinitialised after disconnecting
		InputSubsystem->RemoveMappingContext(LobbyMappingContext);

		// adding the mapping contorls to players in lobby
		InputSubsystem->AddMappingContext(LobbyMappingContext, 0);
	}

	// get the enhanced input componant
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);

	// handle the ready up input when its called for 'this' player
	if (ReadyAction)
	{
		EnhancedInputComponent->BindAction(ReadyAction,	ETriggerEvent::Started,	this, &AReadyUpPlayerController::HandleReady);
	}

	// handle the previous cosmetic action
	if (PreviousCosmeticAction)
	{
		EnhancedInputComponent->BindAction(PreviousCosmeticAction, ETriggerEvent::Started, this,&AReadyUpPlayerController::HandlePreviousCosmetic);
	}

	// handle the next cosmetic action
	if (NextCosmeticAction)
	{
		EnhancedInputComponent->BindAction(NextCosmeticAction, ETriggerEvent::Started, this, &AReadyUpPlayerController::HandleNextCosmetic);
	}
}

// end play removes the lobby IMC so controls dont carry into gameplay level, local player persists though so session and cosmetic data is stored in game instance
void AReadyUpPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULocalPlayer* LocalPlayer =	GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem*	InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (LobbyMappingContext)
			{
				InputSubsystem->RemoveMappingContext(LobbyMappingContext);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}


// find 'this' controllers session player, all individual controllers have an associated platform user. using that identifyer instead of controller numbers
int32 AReadyUpPlayerController::GetPlayerSlot() const
{
	const FPlatformUserId PlatformUserId = GetPlatformUserId();

	// if unreal has not assigned this controller to valid platform user then return none
	if (!PlatformUserId.IsValid())
	{
		return INDEX_NONE;
	}

	// access the shared sesssoin info stored in multiplayergameinstance
	const UMultiplayerGameInstance*	MultiplayerGameInstance = Cast<UMultiplayerGameInstance>(GetGameInstance());

	if (!MultiplayerGameInstance)
	{
		return INDEX_NONE;
	}

	// check gameinstance which session slot beongs to this platform user
	return MultiplayerGameInstance->GetPlayerSlotForPlatformUser(PlatformUserId.GetInternalId());
}


// ready input
void AReadyUpPlayerController::HandleReady()
{
	// check which player pressed ready
	const int32 PlayerSlot = GetPlayerSlot();

	// if its index none then it cant be matched to session player
	if (PlayerSlot == INDEX_NONE)
	{
		return;
	}

	// gameinstance owns the actual ready state, playercontrols just forward players inptu to it
	UMultiplayerGameInstance*MultiplayerGameInstance = Cast<UMultiplayerGameInstance>(GetGameInstance());

	if (!MultiplayerGameInstance)
	{
		return;
	}

	// mark only this oplayers session slot ready
	MultiplayerGameInstance->ReadyPlayer(PlayerSlot);
}


// handle previous cosmetic input
void AReadyUpPlayerController::HandlePreviousCosmetic()
{
	// identify who owns
	const int32 PlayerSlot = GetPlayerSlot();

	if (PlayerSlot == INDEX_NONE)
	{
		return;
	}

	UMultiplayerGameInstance* MultiplayerGameInstance =	Cast<UMultiplayerGameInstance>(GetGameInstance());

	if (!MultiplayerGameInstance)
	{
		return;
	}

	// -1 for moving backward throught the cosmetic list
	MultiplayerGameInstance->CycleCosmetic(PlayerSlot, -1);
}


//handle next cosmetic input
void AReadyUpPlayerController::HandleNextCosmetic()
{
	// identify who owns
	const int32 PlayerSlot = GetPlayerSlot();

	if (PlayerSlot == INDEX_NONE)
	{
		return;
	}

	UMultiplayerGameInstance*MultiplayerGameInstance = Cast<UMultiplayerGameInstance>(GetGameInstance());

	if (!MultiplayerGameInstance)
	{
		return;
	}

	// +1 for moving forward through the list
	MultiplayerGameInstance->CycleCosmetic(PlayerSlot, 1);
}