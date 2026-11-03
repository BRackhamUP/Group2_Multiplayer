#include "Multiplayer/MultiplayerGameInstance.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/InputDeviceLibrary.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/PlayerController.h"

#pragma region helper methods used throughout logic methods

// reuseable method for cycling through cosmetics on the build a spud menu
// when the index moves past either end of the array itl just wrap around to the opposite end
static void CycleIndexWrapped(int32& Index, int32 OptionCount, int32 Direction)
{
	/// when there isnt an available option just return deafult
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

// returns the option name for the UI to update in customisaiotn screen
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

	// display as none if no name is given
	return FText::FromString(TEXT("None"));
}

#pragma endregion

#pragma region controller detection and localplauyer management

// check controllers exist by lookin through the referenced input devices and platform ids
void UMultiplayerGameInstance::GetConnectedGamepadMappings(TArray<FInputDeviceId>& OutDeviceIds, TArray<FPlatformUserId>& OutPlatformUsers) const
{
	// ensure the arrasy are empty at first
	OutDeviceIds.Empty();
	OutPlatformUsers.Empty();

	// populate the input device array with currently connected input devices
	// https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/GameFramework/UInputDeviceLibrary/GetAllConnectedInputDevices?application_version=5.5
	TArray<FInputDeviceId> ConnectedDevices;
	UInputDeviceLibrary::GetAllConnectedInputDevices(ConnectedDevices);

	if (!GEngine)
	{
		return;
	}
	// the input device subsystem give information about what type of harware each devie is
	UInputDeviceSubsystem* InputDeviceSubsystem = GEngine->GetEngineSubsystem<UInputDeviceSubsystem>();

	if (!InputDeviceSubsystem)
	{
		return;
	}

	// do this for every connected evice
	for (int32 DeviceIndex = 0; DeviceIndex < ConnectedDevices.Num(); ++DeviceIndex)
	{
		// get the current device id and its data to check which type of device it is
		const FInputDeviceId DeviceId = ConnectedDevices[DeviceIndex];
		const FHardwareDeviceIdentifier HardwareDevice = InputDeviceSubsystem->GetInputDeviceHardwareIdentifier(DeviceId);

		// ignore mouse and keyboard
		if (HardwareDevice.PrimaryDeviceType != EHardwareDevicePrimaryType::Gamepad)
		{
			continue;
		}
		// now find the platform user that unreal assigned to this device id
		const FPlatformUserId PlatformUserId = UInputDeviceLibrary::GetUserForInputDevice(DeviceId);

		// ignore devices without valid platformuserid
		if (!PlatformUserId.IsValid())
		{
			continue;
		}
		bool bAlreadyAdded = false;

		// looop to check the platform user only appears once
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

		// store the matching devices at the same array index 
		OutDeviceIds.Add(DeviceId);
		OutPlatformUsers.Add(PlatformUserId);

		// lock the amount of platform user to 4
		if (OutPlatformUsers.Num() >= 4)
		{
			break;
		}
	}
}

// which detected controller matches the platform user
int32 UMultiplayerGameInstance::FindConnectedControllerIndex(const TArray<FPlatformUserId>& PlatformUsers, int32 PlatformUserInternalId) const
{
	for (int32 ControllerIndex = 0; ControllerIndex < PlatformUsers.Num(); ++ControllerIndex)
	{
		if (PlatformUsers[ControllerIndex].GetInternalId() == PlatformUserInternalId)
		{
			// loop through the controller indexes to reutnr the matching index that is shared by the device id and platform user array
			return ControllerIndex;
		}
	}
	// return noone for when the ocntorller disconencts
	return INDEX_NONE;
}

