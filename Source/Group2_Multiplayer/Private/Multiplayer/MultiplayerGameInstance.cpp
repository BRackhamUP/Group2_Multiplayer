#include "Multiplayer/MultiplayerGameInstance.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/InputDeviceLibrary.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/PlayerController.h"

static void CycleIndexWrapped(int32& Index, int32 OptionCount, int32 Direction)
{
	if (OptionCount <= 0)
	{
		Index = 0;
		return;
	}

	if (Direction > 0)
	{
		++Index;
	}
	else if (Direction < 0)
	{
		--Index;
	}

	if (Index >= OptionCount)
	{
		Index = 0;
	}

	if (Index < 0)
	{
		Index = OptionCount - 1;
	}
}

static FText GetOptionName(const FText& DisplayName, const FName& CosmeticId)
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
}

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

	for (int32 DeviceIndex = 0; DeviceIndex < ConnectedDevices.Num(); ++DeviceIndex)
	{
		const FInputDeviceId DeviceId = ConnectedDevices[DeviceIndex];
		const FHardwareDeviceIdentifier HardwareDevice = InputDeviceSubsystem->GetInputDeviceHardwareIdentifier(DeviceId);

		if (HardwareDevice.PrimaryDeviceType != EHardwareDevicePrimaryType::Gamepad)
		{
			continue;
		}

		const FPlatformUserId PlatformUserId = UInputDeviceLibrary::GetUserForInputDevice(DeviceId);

		if (!PlatformUserId.IsValid())
		{
			continue;
		}

		bool bAlreadyAdded = false;

		for (int32 ExistingIndex = 0; ExistingIndex < OutPlatformUsers.Num(); ++ExistingIndex)
		{
			if (OutPlatformUsers[ExistingIndex] == PlatformUserId)
			{
				bAlreadyAdded = true;
				break;
			}
		}

		if (bAlreadyAdded)
		{
			continue;
		}

		OutDeviceIds.Add(DeviceId);
		OutPlatformUsers.Add(PlatformUserId);

		if (OutPlatformUsers.Num() >= 4)
		{
			break;
		}
	}
}

int32 UMultiplayerGameInstance::GetPlayerNumberAtDisplayPosition(int32 DisplayPosition) const
{
	for (int32 PlayerIndex = 0; PlayerIndex < SessionPlayers.Num(); ++PlayerIndex)
	{
		const FSessionPlayerData& PlayerData = SessionPlayers[PlayerIndex];

		if (PlayerData.bHasJoined && PlayerData.bConnected && PlayerData.DisplayPosition == DisplayPosition)
		{
			return PlayerData.PlayerNumber;
		}
	}

	return INDEX_NONE;
}

