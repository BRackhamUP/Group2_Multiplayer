// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MultiplayerGameInstance.generated.h"

/*
holds the data needed to remmeber each player during the game session

data is stored in the game instance rather than individual characters, characters/controllers can be destroyed and recreated during level changes

one of these structs is created for each player
*/
USTRUCT(BlueprintType)
struct FSessionPlayerData
{
	GENERATED_BODY()

	// own player number/slot, 0=player1, 1=player2 so on...
	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 PlayerSlot = INDEX_NONE;

	// bool for if 'this' player has readied up
	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	bool bReady = false;

	// cureent temporary cosmetic choice for the lobby
	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 CosmeticIndex = 0;

	//unreal ID for the user that owns this player physical controller
	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 PlatformUserInternalId = INDEX_NONE;

	// unreal ID for the actual physica; input device
	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 InputDeviceInternalId = INDEX_NONE;
};

// event broadcast whenever the player data changes
// used in widgets for refreshing
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSessionPlayersChanged);

/*
game instance used to manage the local multiplayer session

game instance survives between levels so it is  used to keep track of, 
connected controllers, which player owns each controller, ready up state, temporary cosmetic changes

also handles creating/removing local players when the controllers connect/disconenct
*/
UCLASS()
class GROUP2_MULTIPLAYER_API UMultiplayerGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	// check connected controller and rebuild the lobby players if anything has changed
	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Lobby")
	void SynchroniseLobbyPlayers();

	// returns how many valid gamepads unreal currently detects with a max of 4
	UFUNCTION(BlueprintPure, Category = "Multiplayer|Lobby")
	int32 GetConnectedGamepadCount() const;

	// number of players currently stored in session
	UFUNCTION(BlueprintPure, Category = "Multiplayer|Lobby")
	int32 GetSessionPlayerCount() const;

	// for giving blueprints a copy of the current session player info
	UFUNCTION(BlueprintPure, Category = "Multiplayer|Lobby")
	TArray<FSessionPlayerData> GetSessionPlayers() const;

	// find the session player slot unsing cntrollers platform user ID
	UFUNCTION(BlueprintPure, Category = "Multiplayer|Lobby")
	int32 GetPlayerSlotForPlatformUser(int32 PlatformUserInternalId) const;

	// marking one player session data as ready
	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Lobby")
	bool ReadyPlayer(int32 PlayerSlot);

	// returns true only if every player currently in session is ready
	UFUNCTION(BlueprintPure, Category = "Multiplayer|Lobby")
	bool AreAllSessionPlayersReady() const;

	// move forward/baclward through the temporary cosmetic choices
	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Customisation")
	void CycleCosmetic(int32 PlayerSlot, int32 Direction);

	// return how many cosmetic options cureently exist
	UFUNCTION(BlueprintPure, Category = "Multiplayer|Customisation")
	int32 GetCosmeticOptionCount() const;

	// reset ready/cosmetic state without destroying the sesoin player entry
	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Session")
	void ResetSession();

	// widget binding for refreshing when the session data changes
	UPROPERTY(BlueprintAssignable, Category = "Multiplayer|Events")
	FOnSessionPlayersChanged OnSessionPlayersChanged;

protected:

	// list containign current players participating in the sesion
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Multiplayer|Session")
	TArray<FSessionPlayerData> SessionPlayers;

	// num of temporary cosmetic options
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Multiplayer|Customisation", meta = (ClampMin = "1"))
	int32 CosmeticOptionCount = 4;


private:

	// detect valid connected gamepads and get the device ID and platform user ID
	void GetConnectedGamepadMappings(TArray<FInputDeviceId>& OutDeviceIds, TArray<FPlatformUserId>& OutPlatformUsers) const;

	// check whether the currently detected controllers already matched to the existing session
	bool DoesSessionMatchDetectedGamepads(const TArray<FInputDeviceId>& DeviceIds, const TArray<FPlatformUserId>& PlatformUsers) const;

	// rebuild local players and session data after controllers connect/disconenct
	void RebuildLocalPlayersAndSession(const TArray<FInputDeviceId>& DeviceIds, const TArray<FPlatformUserId>& PlatformUsers);
};