// make unreal loacalplayer order match the player order stored in the session
// ensuring after conneciton/disconneciton that the physical controlelr continues to control the correct player slot in game
void UMultiplayerGameInstance::RebuildLocalPlayersIfNeeded(const TArray<FPlatformUserId>& OrderedPlatformUsers)
{
	// unreal keeps at least one local player even if no additiional contorller are connected
	const int32 RequiredLocalPlayerCount = FMath::Max(1, OrderedPlatformUsers.Num());

	// rebuild if unrel has the wrong number of locaplayers
	bool bNeedToRebuild = GetNumLocalPlayers() != RequiredLocalPlayerCount;

	// the count may be correct while the controller order is wrong
	if (!bNeedToRebuild)
	{
		// compare each localplayer against the platform user expected for that slot
		for (int32 PlayerIndex = 0; PlayerIndex < OrderedPlatformUsers.Num(); ++PlayerIndex)
		{
			ULocalPlayer* LocalPlayerAtIndex = GetLocalPlayerByIndex(PlayerIndex);
			ULocalPlayer* ExpectedLocalPlayer = FindLocalPlayerFromPlatformUserId(OrderedPlatformUsers[PlayerIndex]);

			if (LocalPlayerAtIndex != ExpectedLocalPlayer)
			{
				bNeedToRebuild = true;
				break;
			}
		}
	}
	// if local player matches expected then no need to rebuild
	if (!bNeedToRebuild)
	{
		return;
	}

	// unreal holds the first local player so keep it and remove the secondary localpplayer -1
	while (GetNumLocalPlayers() > 1)
	{
		ULocalPlayer* PlayerToRemove = GetLocalPlayerByIndex(GetNumLocalPlayers() - 1);

		if (!PlayerToRemove)
		{
			break;
		}
		RemoveLocalPlayer(PlayerToRemove);
	}

	// if there are no connected platform users then there is nothing to rebuild 
	if (OrderedPlatformUsers.IsEmpty())
	{
		return;
	}

	// reasign unreals "player1" to the player who now owns player 1
	ULocalPlayer* FirstLocalPlayer = GetLocalPlayerByIndex(0);
	if (FirstLocalPlayer)
	{
		FirstLocalPlayer->SetPlatformUserId(OrderedPlatformUsers[0]);
	}

	// recreate the remaining localplayers in sessino order
	for (int32 PlayerIndex = 1; PlayerIndex < OrderedPlatformUsers.Num(); ++PlayerIndex)
	{
		//https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UGameInstance/CreateLocalPlayer
		//create new unreal localplaeyr for this platform user, false prevents unreal from automatically spawning the default playercontroller, out error can store an error message if it fails
		FString OutError;
		ULocalPlayer* NewLocalPlayer = CreateLocalPlayer(OrderedPlatformUsers[PlayerIndex], OutError, false);

		if (!NewLocalPlayer)
		{
			continue;
		}

		FString SpawnError;
		// create the playercontroller belonging to 'this' new localplayer
		NewLocalPlayer->SpawnPlayActor(TEXT(""), SpawnError, GetWorld());
	}
}

