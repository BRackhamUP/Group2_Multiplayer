#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MultiplayerGameInstance.generated.h"

class UStaticMesh;
class UMaterialInterface;
class AController;

USTRUCT(BlueprintType)
struct FSkinOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customisation")
	FName CosmeticId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customisation")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customisation")
	TObjectPtr<UMaterialInterface> Material = nullptr;
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

USTRUCT(BlueprintType)
struct FSessionPlayerData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 PlayerNumber = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 DisplayPosition = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	bool bHasJoined = false;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	bool bConnected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	bool bReady = false;

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

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 SelectedCustomisationRow = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 PlatformUserInternalId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Session Player")
	int32 InputDeviceInternalId = INDEX_NONE;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSessionPlayersChanged);

UCLASS()
class GROUP2_MULTIPLAYER_API UMultiplayerGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Multiplayer|Players")
	int32 GetPlayerNumberAtDisplayPosition(int32 DisplayPosition) const;

	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Controllers")
	void SynchroniseControllers();

	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Players")
	int32 JoinPlayer(int32 PlatformUserInternalId);

	UFUNCTION(BlueprintPure, Category = "Multiplayer|Players")
	int32 GetJoinedPlayerCount() const;

	UFUNCTION(BlueprintPure, Category = "Multiplayer|Players")
	TArray<FSessionPlayerData> GetSessionPlayers() const;

	UFUNCTION(BlueprintPure, Category = "Multiplayer|Players")
	int32 GetPlayerNumberForPlatformUser(int32 PlatformUserInternalId) const;

	UFUNCTION(BlueprintPure, Category = "Multiplayer|Players")
	int32 GetPlayerNumberForController(const AController* Controller) const;

	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Ready")
	bool ReadyPlayer(int32 PlayerNumber);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Ready")
	bool UnreadyPlayer(int32 PlayerNumber);

	UFUNCTION(BlueprintPure, Category = "Multiplayer|Ready")
	bool AreAllJoinedPlayersReady() const;

	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Customisation")
	void MoveCustomisationSelection(int32 PlayerNumber, int32 Direction);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Customisation")
	void CycleCosmetic(int32 PlayerNumber, int32 Direction);

	UFUNCTION(BlueprintPure, Category = "Multiplayer|Customisation")
	FText GetCustomisationDisplayName(int32 PlayerNumber, int32 CategoryIndex) const;

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

	UFUNCTION(BlueprintCallable, Category = "Multiplayer|Session")
	void ResetSession();

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

	UPROPERTY(BlueprintAssignable, Category = "Multiplayer|Events")
	FOnSessionPlayersChanged OnSessionPlayersChanged;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Multiplayer|Session")
	TArray<FSessionPlayerData> SessionPlayers;

private:
	void GetConnectedGamepadMappings(TArray<FInputDeviceId>& OutDeviceIds, TArray<FPlatformUserId>& OutPlatformUsers) const;
	void UpdateDisplayPositions();
};