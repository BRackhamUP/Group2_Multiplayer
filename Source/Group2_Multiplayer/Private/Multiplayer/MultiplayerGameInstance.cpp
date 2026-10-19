// Fill out your copyright notice in the Description page of Project Settings.

#include "Multiplayer/MultiplayerGameInstance.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"

#include "GameFramework/InputDeviceLibrary.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/PlayerController.h"

#include "Kismet/GameplayStatics.h"

// internal struct used for for detecitng controllers, keep the physical device ID paired with platform user that owns it
struct FDetectedGamepad
{
	FInputDeviceId DeviceId;
	FPlatformUserId PlatformUserId;
};

// controller detction
void UMultiplayerGameInstance::GetConnectedGamepadMappings(TArray<FInputDeviceId>& OutDeviceIds, TArray<FPlatformUserId>& OutPlatformUsers) const
{
	// clear anyhting returned forom previous check
	OutDeviceIds.Empty();
	OutPlatformUsers.Empty();

	// get every input device unreal currentl sees
	TArray<FInputDeviceId> ConnectedDevices;

	UInputDeviceLibrary::GetAllConnectedInputDevices(ConnectedDevices);

	// enigne checks before trying the devices
	if (!GEngine)
	{
		return;
	}

	UInputDeviceSubsystem* InputDeviceSubsystem = GEngine->GetEngineSubsystem<UInputDeviceSubsystem>();

	if (!InputDeviceSubsystem)
	{
		return;
	}

	TArray<FDetectedGamepad> DetectedGamepads;

	//go through every connected input device and keep only valid gamepads
	for (const FInputDeviceId DeviceId : ConnectedDevices)
	{
		const FHardwareDeviceIdentifier HardwareDevice = InputDeviceSubsystem->GetInputDeviceHardwareIdentifier(DeviceId);

		// ignore keyboard and mouse input devices
		if (HardwareDevice.PrimaryDeviceType != EHardwareDevicePrimaryType::Gamepad)
		{
			continue;
		}

		// get the platform user unreal has associated with the physical controller
		const FPlatformUserId PlatformUserId = UInputDeviceLibrary::GetUserForInputDevice(DeviceId);

		// controllers need a valid platform user so inout can be assigned independantly 
		if (!PlatformUserId.IsValid())
		{
			continue;
		}

		// make sure th esmae oplatform user is not accidently added twice
		bool bPlatformUserAlreadyAdded = false;

		for (const FDetectedGamepad& ExistingGamepad : DetectedGamepads)
		{
			if (ExistingGamepad.PlatformUserId == PlatformUserId)
			{
				bPlatformUserAlreadyAdded = true;
				break;
			}
		}


		if (bPlatformUserAlreadyAdded)
		{

			continue;
		}

		// keep physical controller and its platform user paired together
		FDetectedGamepad NewGamepad;

		NewGamepad.DeviceId = DeviceId;
		NewGamepad.PlatformUserId = PlatformUserId;

		DetectedGamepads.Add(NewGamepad);
	}

	// sort by the device ID so the controller assignment is predicatable, lower IDs become the earlier player slotes
	DetectedGamepads.Sort([](const FDetectedGamepad& A, const FDetectedGamepad& B)
		{
			return A.DeviceId.GetId() < B.DeviceId.GetId();
		}
	);

	// only support 4 players max
	if (DetectedGamepads.Num() > 4)
	{
		DetectedGamepads.SetNum(4);
	}

	// split deteced controller data into two arrays used by rest of the system
	for (const FDetectedGamepad& Gamepad : DetectedGamepads)
	{
		OutDeviceIds.Add(Gamepad.DeviceId);
		OutPlatformUsers.Add(Gamepad.PlatformUserId);
	}
}

// return the number of valid connected gamepads
int32 UMultiplayerGameInstance::GetConnectedGamepadCount() const
{
	TArray<FInputDeviceId> DeviceIds;
	TArray<FPlatformUserId> PlatformUsers;

	GetConnectedGamepadMappings(DeviceIds, PlatformUsers);

	return DeviceIds.Num();
}

