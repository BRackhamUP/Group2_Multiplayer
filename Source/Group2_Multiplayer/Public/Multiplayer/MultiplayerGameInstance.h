#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MultiplayerGameInstance.generated.h"

class UStaticMesh;
class UMaterialInterface;
class AController;

// multiplayer game instance is repsonsible for handling the data associated with the connected players
// declaring collections of structs needed for all cosmetic items and player data

// using structs of blueprint type so the designers have easy access through the derrived BP in engine 
USTRUCT(BlueprintType)
struct FSkinOption
{
	GENERATED_BODY()

	// cosmetic ID name is used to represent the actual name of the item in engine eg: SM_CowboyHat1... or whatever
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customisation")
	FName CosmeticId = NAME_None;

	// display name is the name that will appear on the in game build a spud screen
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customisation")
	FText DisplayName;

	// custom materials have been created by the srtists so the skin type needs to be a material pointer
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customisation")
	TObjectPtr<UMaterialInterface> Material = nullptr;
};

// same struct logic and set up with ID, in game display name except that actual cosmetic meshes get an additional RelativeTransform property
USTRUCT(BlueprintType)
struct FCosmeticMeshOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customisation")
	FName CosmeticId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customisation")
	FText DisplayName;

	// pointer to the static mesh, easy assignment in the BP for designer facing control
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customisation")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	// relative transform additions to the skeletal mesh cosmetics struct so designers can easily adjust/tweak how they appear on the character
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customisation")
	FTransform RelativeTransform = FTransform::Identity;
};

// session player data invovles storing the data relavent to cosmetic changes per player,
// their current state eg: if ready/connected etc.. 
// and their internal ID assignemnts by unreal for handling conneciton/disconnection logic
USTRUCT(BlueprintType)
struct FSessionPlayerData
{
	GENERATED_BODY()

	// playerner number and idsplay position are used for in game assignment 
	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 PlayerNumber = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 DisplayPosition = INDEX_NONE;

	// player states used to determine in game logic
	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	bool bHasJoined = false;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	bool bConnected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	bool bReady = false;

	// all cosmetic indexes relevant to the specific players cosmetic choices
	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 SkinIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 RingColourIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 HeadwearIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 EyewearIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 BeardIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 ClothingIndex = 0;

	// used for determining which cutomisation row the player is currently cycling through
	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 SelectedCustomisationRow = 0;

	// unreal detection of gamepads assigns useable ID's to the controllers, 
	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 PlatformUserInternalId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 InputDeviceInternalId = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct FBodyColourOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customisation")
	FName CosmeticId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customisation")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customisation")
	FLinearColor Colour = FLinearColor::White;
};

//https://dev.epicgames.com/documentation/unreal-engine/multicast-delegates-in-unreal-engine
// this super duper method is allowing the ability to attach multiple delgate methods which can all be executed at once
// specifically by using the Broadcast() method, used frequently to handle player connection/disconnection logic
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSessionPlayersChanged);

// colleciton of methods used for obtaining and using player information within the game instance
UCLASS()
class GROUP2_MULTIPLAYER_API UMultiplayerGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:

#pragma region player session

	// after many refactors of the multiplayer logic, I'm not sure if this is being used in BP by designers for UI stuff or not... best to keept to be safe for now
	UFUNCTION(BlueprintPure, Category = "Multiplayer|Players")
	int32 GetPlayerNumberAtDisplayPosition(int32 DisplayPosition) const;

	// method for syncronising contorllers with session players
	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Controllers")
	void SynchroniseControllers();

	// method for additional players joining the lobby
	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Players")
	int32 JoinPlayer(int32 PlatformUserInternalId);

	// simple getter methods
	UFUNCTION(BlueprintPure, Category = "Multiplayer|Players")
	int32 GetJoinedPlayerCount() const;

	UFUNCTION(BlueprintPure, Category = "Multiplayer|Players")
	TArray<FSessionPlayerData> GetSessionPlayers() const;

	UFUNCTION(BlueprintPure, Category = "Multiplayer|Players")
	int32 GetPlayerNumberForPlatformUser(int32 PlatformUserInternalId) const;

	UFUNCTION(BlueprintPure, Category = "Multiplayer|Players")
	int32 GetPlayerNumberForController(const AController* Controller) const;

