// Fill out your copyright notice in the Description page of Project Settings.

#include "Multiplayer/MultiplayerGameInstance.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"

#include "GameFramework/InputDeviceLibrary.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/PlayerController.h"

#include "Kismet/GameplayStatics.h"


// keeps each physical controller paired with the platform user that owns it
struct FDetectedGamepad
{
	FInputDeviceId DeviceId;
	FPlatformUserId PlatformUserId;
};

void UMultiplayerGameInstance::GetConnectedGamepadMappings(TArray<FInputDeviceId>& OutDeviceIds, TArray<FPlatformUserId>& OutPlatformUsers) const
{
	OutDeviceIds.Empty();
	OutPlatformUsers.Empty();

	TArray<FInputDeviceId> ConnectedDevices;

	UInputDeviceLibrary::GetAllConnectedInputDevices(ConnectedDevices);

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

	for (const FInputDeviceId DeviceId : ConnectedDevices)
	{
		const FHardwareDeviceIdentifier HardwareDevice = InputDeviceSubsystem->GetInputDeviceHardwareIdentifier(DeviceId);

		// ignore keyboard/mouse/etc
		if (HardwareDevice.PrimaryDeviceType != EHardwareDevicePrimaryType::Gamepad)
		{
			continue;
		}

		const FPlatformUserId PlatformUserId = UInputDeviceLibrary::GetUserForInputDevice(DeviceId);

		if (!PlatformUserId.IsValid())
		{
			continue;
		}

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

		FDetectedGamepad NewGamepad;

		NewGamepad.DeviceId = DeviceId;
		NewGamepad.PlatformUserId = PlatformUserId;

		DetectedGamepads.Add(NewGamepad);
	}

	// lower device IDs become the earlier player slots
	DetectedGamepads.Sort([](const FDetectedGamepad& A, const FDetectedGamepad& B)
		{
			return A.DeviceId.GetId() < B.DeviceId.GetId();
		}
	);

	if (DetectedGamepads.Num() > 4)
	{
		DetectedGamepads.SetNum(4);
	}

	for (const FDetectedGamepad& Gamepad : DetectedGamepads)
	{
		OutDeviceIds.Add(Gamepad.DeviceId);
		OutPlatformUsers.Add(Gamepad.PlatformUserId);
	}
}


int32 UMultiplayerGameInstance::GetConnectedGamepadCount() const
{
	TArray<FInputDeviceId> DeviceIds;
	TArray<FPlatformUserId> PlatformUsers;

	GetConnectedGamepadMappings(DeviceIds, PlatformUsers);

	return DeviceIds.Num();
}


bool UMultiplayerGameInstance::DoesSessionMatchDetectedGamepads(const TArray<FInputDeviceId>& DeviceIds, const TArray<FPlatformUserId>& PlatformUsers) const
{
	if (DeviceIds.Num() != SessionPlayers.Num())
	{
		return false;
	}

	if (PlatformUsers.Num() != SessionPlayers.Num())
	{
		return false;
	}

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

	return true;
}


void UMultiplayerGameInstance::SynchroniseLobbyPlayers()
{
	TArray<FInputDeviceId> DeviceIds;
	TArray<FPlatformUserId> PlatformUsers;

	GetConnectedGamepadMappings(DeviceIds, PlatformUsers);

	if (DoesSessionMatchDetectedGamepads(DeviceIds, PlatformUsers))
	{
		return;
	}

	RebuildLocalPlayersAndSession(DeviceIds, PlatformUsers);
}