// synchronise the physical gamepads, session player slots and unreal localplayers
// remove dicsconnected player and shift the remaining players to fill empty slots also reordering the localplayers so controller wnership matches
void UMultiplayerGameInstance::SynchroniseControllers()
{
	// retrieve the ids and platform users
	TArray<FInputDeviceId> DeviceIds;
	TArray<FPlatformUserId> PlatformUsers;

	GetConnectedGamepadMappings(DeviceIds, PlatformUsers);

	bool bSessionChanged = false;

	// create array for players still connectd
	TArray<FSessionPlayerData> ConnectedPlayers;

	// build the new player list containign only joined players whose controllers still exist
	for (int32 PlayerIndex = 0; PlayerIndex < SessionPlayers.Num(); ++PlayerIndex)
	{
		const FSessionPlayerData& ExistingPlayer = SessionPlayers[PlayerIndex];
		// empty slots do not need to be copied
		if (!ExistingPlayer.bHasJoined)
		{
			continue;
		}
		// check the controller that owns this session player is still conencted
		const int32 ConnectedControllerIndex = FindConnectedControllerIndex(PlatformUsers, ExistingPlayer.PlatformUserInternalId);

		// if the contorller disconnects then do not copy into the session
		if (ConnectedControllerIndex == INDEX_NONE)
		{
			bSessionChanged = true;
			continue;
		}

		// copy the exsiting players data into this survinig player container
		// this is to preserve their cosmetic choices when syncronising other disconnected controllers
		FSessionPlayerData SurvivingPlayer = ExistingPlayer;

		// add the new player slot to the next positon in the array
		const int32 NewPlayerNumber = ConnectedPlayers.Num();
		const int32 CurrentDeviceId = DeviceIds[ConnectedControllerIndex].GetId();

		// check whether any of the session player assignment data changed so BP's that use it can refresh
		if (SurvivingPlayer.PlayerNumber != NewPlayerNumber || SurvivingPlayer.DisplayPosition != NewPlayerNumber || SurvivingPlayer.InputDeviceInternalId != CurrentDeviceId || !SurvivingPlayer.bConnected)
		{
			bSessionChanged = true;
		}
		// update the players session position and input device
		SurvivingPlayer.PlayerNumber = NewPlayerNumber;
		SurvivingPlayer.DisplayPosition = NewPlayerNumber;
		SurvivingPlayer.bConnected = true;
		SurvivingPlayer.InputDeviceInternalId = CurrentDeviceId;
		ConnectedPlayers.Add(SurvivingPlayer);
	}

	// always keep the four session entrys available and unused positions reset for fresh player data
	while (ConnectedPlayers.Num() < 4)
	{
		FSessionPlayerData EmptyPlayer;
		EmptyPlayer.PlayerNumber = ConnectedPlayers.Num();
		ConnectedPlayers.Add(EmptyPlayer);
	}

	// replace the old session list with the newly connected player order
	SessionPlayers = ConnectedPlayers;

	// build the controlle order unreal needs to use
	// this is for the buildaspud cosmetic to funciton properly as technically player 4 can join the lobby at player 2 slot
	// so need to update unreal to match the session order
	TArray<FPlatformUserId> OrderedPlatformUsers;

	for (int32 PlayerIndex = 0; PlayerIndex < SessionPlayers.Num(); ++PlayerIndex)
	{
		const FSessionPlayerData& PlayerData = SessionPlayers[PlayerIndex];
		// check player data for if the player is connected or joined the lobby
		if (!PlayerData.bHasJoined || !PlayerData.bConnected)
		{
			continue;
		}

		// find the connected controllers and check if they have an index
		const int32 ControllerIndex = FindConnectedControllerIndex(PlatformUsers, PlayerData.PlatformUserInternalId);
		if (ControllerIndex != INDEX_NONE)
		{
			// add them to the order
			OrderedPlatformUsers.Add(PlatformUsers[ControllerIndex]);
		}
	}

	// Connected controllers which have not joined go at the end.
	for (int32 ControllerIndex = 0; ControllerIndex < PlatformUsers.Num(); ++ControllerIndex)
	{
		if (!OrderedPlatformUsers.Contains(PlatformUsers[ControllerIndex]))
		{
			OrderedPlatformUsers.Add(PlatformUsers[ControllerIndex]);
		}
	}
	// utilise rebuild to make unreal localplayer/controller order match this session order
	RebuildLocalPlayersIfNeeded(OrderedPlatformUsers);

	// notify ui and other listening method whenever session player data actually changes
	if (bSessionChanged)
	{
		OnSessionPlayersChanged.Broadcast();
	}
}

#pragma endregion

#pragma region player management in session