#pragma endregion

#pragma region ready state system

	// state check booleans for player lobby and game state
	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Ready")
	bool ReadyPlayer(int32 PlayerNumber);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Ready")
	bool UnreadyPlayer(int32 PlayerNumber);

	UFUNCTION(BlueprintPure, Category = "Multiplayer|Ready")
	bool AreAllJoinedPlayersReady() const;

#pragma endregion

#pragma region customisation

	// methods used for navigating the cosmetic menu
	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Customisation")
	void MoveCustomisationSelection(int32 PlayerNumber, int32 Direction);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Customisation")
	void CycleCosmetic(int32 PlayerNumber, int32 Direction);

	UFUNCTION(BlueprintPure, Category = "Multiplayer|Customisation")
	FText GetCustomisationDisplayName(int32 PlayerNumber, int32 CategoryIndex) const;

#pragma endregion

#pragma region ring customisaiton

	// methods created by Luke and used for setting, cycling and handling the ring cosmetics in game
	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Customisation|Ring")
	bool SetRingColour(int32 PlayerNumber, int32 ColourIndex);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Customisation|Ring")
	void CycleRingColour(int32 PlayerNumber, int32 Direction);

	UFUNCTION(BlueprintPure, Category = "Multiplayer|Customisation|Ring")
	bool IsRingColourTaken(int32 ColourIndex, int32 IgnorePlayerNumber = -1) const;

	UFUNCTION(BlueprintPure, Category = "Multiplayer|Customisation|Ring")
	int32 GetRingColourIndex(int32 PlayerNumber) const;

	UFUNCTION(BlueprintPure, Category = "Multiplayer|Customisation|Ring")
	FLinearColor GetRingColourForPlayer(int32 PlayerNumber) const;

#pragma endregion

	// resetiing the session data 
	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Session")
	void ResetSession();

	// creating the container arrays of the cosmetic structs,
	// used by the designers in BP to populate the arrays with items
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Multiplayer|Customisation|Options")
	TArray<FSkinOption> SkinOptions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Multiplayer|Customisation|Options")
	TArray<FBodyColourOption> RingColourOptions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Multiplayer|Customisation|Options")
	TArray<FCosmeticMeshOption> HeadwearOptions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Multiplayer|Customisation|Options")
	TArray<FCosmeticMeshOption> EyewearOptions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Multiplayer|Customisation|Options")
	TArray<FCosmeticMeshOption> BeardOptions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Multiplayer|Customisation|Options")
	TArray<FCosmeticMeshOption> ClothingOptions;

	// copy of the spud shops purchased item flags
	// these unlocks are shared between all players in the session
	// index 0-4 = Headwear, 5-9 = Eyewear 10-14 = Beard, 15-19 = Clothing.
	UPROPERTY(BlueprintReadWrite, Category = "Multiplayer|Customisation|Shop")
	TArray<bool> PurchasedShopCosmetics;

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer|Events")
	FOnSessionPlayersChanged OnSessionPlayersChanged;

protected:
	// array containing the current session players
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Multiplayer|Session")
	TArray<FSessionPlayerData> SessionPlayers;

private:

	// internal helper methodss
	void GetConnectedGamepadMappings(TArray<FInputDeviceId>& OutDeviceIds, TArray<FPlatformUserId>& OutPlatformUsers) const;
	int32 FindConnectedControllerIndex(const TArray<FPlatformUserId>& PlatformUsers, int32 PlatformUserInternalId) const;
	void RebuildLocalPlayersIfNeeded(const TArray<FPlatformUserId>& OrderedPlatformUsers);
	bool CanCustomisePlayer(int32 PlayerNumber) const;

	// cycles through a cosmetic category while skipping locked items
	void CycleAvailableCosmetic(int32& CurrentIndex, int32 OptionCount, int32 CategoryIndex, int32 Direction);
};