// chekc if controller list has changed
bool UMultiplayerGameInstance::DoesSessionMatchDetectedGamepads(const TArray<FInputDeviceId>& DeviceIds, const TArray<FPlatformUserId>& PlatformUsers) const
{
	// different unmber of controllers means the session needs rebuilding
	if (DeviceIds.Num() != SessionPlayers.Num())
	{
		return false;
	}

	if (PlatformUsers.Num() != SessionPlayers.Num())
	{
		return false;
	}

	// even if the number is the same check the IDs, for ctching cases where one controller disconnects and another plugs in to replace
	for (int32 Index = 0; Index < SessionPlayers.Num(); ++Index)
	{
		if (SessionPlayers[Index].InputDeviceInternalId != DeviceIds[Index].GetId())
		{
			return false;
		}

		if (SessionPlayers[Index].PlatformUserInternalId != PlatformUsers[Index].GetInternalId())
		{
			return false;
		}
	}
	// everything still matches so no rebuud needed
	return true;
}

// sycronise lobby players
void UMultiplayerGameInstance::SynchroniseLobbyPlayers()
{
	TArray<FInputDeviceId> DeviceIds;
	TArray<FPlatformUserId> PlatformUsers;

	// get the controllers currently connected to pc
	GetConnectedGamepadMappings(DeviceIds, PlatformUsers);

	// if the same controller are represented in the session, do nothign
	if (DoesSessionMatchDetectedGamepads(DeviceIds, PlatformUsers))
	{
		return;
	}

	// controller list changed so rebuild local players information
	RebuildLocalPlayersAndSession(DeviceIds, PlatformUsers);
}

// rebuild the local players session
void UMultiplayerGameInstance::RebuildLocalPlayersAndSession(const TArray<FInputDeviceId>& DeviceIds, const TArray<FPlatformUserId>& PlatformUsers)
{
	// remomve secondary players before reuilding but local player 0 is kept because unreal creates the primary local player for the GI
	while (GetNumLocalPlayers() > 1)
	{
		ULocalPlayer* PlayerToRemove = GetLocalPlayerByIndex(GetNumLocalPlayers() - 1);


		if (!PlayerToRemove)
		{
			break;
		}

		RemoveLocalPlayer(PlayerToRemove);
	}

	// clear old session info
	SessionPlayers.Empty();

	// if there are no controllers connected there is nothing to create
	if (PlatformUsers.IsEmpty())
	{
		OnSessionPlayersChanged.Broadcast();

		return;
	}

	// player 1 is auto created by inreal so instead o fcreating another just assign first detected controller to this player
	ULocalPlayer* FirstLocalPlayer = GetLocalPlayerByIndex(0);

	if (!FirstLocalPlayer)
	{
		return;
	}

	FirstLocalPlayer->SetPlatformUserId(PlatformUsers[0]);

	// players 2 to 4, create another local player for each phsical controller
	for (int32 Index = 1; Index < PlatformUsers.Num(); ++Index)
	{
		FString CreateError;

		ULocalPlayer* NewLocalPlayer = CreateLocalPlayer(PlatformUsers[Index], CreateError, false);

		// if local player creatinon fials then skip this controller
		if (!NewLocalPlayer)
		{
			continue;
		}

		FString SpawnError;

		// manually create th eplayercontorller belonging to this lcoal player
		const bool bSpawnSucceeded = NewLocalPlayer->SpawnPlayActor(TEXT(""), SpawnError, GetWorld());

		if (!bSpawnSucceeded)
		{
			continue;
		}

		// check that a playercontroller now exists for this platform user for handling input on ready up screen
		APlayerController* SpawnedController = UGameplayStatics::GetPlayerControllerFromPlatformUser(this, PlatformUsers[Index]);

		if (!SpawnedController)
		{

			continue;
		}
	}

	// build the session players array used for the rest of the game
	for (int32 Index = 0; Index < PlatformUsers.Num(); ++Index)
	{
		// find the localplayer belonging to the controller
		ULocalPlayer* LocalPlayer = FindLocalPlayerFromPlatformUserId(PlatformUsers[Index]);

		if (!LocalPlayer)
		{
			continue;
		}

		// make sure this player also has a valid player controller
		APlayerController* PlayerController = UGameplayStatics::GetPlayerControllerFromPlatformUser(this, PlatformUsers[Index]);

		if (!PlayerController)
		{
			continue;
		}

		FSessionPlayerData NewPlayerData;

		// session slots are zero based, first player added becomes slot 0
		NewPlayerData.PlayerSlot = SessionPlayers.Num();

		//players always enter the lobby as not ready
		NewPlayerData.bReady = false;

		//give each player a different starting colour for testing
		NewPlayerData.CosmeticIndex = NewPlayerData.PlayerSlot % FMath::Max(1, CosmeticOptionCount);
		// give each player a different starting colour when possible
		NewPlayerData.CosmeticIndex = BodyColourOptions.IsEmpty() ? 0 : NewPlayerData.PlayerSlot % BodyColourOptions.Num();

		// index 0 will be our "None" cosmetic
		NewPlayerData.HeadCosmeticIndex = 0;
		NewPlayerData.ChestCosmeticIndex = 0;
		NewPlayerData.LegsCosmeticIndex = 0;
		NewPlayerData.BackCosmeticIndex = 0;

		NewPlayerData.SelectedCustomisationRow = 0;

		NewPlayerData.PlatformUserInternalId = PlatformUsers[Index].GetInternalId();

		//remember which physical device belongs to this player
		NewPlayerData.InputDeviceInternalId = DeviceIds[Index].GetId();

		SessionPlayers.Add(NewPlayerData);
	}
	// tell listening widgets the player info has changed

	OnSessionPlayersChanged.Broadcast();
}