void UMultiplayerGameInstance::SynchroniseControllers()
{
	TArray<FInputDeviceId> DeviceIds;
	TArray<FPlatformUserId> PlatformUsers;
	GetConnectedGamepadMappings(DeviceIds, PlatformUsers);

	const int32 RequiredLocalPlayerCount = FMath::Max(1, PlatformUsers.Num());
	bool bNeedToRebuildLocalPlayers = GetNumLocalPlayers() != RequiredLocalPlayerCount;

	if (!bNeedToRebuildLocalPlayers)
	{
		for (int32 ControllerIndex = 0; ControllerIndex < PlatformUsers.Num(); ++ControllerIndex)
		{
			if (!FindLocalPlayerFromPlatformUserId(PlatformUsers[ControllerIndex]))
			{
				bNeedToRebuildLocalPlayers = true;
				break;
			}
		}
	}

	if (bNeedToRebuildLocalPlayers)
	{
		// Remove secondary LocalPlayers before rebuilding them.
		while (GetNumLocalPlayers() > 1)
		{
			ULocalPlayer* PlayerToRemove = GetLocalPlayerByIndex(GetNumLocalPlayers() - 1);

			if (!PlayerToRemove)
			{
				break;
			}

			RemoveLocalPlayer(PlayerToRemove);
		}

		if (!PlatformUsers.IsEmpty())
		{
			ULocalPlayer* FirstLocalPlayer = GetLocalPlayerByIndex(0);

			if (FirstLocalPlayer)
			{
				FirstLocalPlayer->SetPlatformUserId(PlatformUsers[0]);
			}

			for (int32 ControllerIndex = 1; ControllerIndex < PlatformUsers.Num(); ++ControllerIndex)
			{
				FString CreateError;
				ULocalPlayer* NewLocalPlayer = CreateLocalPlayer(PlatformUsers[ControllerIndex], CreateError, false);

				if (!NewLocalPlayer)
				{
					continue;
				}

				FString SpawnError;
				NewLocalPlayer->SpawnPlayActor(TEXT(""), SpawnError, GetWorld());
			}
		}
	}

	bool bSessionChanged = false;

	for (int32 PlayerIndex = 0; PlayerIndex < SessionPlayers.Num(); ++PlayerIndex)
	{
		FSessionPlayerData& PlayerData = SessionPlayers[PlayerIndex];

		if (!PlayerData.bHasJoined)
		{
			continue;
		}

		bool bControllerFound = false;
		int32 CurrentDeviceId = INDEX_NONE;

		for (int32 ControllerIndex = 0; ControllerIndex < PlatformUsers.Num(); ++ControllerIndex)
		{
			if (PlayerData.PlatformUserInternalId == PlatformUsers[ControllerIndex].GetInternalId())
			{
				bControllerFound = true;
				CurrentDeviceId = DeviceIds[ControllerIndex].GetId();
				break;
			}
		}

		if (PlayerData.bConnected != bControllerFound)
		{
			PlayerData.bConnected = bControllerFound;
			bSessionChanged = true;

			if (!bControllerFound)
			{
				PlayerData.bReady = false;
			}
		}

		if (PlayerData.InputDeviceInternalId != CurrentDeviceId)
		{
			PlayerData.InputDeviceInternalId = CurrentDeviceId;
			bSessionChanged = true;
		}
	}

	if (bSessionChanged)
	{
		UpdateDisplayPositions();
		OnSessionPlayersChanged.Broadcast();
	}
}

int32 UMultiplayerGameInstance::JoinPlayer(int32 PlatformUserInternalId)
{
	if (SessionPlayers.Num() != 4)
	{
		return INDEX_NONE;
	}

	TArray<FInputDeviceId> DeviceIds;
	TArray<FPlatformUserId> PlatformUsers;
	GetConnectedGamepadMappings(DeviceIds, PlatformUsers);

	bool bControllerIsConnected = false;
	int32 CurrentDeviceId = INDEX_NONE;

	for (int32 ControllerIndex = 0; ControllerIndex < PlatformUsers.Num(); ++ControllerIndex)
	{
		if (PlatformUsers[ControllerIndex].GetInternalId() == PlatformUserInternalId)
		{
			bControllerIsConnected = true;
			CurrentDeviceId = DeviceIds[ControllerIndex].GetId();
			break;
		}
	}

	if (!bControllerIsConnected)
	{
		return INDEX_NONE;
	}

	// Reconnect an existing permanent player.
	for (int32 PlayerIndex = 0; PlayerIndex < SessionPlayers.Num(); ++PlayerIndex)
	{
		FSessionPlayerData& PlayerData = SessionPlayers[PlayerIndex];

		if (PlayerData.bHasJoined && PlayerData.PlatformUserInternalId == PlatformUserInternalId)
		{
			PlayerData.bConnected = true;
			PlayerData.InputDeviceInternalId = CurrentDeviceId;

			UpdateDisplayPositions();
			OnSessionPlayersChanged.Broadcast();

			return PlayerData.PlayerNumber;
		}
	}

	// New controllers claim the first unused player number.
	for (int32 PlayerIndex = 0; PlayerIndex < SessionPlayers.Num(); ++PlayerIndex)
	{
		FSessionPlayerData& PlayerData = SessionPlayers[PlayerIndex];

		if (PlayerData.bHasJoined)
		{
			continue;
		}

		PlayerData.bHasJoined = true;
		PlayerData.bConnected = true;
		PlayerData.bReady = false;
		PlayerData.PlatformUserInternalId = PlatformUserInternalId;
		PlayerData.InputDeviceInternalId = CurrentDeviceId;

		for (int32 ColourIndex = 0; ColourIndex < RingColourOptions.Num(); ++ColourIndex)
		{
			if (!IsRingColourTaken(ColourIndex, PlayerData.PlayerNumber))
			{
				PlayerData.RingColourIndex = ColourIndex;
				break;
			}
		}

		UpdateDisplayPositions();
		OnSessionPlayersChanged.Broadcast();

		return PlayerData.PlayerNumber;
	}

	return INDEX_NONE;
}