// adds a connected controller to the first available session player slot
int32 UMultiplayerGameInstance::JoinPlayer(int32 PlatformUserInternalId)
{
	// only designe dfor exactly four available player slots
	if (SessionPlayers.Num() != 4)
	{
		return INDEX_NONE;
	}

	// get the physical controller list and ensure 'this' platform user is still connected
	TArray<FInputDeviceId> DeviceIds;
	TArray<FPlatformUserId> PlatformUsers;

	GetConnectedGamepadMappings(DeviceIds, PlatformUsers);

	const int32 ControllerIndex = FindConnectedControllerIndex(PlatformUsers, PlatformUserInternalId);

	if (ControllerIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	// obviously dont allow already joined controllers to join twice
	for (int32 PlayerIndex = 0; PlayerIndex < SessionPlayers.Num(); ++PlayerIndex)
	{
		const FSessionPlayerData& PlayerData = SessionPlayers[PlayerIndex];

		if (PlayerData.bHasJoined && PlayerData.PlatformUserInternalId == PlatformUserInternalId)
		{
			return PlayerData.PlayerNumber;
		}
	}

	// search from player 1 onwards so that new players always take the first available slot
	for (int32 PlayerIndex = 0; PlayerIndex < SessionPlayers.Num(); ++PlayerIndex)
	{
		if (SessionPlayers[PlayerIndex].bHasJoined)
		{
			continue;
		}
		// start with an instance of fresh struct playerdata so reconnecting controllers does not recover old states (was a pain in testing :( )
		FSessionPlayerData NewPlayer;

		// store the unresl id that identify the physcial pad currently owning 'this' player
		NewPlayer.PlayerNumber = PlayerIndex;
		NewPlayer.DisplayPosition = PlayerIndex;

		NewPlayer.bHasJoined = true;
		NewPlayer.bConnected = true;
		NewPlayer.bReady = false;

		NewPlayer.PlatformUserInternalId = PlatformUserInternalId;
		NewPlayer.InputDeviceInternalId = DeviceIds[ControllerIndex].GetId();

		// to esnrue no duplicate ring colours, give the first unused ring to the new player
		for (int32 ColourIndex = 0; ColourIndex < RingColourOptions.Num(); ++ColourIndex)
		{
			if (!IsRingColourTaken(ColourIndex, PlayerIndex))
			{
				NewPlayer.RingColourIndex = ColourIndex;
				break;
			}
		}
		// store the complete playerdata and notify via the broadcast for any UI or method updates
		SessionPlayers[PlayerIndex] = NewPlayer;
		OnSessionPlayersChanged.Broadcast();

		return PlayerIndex;
	}

	return INDEX_NONE;
}

// return the number of session players that have joined and currently have a connected controller
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

// return the current four player session data for use by blueprints and stuffff
TArray<FSessionPlayerData> UMultiplayerGameInstance::GetSessionPlayers() const
{
	return SessionPlayers;
}

// find which session player currently has a build a spud display slot
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
	// return default when no one is using the position
	return INDEX_NONE;
}

// fidn current sesion player owned by a specifci unreal platformuserID
// this is so the contorller id can survive the player slot positon changes after disconneciton
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