// session data getters

int32 UMultiplayerGameInstance::GetSessionPlayerCount() const
{
	return SessionPlayers.Num();
}

TArray<FSessionPlayerData>UMultiplayerGameInstance::GetSessionPlayers() const
{
	return SessionPlayers;
}

//find which session player belongs to a particular platform user
int32 UMultiplayerGameInstance::GetPlayerSlotForPlatformUser(int32 PlatformUserInternalId) const
{
	for (const FSessionPlayerData& PlayerData : SessionPlayers)
	{
		if (PlayerData.PlatformUserInternalId == PlatformUserInternalId)
		{
			return PlayerData.PlayerSlot;
		}
	}

	// no matching session player exists
	return INDEX_NONE;
}

// ready state

bool UMultiplayerGameInstance::ReadyPlayer(int32 PlayerSlot)
{
	// make sure the requested session player exists
	if (!SessionPlayers.IsValidIndex(PlayerSlot))
	{
		return false;
	}

	FSessionPlayerData& PlayerData = SessionPlayers[PlayerSlot];

	// once ready then players cannot unready... for now i guess
	if (PlayerData.bReady)
	{
		return false;
	}

	PlayerData.bReady = true;

	// notify UI this players state change
	OnSessionPlayersChanged.Broadcast();

	return true;
}


bool UMultiplayerGameInstance::AreAllSessionPlayersReady() const
{
	if (SessionPlayers.IsEmpty())
	{
		return false;
	}

	for (const FSessionPlayerData& PlayerData : SessionPlayers)
	{
		if (!PlayerData.bReady)
		{
			return false;
		}
	}

	return true;
}

void UMultiplayerGameInstance::MoveCustomisationSelection(int32 PlayerSlot, int32 Direction)
{
	if (!SessionPlayers.IsValidIndex(PlayerSlot))
	{
		return;
	}

	FSessionPlayerData& PlayerData = SessionPlayers[PlayerSlot];

	// customisation locks once the player readies
	if (PlayerData.bReady)
	{
		return;
	}

	constexpr int32 CategoryCount = 5;

	int32 NewRow = (PlayerData.SelectedCustomisationRow + Direction) % CategoryCount;

	if (NewRow < 0)
	{
		NewRow += CategoryCount;
	}

	PlayerData.SelectedCustomisationRow = NewRow;

	OnSessionPlayersChanged.Broadcast();
}

void UMultiplayerGameInstance::CycleCosmetic(int32 PlayerSlot, int32 Direction)
{
	if (!SessionPlayers.IsValidIndex(PlayerSlot))
	{
		return;
	}

	FSessionPlayerData& PlayerData = SessionPlayers[PlayerSlot];

	if (PlayerData.bReady)
	{
		return;
	}

	switch (PlayerData.SelectedCustomisationRow)
	{
	case 0:

		CycleIndexWrapped(PlayerData.CosmeticIndex, BodyColourOptions.Num(), Direction);

		break;


	case 1:

		CycleIndexWrapped(PlayerData.HeadCosmeticIndex, HeadCosmeticOptions.Num(), Direction);

		break;


	case 2:

		CycleIndexWrapped(PlayerData.ChestCosmeticIndex, ChestCosmeticOptions.Num(), Direction);

		break;


	case 3:

		CycleIndexWrapped(PlayerData.LegsCosmeticIndex, LegsCosmeticOptions.Num(), Direction);

		break;


	case 4:

		CycleIndexWrapped(PlayerData.BackCosmeticIndex, BackCosmeticOptions.Num(), Direction);

		break;


	default:
		break;
	}

	OnSessionPlayersChanged.Broadcast();
}