void UMultiplayerGameInstance::RebuildLocalPlayersAndSession(const TArray<FInputDeviceId>& DeviceIds, const TArray<FPlatformUserId>& PlatformUsers)
{
	// remove secondary local players before rebuilding
	while (GetNumLocalPlayers() > 1)
	{
		ULocalPlayer* PlayerToRemove = GetLocalPlayerByIndex(GetNumLocalPlayers() - 1);

		if (!PlayerToRemove)
		{
			break;
		}

		RemoveLocalPlayer(PlayerToRemove);
	}

	SessionPlayers.Empty();

	if (PlatformUsers.IsEmpty())
	{
		OnSessionPlayersChanged.Broadcast();
		return;
	}

	// Player 1 uses Unreal's existing LocalPlayer 0
	ULocalPlayer* FirstLocalPlayer = GetLocalPlayerByIndex(0);

	if (!FirstLocalPlayer)
	{
		return;
	}

	FirstLocalPlayer->SetPlatformUserId(PlatformUsers[0]);

	/*
	players 2-4 are created manually

	false prevents CreateLocalPlayer from trying to create the
	PlayerController itself - SpawnPlayActor handles that below
	*/
	for (int32 Index = 1; Index < PlatformUsers.Num(); ++Index)
	{
		FString CreateError;

		ULocalPlayer* NewLocalPlayer = CreateLocalPlayer(PlatformUsers[Index], CreateError, false);

		if (!NewLocalPlayer)
		{
			continue;
		}

		FString SpawnError;

		const bool bSpawnSucceeded = NewLocalPlayer->SpawnPlayActor(TEXT(""), SpawnError, GetWorld());

		if (!bSpawnSucceeded)
		{
			continue;
		}

		APlayerController* SpawnedController = UGameplayStatics::GetPlayerControllerFromPlatformUser(this, PlatformUsers[Index]);

		if (!SpawnedController)
		{
			continue;
		}
	}

	// build session player data
	for (int32 Index = 0; Index < PlatformUsers.Num(); ++Index)
	{
		ULocalPlayer* LocalPlayer = FindLocalPlayerFromPlatformUserId(PlatformUsers[Index]);

		if (!LocalPlayer)
		{
			continue;
		}

		APlayerController* PlayerController = UGameplayStatics::GetPlayerControllerFromPlatformUser(this, PlatformUsers[Index]);

		if (!PlayerController)
		{
			continue;
		}

		FSessionPlayerData NewPlayerData;

		NewPlayerData.PlayerSlot = SessionPlayers.Num();

		NewPlayerData.bReady = false;

		// give each player a different starting colour when possible
		NewPlayerData.CosmeticIndex = BodyColourOptions.IsEmpty() ? 0 : NewPlayerData.PlayerSlot % BodyColourOptions.Num();

		// index 0 will be our "None" cosmetic
		NewPlayerData.HeadCosmeticIndex = 0;
		NewPlayerData.ChestCosmeticIndex = 0;
		NewPlayerData.LegsCosmeticIndex = 0;
		NewPlayerData.BackCosmeticIndex = 0;

		NewPlayerData.SelectedCustomisationRow = 0;

		NewPlayerData.PlatformUserInternalId = PlatformUsers[Index].GetInternalId();

		NewPlayerData.InputDeviceInternalId = DeviceIds[Index].GetId();

		SessionPlayers.Add(NewPlayerData);
	}

	OnSessionPlayersChanged.Broadcast();
}


int32 UMultiplayerGameInstance::GetSessionPlayerCount() const
{
	return SessionPlayers.Num();
}

TArray<FSessionPlayerData>UMultiplayerGameInstance::GetSessionPlayers() const
{
	return SessionPlayers;
}

int32 UMultiplayerGameInstance::GetPlayerSlotForPlatformUser(int32 PlatformUserInternalId) const
{
	for (const FSessionPlayerData& PlayerData : SessionPlayers)
	{
		if (PlayerData.PlatformUserInternalId == PlatformUserInternalId)
		{
			return PlayerData.PlayerSlot;
		}
	}

	return INDEX_NONE;
}


bool UMultiplayerGameInstance::ReadyPlayer(int32 PlayerSlot)
{
	if (!SessionPlayers.IsValidIndex(PlayerSlot))
	{
		return false;
	}

	FSessionPlayerData& PlayerData = SessionPlayers[PlayerSlot];

	// ready is currently one-way
	if (PlayerData.bReady)
	{
		return false;
	}

	PlayerData.bReady = true;

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

void UMultiplayerGameInstance::ResetSession()
{
	for (FSessionPlayerData& PlayerData : SessionPlayers)
	{

		PlayerData.bReady = false;

		PlayerData.SelectedCustomisationRow = 0;
	}

	OnSessionPlayersChanged.Broadcast();
}