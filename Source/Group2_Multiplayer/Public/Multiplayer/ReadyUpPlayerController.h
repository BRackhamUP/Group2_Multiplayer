#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ReadyUpPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UMultiplayerGameInstance;
struct FInputActionValue;

UCLASS()
class GROUP2_MULTIPLAYER_API AReadyUpPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AReadyUpPlayerController();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadOnly, Category = "Customisation|Ready")
	float ReadyHoldProgress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Customisation|Ready")
	bool bHoldingReadyButton = false;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customisation|Input")
	TObjectPtr<UInputMappingContext> LobbyMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customisation|Input")
	TObjectPtr<UInputAction> ReadyAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customisation|Input")
	TObjectPtr<UInputAction> UnreadyAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customisation|Input")
	TObjectPtr<UInputAction> PreviousCosmeticAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customisation|Input")
	TObjectPtr<UInputAction> NextCosmeticAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customisation|Input")
	TObjectPtr<UInputAction> NavigateCustomisationAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customisation|Input")
	TObjectPtr<UInputAction> RotateSpud;

	UFUNCTION(BlueprintImplementableEvent, Category = "Customisation")
	void RotatePreviewCharacter(int32 PlayerNumber, float DeltaYaw);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customisation|Ready", meta = (ClampMin = "0.1"))
	float RequiredReadyHoldTime = 3.0f;

private:
	void AutoJoinPlayerOne();
	void HandleReadyPressed();
	void HandleReadyReleased();
	void HandleUnready();
	void HandlePreviousCosmetic();
	void HandleNextCosmetic();
	void HandleNavigateCustomisation(const FInputActionValue& Value);
	void HandleRotateSpud(const FInputActionValue& Value);

	void CycleSelectedCosmetic(int32 Direction);
	int32 GetPlayerNumber() const;
	UMultiplayerGameInstance* GetMultiplayerGameInstance() const;

	float ReadyHoldTime = 0.0f;
};