void UMultiplayerGameInstance::CycleIndexWrapped(int32& Index, int32 OptionCount, int32 Direction) const
{
	if (OptionCount <= 0)
	{
		Index = 0;
		return;
	}

	Index = (Index + Direction) % OptionCount;

	if (Index < 0)
	{
		Index += OptionCount;
	}
}


int32 UMultiplayerGameInstance::GetOptionCountForCategory(int32 CategoryIndex) const
{
	switch (CategoryIndex)
	{
	case 0:
		return BodyColourOptions.Num();

	case 1:
		return HeadCosmeticOptions.Num();

	case 2:
		return ChestCosmeticOptions.Num();

	case 3:
		return LegsCosmeticOptions.Num();

	case 4:
		return BackCosmeticOptions.Num();

	default:
		return 0;
	}
}


int32 UMultiplayerGameInstance::GetCosmeticOptionCount() const
{
	return BodyColourOptions.Num();
}

FText UMultiplayerGameInstance::GetCustomisationDisplayName(int32 PlayerSlot, int32 CategoryIndex) const
{
	if (!SessionPlayers.IsValidIndex(PlayerSlot))
	{
		return FText::FromString(TEXT("None"));
	}

	const FSessionPlayerData& PlayerData = SessionPlayers[PlayerSlot];

	auto ResolveName = [](const FText& DisplayName, const FName CosmeticId)
		{
			if (!DisplayName.IsEmpty())
			{
				return DisplayName;
			}

			if (!CosmeticId.IsNone())
			{
				return FText::FromName(CosmeticId);
			}

			return FText::FromString(TEXT("None"));
		};

	switch (CategoryIndex)
	{
	case 0:

		if (BodyColourOptions.IsValidIndex(PlayerData.CosmeticIndex))
		{
			const FBodyColourOption& Option = BodyColourOptions[PlayerData.CosmeticIndex];

			return ResolveName(Option.DisplayName, Option.CosmeticId);
		}

		break;


	case 1:

		if (HeadCosmeticOptions.IsValidIndex(PlayerData.HeadCosmeticIndex))
		{
			const FCosmeticMeshOption& Option = HeadCosmeticOptions[PlayerData.HeadCosmeticIndex];

			return ResolveName(Option.DisplayName, Option.CosmeticId);
		}

		break;


	case 2:

		if (ChestCosmeticOptions.IsValidIndex(PlayerData.ChestCosmeticIndex))
		{
			const FCosmeticMeshOption& Option = ChestCosmeticOptions[PlayerData.ChestCosmeticIndex];

			return ResolveName(Option.DisplayName, Option.CosmeticId);
		}

		break;


	case 3:

		if (LegsCosmeticOptions.IsValidIndex(PlayerData.LegsCosmeticIndex))
		{
			const FCosmeticMeshOption& Option = LegsCosmeticOptions[PlayerData.LegsCosmeticIndex];

			return ResolveName(Option.DisplayName, Option.CosmeticId);
		}

		break;


	case 4:

		if (BackCosmeticOptions.IsValidIndex(PlayerData.BackCosmeticIndex))
		{
			const FCosmeticMeshOption& Option = BackCosmeticOptions[PlayerData.BackCosmeticIndex];

			return ResolveName(Option.DisplayName, Option.CosmeticId);
		}

		break;


	default:
		break;
	}

	return FText::FromString(TEXT("None"));
}

// reset sesssoin
void UMultiplayerGameInstance::ResetSession()
{
	for (FSessionPlayerData& PlayerData : SessionPlayers)
	{

		PlayerData.bReady = false;

		PlayerData.SelectedCustomisationRow = 0;
	}

	OnSessionPlayersChanged.Broadcast();
}