// finds the current sessoin player belonging to a player controller
int32 UMultiplayerGameInstance::GetPlayerNumberForController(const AController* Controller) const
{
	const APlayerController* PlayerController = Cast<APlayerController>(Controller);

	if (!PlayerController)
	{
		return INDEX_NONE;
	}

	// look to see if the controllers platformuserid matches the session data
	const FPlatformUserId PlatformUserId = PlayerController->GetPlatformUserId();
	if (PlatformUserId.IsValid())
	{
		const int32 PlayerNumber = GetPlayerNumberForPlatformUser(PlatformUserId.GetInternalId());

		if (PlayerNumber != INDEX_NONE)
		{
			return PlayerNumber;
		}
	}

	// use this controller current positon in unreal localplayer array if cant use platformuserid
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

	// match the localplayer position against the players current display/session ordering
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

// clear all exisiting session data and recreate four fresh player positons
void UMultiplayerGameInstance::ResetSession()
{
	SessionPlayers.Empty();
	// assign each empty its array positon and use the defaults for struct values
	for (int32 PlayerNumber = 0; PlayerNumber < 4; ++PlayerNumber)
	{
		FSessionPlayerData NewPlayer;
		NewPlayer.PlayerNumber = PlayerNumber;
		SessionPlayers.Add(NewPlayer);
	}
	OnSessionPlayersChanged.Broadcast();
}

#pragma endregion

#pragma region ready state check

// methods for changin player ready states and checking all joined players are ready
bool UMultiplayerGameInstance::ReadyPlayer(int32 PlayerNumber)
{
	// only valid players can change to ready state
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
	// has to be a joined player for the result to ever be true
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

#pragma endregion

#pragma region	customisation navigation

// helper method for validating a player can qualify to customise their player
bool UMultiplayerGameInstance::CanCustomisePlayer(int32 PlayerNumber) const
{
	if (!SessionPlayers.IsValidIndex(PlayerNumber))
	{
		return false;
	}
	const FSessionPlayerData& PlayerData = SessionPlayers[PlayerNumber];
	return PlayerData.bHasJoined && PlayerData.bConnected && !PlayerData.bReady;
}

// method for moving the players customisaiton selection up and down theough the categories
void UMultiplayerGameInstance::MoveCustomisationSelection(int32 PlayerNumber, int32 Direction)
{
	if (!CanCustomisePlayer(PlayerNumber))
	{
		return;
	}

	FSessionPlayerData& PlayerData = SessionPlayers[PlayerNumber];
	CycleIndexWrapped(PlayerData.SelectedCustomisationRow, 6, Direction);
	OnSessionPlayersChanged.Broadcast();
}

// utilise the cycle through index method to go through the cosmetic items currently present in that array
void UMultiplayerGameInstance::CycleCosmetic(int32 PlayerNumber, int32 Direction)
{
	if (!CanCustomisePlayer(PlayerNumber) || Direction == 0)
	{
		return;
	}

	FSessionPlayerData& PlayerData = SessionPlayers[PlayerNumber];

	// selectedcustomisationrow is determining which cosmetic index should be changed
	switch (PlayerData.SelectedCustomisationRow)
	{
	case 0: // skin colour
		CycleIndexWrapped(PlayerData.SkinIndex, SkinOptions.Num(), Direction);
		break;

	case 1: // ring colour uses different cycling logic because two players cannot have the same ring
		CycleRingColour(PlayerNumber, Direction);
		return;

	case 2: // headwear
		CycleIndexWrapped(PlayerData.HeadwearIndex, HeadwearOptions.Num(), Direction);
		break;

	case 3: // eyewear
		CycleIndexWrapped(PlayerData.EyewearIndex, EyewearOptions.Num(), Direction);
		break;

	case 4: // beard
		CycleIndexWrapped(PlayerData.BeardIndex, BeardOptions.Num(), Direction);
		break;

	case 5: // clothing
		CycleIndexWrapped(PlayerData.ClothingIndex, ClothingOptions.Num(), Direction);
		break;

	default:
		return;
	}
	OnSessionPlayersChanged.Broadcast();
}

#pragma endregion

#pragma region ring colour customisation

// attempt to assign a specific ring colour to a player
bool UMultiplayerGameInstance::SetRingColour(int32 PlayerNumber, int32 ColourIndex)
{
	// checking the player can customise, the colour is valid and is not taken already
	if (!CanCustomisePlayer(PlayerNumber) || !RingColourOptions.IsValidIndex(ColourIndex) || IsRingColourTaken(ColourIndex, PlayerNumber))
	{
		return false;
	}

	SessionPlayers[PlayerNumber].RingColourIndex = ColourIndex;
	OnSessionPlayersChanged.Broadcast();
	return true;
}

// cycle through ring colours until an uunsed option is found
void UMultiplayerGameInstance::CycleRingColour(int32 PlayerNumber, int32 Direction)
{
	if (!CanCustomisePlayer(PlayerNumber) || RingColourOptions.IsEmpty() || Direction == 0)
	{
		return;
	}

	FSessionPlayerData& PlayerData = SessionPlayers[PlayerNumber];
	// use a temporary index so the player colour doesnt change unless a valid option is found
	int32 CandidateColour = PlayerData.RingColourIndex;

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

// check whether the ring colour is currently owned by another joined player
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

// return the ring colour index for a player
int32 UMultiplayerGameInstance::GetRingColourIndex(int32 PlayerNumber) const
{
	if (!SessionPlayers.IsValidIndex(PlayerNumber))
	{
		return INDEX_NONE;
	}

	return SessionPlayers[PlayerNumber].RingColourIndex;
}

// convert eh ring colour index into an actual colour stored in the in game array of colours
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

	// fallback used for testing, not in a multiplayer session
	const int32 FallbackIndex = FMath::Max(PlayerNumber, 0) % RingColourOptions.Num();
	return RingColourOptions[FallbackIndex].Colour;
}

#pragma endregion

// return the name shown in the build a spud UI for the players selected cosmetics
FText UMultiplayerGameInstance::GetCustomisationDisplayName(int32 PlayerNumber, int32 CategoryIndex) const
{
	// unused player slots have no cosmetic name to display
	if (!SessionPlayers.IsValidIndex(PlayerNumber) || !SessionPlayers[PlayerNumber].bHasJoined)
	{
		return FText::FromString(TEXT("None"));
	}
	// access current players stored cosmetic selections
	const FSessionPlayerData& PlayerData = SessionPlayers[PlayerNumber];
	
	// find the correct cosmetic array by useing the category index
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
