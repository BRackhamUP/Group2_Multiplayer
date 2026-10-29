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

		// give each player a different starting ring colour when possible
		NewPlayerData.RingColourIndex = RingColourOptions.IsEmpty() ? 0 : NewPlayerData.PlayerSlot % RingColourOptions.Num();

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

	// 0 body colour, 1 head, 2 chest, 3 legs, 4 back, 5 ring colour
	constexpr int32 CategoryCount = 6;

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


	case 5:

		CycleRingColour(PlayerSlot, Direction);

		return;


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

	case 5:
		return RingColourOptions.Num();

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


	case 5:

		if (RingColourOptions.IsValidIndex(PlayerData.RingColourIndex))
		{
			const FBodyColourOption& Option = RingColourOptions[PlayerData.RingColourIndex];

			return ResolveName(Option.DisplayName, Option.CosmeticId);
		}

		break;


	default:
		break;
	}

	return FText::FromString(TEXT("None"));
}


// player ring colour

bool UMultiplayerGameInstance::SetRingColour(int32 PlayerSlot, int32 ColourIndex)
{
	if (!SessionPlayers.IsValidIndex(PlayerSlot) || !RingColourOptions.IsValidIndex(ColourIndex))
	{
		return false;
	}

	FSessionPlayerData& PlayerData = SessionPlayers[PlayerSlot];

	// customisation locks once the player readies
	if (PlayerData.bReady || IsRingColourTaken(ColourIndex, PlayerSlot))
	{
		return false;
	}

	PlayerData.RingColourIndex = ColourIndex;

	OnSessionPlayersChanged.Broadcast();

	return true;
}

void UMultiplayerGameInstance::CycleRingColour(int32 PlayerSlot, int32 Direction)
{
	if (!SessionPlayers.IsValidIndex(PlayerSlot) || RingColourOptions.IsEmpty() || Direction == 0)
	{
		return;
	}

	FSessionPlayerData& PlayerData = SessionPlayers[PlayerSlot];

	if (PlayerData.bReady)
	{
		return;
	}

	const int32 OptionCount = RingColourOptions.Num();
	const int32 Step = Direction > 0 ? 1 : -1;
	int32 Candidate = PlayerData.RingColourIndex;

	// cycle colours until we find one nobody else has
	// if every other colour is taken nothing changes (this should never happen anyway)
	for (int32 Attempt = 0; Attempt < OptionCount; ++Attempt)
	{
		CycleIndexWrapped(Candidate, OptionCount, Step);

		if (!IsRingColourTaken(Candidate, PlayerSlot))
		{
			PlayerData.RingColourIndex = Candidate;

			OnSessionPlayersChanged.Broadcast();

			return;
		}
	}
}

bool UMultiplayerGameInstance::IsRingColourTaken(int32 ColourIndex, int32 IgnorePlayerSlot) const
{
	for (const FSessionPlayerData& PlayerData : SessionPlayers)
	{
		if (PlayerData.PlayerSlot != IgnorePlayerSlot && PlayerData.RingColourIndex == ColourIndex)
		{
			return true;
		}
	}

	return false;
}

int32 UMultiplayerGameInstance::GetRingColourIndex(int32 PlayerSlot) const
{
	return SessionPlayers.IsValidIndex(PlayerSlot) ? SessionPlayers[PlayerSlot].RingColourIndex : INDEX_NONE;
}

FLinearColor UMultiplayerGameInstance::GetRingColourForPlayerSlot(int32 PlayerSlot) const
{
	if (RingColourOptions.IsEmpty())
	{
		return FLinearColor::White;
	}

	// chosen colour from the lobby if there is one (there should always be one)
	if (SessionPlayers.IsValidIndex(PlayerSlot) && RingColourOptions.IsValidIndex(SessionPlayers[PlayerSlot].RingColourIndex))
	{
		return RingColourOptions[SessionPlayers[PlayerSlot].RingColourIndex].Colour;
	}

	// if there is no session (testing) then use slots default colour
	return RingColourOptions[FMath::Max(PlayerSlot, 0) % RingColourOptions.Num()].Colour;
}

int32 UMultiplayerGameInstance::GetPlayerSlotForController(const AController* Controller) const
{
	const APlayerController* PlayerController = Cast<APlayerController>(Controller);

	if (!PlayerController)
	{
		return INDEX_NONE;
	}

	// same matching the lobby uses
	const FPlatformUserId PlatformUserId = PlayerController->GetPlatformUserId();

	if (PlatformUserId.IsValid())
	{
		const int32 PlayerSlot = GetPlayerSlotForPlatformUser(PlatformUserId.GetInternalId());

		if (PlayerSlot != INDEX_NONE)
		{
			return PlayerSlot;
		}
	}

	// no lobby session - fall back to the order unreal created the local players in
	if (const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
	{
		return GetLocalPlayers().IndexOfByKey(LocalPlayer);
	}

	return INDEX_NONE;
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