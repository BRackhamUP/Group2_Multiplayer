#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MultiplayerGameInstance.generated.h"


class UStaticMesh;
class AController;

/*
one available body colour

CosmeticId is included so a future unlock/save system has a stable
identifier to work with without changing the current array structure
*/
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


/*
one available mesh cosmetic

RelativeTransform allows designers to adjust an item's position,
rotation and scale without changing code
*/
USTRUCT(BlueprintType)
struct FCosmeticMeshOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customisation")
	FName CosmeticId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customisation")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customisation")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customisation")
	FTransform RelativeTransform = FTransform::Identity;
};

/*
holds the data needed to remember each player during the game session

this lives in the GameInstance rather than the player character because
characters/controllers are destroyed and recreated during level changes

one struct exists for each participating player
*/
USTRUCT(BlueprintType)
struct FSessionPlayerData
{
	GENERATED_BODY()

	// zero based - 0 = player 1, 1 = player 2 etc...
	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 PlayerSlot = INDEX_NONE;

	// whether this player has readied up
	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	bool bReady = false;


	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 CosmeticIndex = 0;

	// unique per player, two players can never have the same ring colour
	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 RingColourIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 HeadCosmeticIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 ChestCosmeticIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 LegsCosmeticIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 BackCosmeticIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 SelectedCustomisationRow = 0;

	// unreal user that owns this player's controller
	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 PlatformUserInternalId = INDEX_NONE;

	// physical input device ID
	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 InputDeviceInternalId = INDEX_NONE;
};


// widgets / preview actors listen to this to update themselves
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSessionPlayersChanged);


UCLASS()
class GROUP2_MULTIPLAYER_API UMultiplayerGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Lobby")
	void SynchroniseLobbyPlayers();


	UFUNCTION(BlueprintPure, Category = "Multiplayer|Lobby")
	int32 GetConnectedGamepadCount() const;


	UFUNCTION(BlueprintPure, Category = "Multiplayer|Lobby")
	int32 GetSessionPlayerCount() const;


	UFUNCTION(BlueprintPure, Category = "Multiplayer|Lobby")
	TArray<FSessionPlayerData> GetSessionPlayers() const;


	UFUNCTION(BlueprintPure, Category = "Multiplayer|Lobby")
	int32 GetPlayerSlotForPlatformUser(int32 PlatformUserInternalId) const;


	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Lobby")
	bool ReadyPlayer(int32 PlayerSlot);


	UFUNCTION(BlueprintPure, Category = "Multiplayer|Lobby")
	bool AreAllSessionPlayersReady() const;

	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Customisation")
	void MoveCustomisationSelection(int32 PlayerSlot, int32 Direction);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Customisation")
	void CycleCosmetic(int32 PlayerSlot, int32 Direction);


	// kept for compatibility with any Blueprint references
	// now returns the number of body colour options
	UFUNCTION(BlueprintPure, Category = "Multiplayer|Customisation")
	int32 GetCosmeticOptionCount() const;


	/*
	used by the lobby UI so it can display the currently selected
	option name without needing to understand all the arrays itself
	*/
	UFUNCTION(BlueprintPure, Category = "Multiplayer|Customisation")
	FText GetCustomisationDisplayName(int32 PlayerSlot, int32 CategoryIndex) const;

	/*
	set player ring colour directly
	returns false if the slot/colour is invalid, the player has readied up,
	or another player already has that colour
	*/
	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Customisation|Ring")
	bool SetRingColour(int32 PlayerSlot, int32 ColourIndex);

	// move to the next or previous ring colour, skipping colours other players have
	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Customisation|Ring")
	void CycleRingColour(int32 PlayerSlot, int32 Direction);

	// true if a player OTHER than IgnorePlayerSlot has this colour (use to grey out picked colours)
	UFUNCTION(BlueprintPure, Category = "Multiplayer|Customisation|Ring")
	bool IsRingColourTaken(int32 ColourIndex, int32 IgnorePlayerSlot = -1) const;

	// players current index into RingColourOptions
	UFUNCTION(BlueprintPure, Category = "Multiplayer|Customisation|Ring")
	int32 GetRingColourIndex(int32 PlayerSlot) const;

	/*
	the actual colour to show for a player
	uses slots default colour if you are testing
	*/
	UFUNCTION(BlueprintPure, Category = "Multiplayer|Customisation|Ring")
	FLinearColor GetRingColourForPlayerSlot(int32 PlayerSlot) const;

	/*
	which player slot a controller belongs to
	matches by platform user like the lobby does, falling back to local player order
	when there is no lobby session
	*/
	UFUNCTION(BlueprintPure, Category = "Multiplayer|Session")
	int32 GetPlayerSlotForController(const AController* Controller) const;


	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Session")
	void ResetSession();

	/*
	designers populate these arrays through BP_GI_Multiplayer
	no C++ changes are required when new hats/chest pieces/etc are created
	*/

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Multiplayer|Customisation|Options")
	TArray<FBodyColourOption> BodyColourOptions;

	// the preset ring colours (one per player, so 4)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Multiplayer|Customisation|Options")
	TArray<FBodyColourOption> RingColourOptions;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Multiplayer|Customisation|Options")
	TArray<FCosmeticMeshOption> HeadCosmeticOptions;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Multiplayer|Customisation|Options")
	TArray<FCosmeticMeshOption> ChestCosmeticOptions;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Multiplayer|Customisation|Options")
	TArray<FCosmeticMeshOption> LegsCosmeticOptions;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Multiplayer|Customisation|Options")
	TArray<FCosmeticMeshOption> BackCosmeticOptions;

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer|Events")
	FOnSessionPlayersChanged OnSessionPlayersChanged;


protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Multiplayer|Session")
	TArray<FSessionPlayerData> SessionPlayers;


private:

	void GetConnectedGamepadMappings(TArray<FInputDeviceId>& OutDeviceIds, TArray<FPlatformUserId>& OutPlatformUsers) const;

	bool DoesSessionMatchDetectedGamepads(const TArray<FInputDeviceId>& DeviceIds, const TArray<FPlatformUserId>& PlatformUsers) const;

	void RebuildLocalPlayersAndSession(const TArray<FInputDeviceId>& DeviceIds, const TArray<FPlatformUserId>& PlatformUsers);

	// return number of options available for one of the five categories
	int32 GetOptionCountForCategory(int32 CategoryIndex) const;

	// helper used to wrap an index around an array
	void CycleIndexWrapped(int32& Index, int32 OptionCount, int32 Direction) const;
};