void UMultiplayerGameInstance::UpdateDisplayPositions()
{
	int32 NextDisplayPosition = 0;

	for (int32 PlayerIndex = 0; PlayerIndex < SessionPlayers.Num(); ++PlayerIndex)
	{
		FSessionPlayerData& PlayerData = SessionPlayers[PlayerIndex];
		PlayerData.DisplayPosition = INDEX_NONE;

		if (PlayerData.bHasJoined && PlayerData.bConnected)
		{
			PlayerData.DisplayPosition = NextDisplayPosition++;
		}
	}
}

int32 UMultiplayerGameInstance::GetJoinedPlayerCount() const
{
	int32 PlayerCount = 0;

	for (int32 PlayerIndex = 0; PlayerIndex < SessionPlayers.Num(); ++PlayerIndex)
	{
		const FSessionPlayerData& PlayerData = SessionPlayers[PlayerIndex];

		if (PlayerData.bHasJoined && PlayerData.bConnected)
		{
			++PlayerCount;
		}
	}

	return PlayerCount;
}

TArray<FSessionPlayerData> UMultiplayerGameInstance::GetSessionPlayers() const
{
	return SessionPlayers;
}

int32 UMultiplayerGameInstance::GetPlayerNumberForPlatformUser(int32 PlatformUserInternalId) const
{
	for (int32 PlayerIndex = 0; PlayerIndex < SessionPlayers.Num(); ++PlayerIndex)
	{
		const FSessionPlayerData& PlayerData = SessionPlayers[PlayerIndex];

		if (PlayerData.bHasJoined && PlayerData.PlatformUserInternalId == PlatformUserInternalId)
		{
			return PlayerData.PlayerNumber;
		}
	}

	return INDEX_NONE;
}

bool UMultiplayerGameInstance::ReadyPlayer(int32 PlayerNumber)
{
	if (!SessionPlayers.IsValidIndex(PlayerNumber))
	{
		return false;
	}

	FSessionPlayerData& PlayerData = SessionPlayers[PlayerNumber];

	if (!PlayerData.bHasJoined || !PlayerData.bConnected || PlayerData.bReady)
	{
		return false;
	}

	PlayerData.bReady = true;
	OnSessionPlayersChanged.Broadcast();

	return true;
}

bool UMultiplayerGameInstance::UnreadyPlayer(int32 PlayerNumber)
{
	if (!SessionPlayers.IsValidIndex(PlayerNumber))
	{
		return false;
	}

	FSessionPlayerData& PlayerData = SessionPlayers[PlayerNumber];

	if (!PlayerData.bHasJoined || !PlayerData.bConnected || !PlayerData.bReady)
	{
		return false;
	}

	PlayerData.bReady = false;
	OnSessionPlayersChanged.Broadcast();

	return true;
}

bool UMultiplayerGameInstance::AreAllJoinedPlayersReady() const
{
	int32 JoinedPlayerCount = 0;

	for (int32 PlayerIndex = 0; PlayerIndex < SessionPlayers.Num(); ++PlayerIndex)
	{
		const FSessionPlayerData& PlayerData = SessionPlayers[PlayerIndex];

		if (!PlayerData.bHasJoined || !PlayerData.bConnected)
		{
			continue;
		}

		++JoinedPlayerCount;

		if (!PlayerData.bReady)
		{
			return false;
		}
	}

	return JoinedPlayerCount > 0;
}

void UMultiplayerGameInstance::MoveCustomisationSelection(int32 PlayerNumber, int32 Direction)
{
	if (!SessionPlayers.IsValidIndex(PlayerNumber))
	{
		return;
	}

	FSessionPlayerData& PlayerData = SessionPlayers[PlayerNumber];

	if (!PlayerData.bHasJoined || !PlayerData.bConnected || PlayerData.bReady)
	{
		return;
	}

	CycleIndexWrapped(PlayerData.SelectedCustomisationRow, 6, Direction);
	OnSessionPlayersChanged.Broadcast();
}

void UMultiplayerGameInstance::CycleCosmetic(int32 PlayerNumber, int32 Direction)
{
	if (!SessionPlayers.IsValidIndex(PlayerNumber) || Direction == 0)
	{
		return;
	}

	FSessionPlayerData& PlayerData = SessionPlayers[PlayerNumber];

	if (!PlayerData.bHasJoined || !PlayerData.bConnected || PlayerData.bReady)
	{
		return;
	}

	switch (PlayerData.SelectedCustomisationRow)
	{
	case 0:
		CycleIndexWrapped(PlayerData.SkinIndex, SkinOptions.Num(), Direction);
		break;

	case 1:
		CycleRingColour(PlayerNumber, Direction);
		return;

	case 2:
		CycleIndexWrapped(PlayerData.HeadwearIndex, HeadwearOptions.Num(), Direction);
		break;

	case 3:
		CycleIndexWrapped(PlayerData.EyewearIndex, EyewearOptions.Num(), Direction);
		break;

	case 4:
		CycleIndexWrapped(PlayerData.BeardIndex, BeardOptions.Num(), Direction);
		break;

	case 5:
		CycleIndexWrapped(PlayerData.ClothingIndex, ClothingOptions.Num(), Direction);
		break;

	default:
		return;
	}

	OnSessionPlayersChanged.Broadcast();
}

FText UMultiplayerGameInstance::GetCustomisationDisplayName(int32 PlayerNumber, int32 CategoryIndex) const
{
	if (!SessionPlayers.IsValidIndex(PlayerNumber) || !SessionPlayers[PlayerNumber].bHasJoined)
	{
		return FText::FromString(TEXT("None"));
	}

	const FSessionPlayerData& PlayerData = SessionPlayers[PlayerNumber];

	switch (CategoryIndex)
	{
	case 0:
		if (SkinOptions.IsValidIndex(PlayerData.SkinIndex))
		{
			const FSkinOption& Option = SkinOptions[PlayerData.SkinIndex];
			return GetOptionName(Option.DisplayName, Option.CosmeticId);
		}
		break;

	case 1:
		if (RingColourOptions.IsValidIndex(PlayerData.RingColourIndex))
		{
			const FBodyColourOption& Option = RingColourOptions[PlayerData.RingColourIndex];
			return GetOptionName(Option.DisplayName, Option.CosmeticId);
		}
		break;

	case 2:
		if (HeadwearOptions.IsValidIndex(PlayerData.HeadwearIndex))
		{
			const FCosmeticMeshOption& Option = HeadwearOptions[PlayerData.HeadwearIndex];
			return GetOptionName(Option.DisplayName, Option.CosmeticId);
		}
		break;

	case 3:
		if (EyewearOptions.IsValidIndex(PlayerData.EyewearIndex))
		{
			const FCosmeticMeshOption& Option = EyewearOptions[PlayerData.EyewearIndex];
			return GetOptionName(Option.DisplayName, Option.CosmeticId);
		}
		break;

	case 4:
		if (BeardOptions.IsValidIndex(PlayerData.BeardIndex))
		{
			const FCosmeticMeshOption& Option = BeardOptions[PlayerData.BeardIndex];
			return GetOptionName(Option.DisplayName, Option.CosmeticId);
		}
		break;

	case 5:
		if (ClothingOptions.IsValidIndex(PlayerData.ClothingIndex))
		{
			const FCosmeticMeshOption& Option = ClothingOptions[PlayerData.ClothingIndex];
			return GetOptionName(Option.DisplayName, Option.CosmeticId);
		}
		break;
	}

	return FText::FromString(TEXT("None"));
}

bool UMultiplayerGameInstance::SetRingColour(int32 PlayerNumber, int32 ColourIndex)
{
	if (!SessionPlayers.IsValidIndex(PlayerNumber) || !RingColourOptions.IsValidIndex(ColourIndex))
	{
		return false;
	}

	FSessionPlayerData& PlayerData = SessionPlayers[PlayerNumber];

	if (!PlayerData.bHasJoined || !PlayerData.bConnected || PlayerData.bReady || IsRingColourTaken(ColourIndex, PlayerNumber))
	{
		return false;
	}

	PlayerData.RingColourIndex = ColourIndex;
	OnSessionPlayersChanged.Broadcast();

	return true;
}

void UMultiplayerGameInstance::CycleRingColour(int32 PlayerNumber, int32 Direction)
{
	if (!SessionPlayers.IsValidIndex(PlayerNumber) || RingColourOptions.IsEmpty() || Direction == 0)
	{
		return;
	}

	FSessionPlayerData& PlayerData = SessionPlayers[PlayerNumber];

	if (!PlayerData.bHasJoined || !PlayerData.bConnected || PlayerData.bReady)
	{
		return;
	}

	int32 CandidateColour = PlayerData.RingColourIndex;

	// Keep cycling until an unused colour is found.
	for (int32 Attempt = 0; Attempt < RingColourOptions.Num(); ++Attempt)
	{
		CycleIndexWrapped(CandidateColour, RingColourOptions.Num(), Direction);

		if (!IsRingColourTaken(CandidateColour, PlayerNumber))
		{
			PlayerData.RingColourIndex = CandidateColour;
			OnSessionPlayersChanged.Broadcast();
			return;
		}
	}
}

bool UMultiplayerGameInstance::IsRingColourTaken(int32 ColourIndex, int32 IgnorePlayerNumber) const
{
	for (int32 PlayerIndex = 0; PlayerIndex < SessionPlayers.Num(); ++PlayerIndex)
	{
		const FSessionPlayerData& PlayerData = SessionPlayers[PlayerIndex];

		if (!PlayerData.bHasJoined || PlayerData.PlayerNumber == IgnorePlayerNumber)
		{
			continue;
		}

		if (PlayerData.RingColourIndex == ColourIndex)
		{
			return true;
		}
	}

	return false;
}

int32 UMultiplayerGameInstance::GetRingColourIndex(int32 PlayerNumber) const
{
	if (!SessionPlayers.IsValidIndex(PlayerNumber))
	{
		return INDEX_NONE;
	}

	return SessionPlayers[PlayerNumber].RingColourIndex;
}

FLinearColor UMultiplayerGameInstance::GetRingColourForPlayer(int32 PlayerNumber) const
{
	if (RingColourOptions.IsEmpty())
	{
		return FLinearColor::White;
	}

	if (SessionPlayers.IsValidIndex(PlayerNumber))
	{
		const FSessionPlayerData& PlayerData = SessionPlayers[PlayerNumber];

		if (RingColourOptions.IsValidIndex(PlayerData.RingColourIndex))
		{
			return RingColourOptions[PlayerData.RingColourIndex].Colour;
		}
	}

	// Fallback for testing without a normal session.
	const int32 FallbackIndex = FMath::Max(PlayerNumber, 0) % RingColourOptions.Num();
	return RingColourOptions[FallbackIndex].Colour;
}

int32 UMultiplayerGameInstance::GetPlayerNumberForController(const AController* Controller) const
{
	const APlayerController* PlayerController = Cast<APlayerController>(Controller);

	if (!PlayerController)
	{
		return INDEX_NONE;
	}

	const FPlatformUserId PlatformUserId = PlayerController->GetPlatformUserId();

	if (PlatformUserId.IsValid())
	{
		const int32 PlayerNumber = GetPlayerNumberForPlatformUser(PlatformUserId.GetInternalId());

		if (PlayerNumber != INDEX_NONE)
		{
			return PlayerNumber;
		}
	}

	const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();

	if (!LocalPlayer)
	{
		return INDEX_NONE;
	}

	const int32 LocalPlayerIndex = GetLocalPlayers().IndexOfByKey(LocalPlayer);

	if (LocalPlayerIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	for (int32 PlayerIndex = 0; PlayerIndex < SessionPlayers.Num(); ++PlayerIndex)
	{
		const FSessionPlayerData& PlayerData = SessionPlayers[PlayerIndex];

		if (PlayerData.bHasJoined && PlayerData.bConnected && PlayerData.DisplayPosition == LocalPlayerIndex)
		{
			return PlayerData.PlayerNumber;
		}
	}

	return INDEX_NONE;
}

void UMultiplayerGameInstance::ResetSession()
{
	SessionPlayers.Empty();

	for (int32 PlayerNumber = 0; PlayerNumber < 4; ++PlayerNumber)
	{
		FSessionPlayerData NewPlayer;
		NewPlayer.PlayerNumber = PlayerNumber;
		SessionPlayers.Add(NewPlayer);
	}

	OnSessionPlayersChanged.Broadcast();
}