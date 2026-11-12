#include "Multiplayer/ReadyUpPlayerController.h"

#include "Multiplayer/MultiplayerGameInstance.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h"

AReadyUpPlayerController::AReadyUpPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AReadyUpPlayerController::BeginPlay()
{
	Super::BeginPlay();

	ULocalPlayer* LocalPlayer = GetLocalPlayer();

	if (!LocalPlayer)
	{
		return;
	}

	// Player 1 joins automatically after the level has initialised.
	const int32 LocalPlayerIndex = GetGameInstance()->GetLocalPlayers().IndexOfByKey(LocalPlayer);

	if (LocalPlayerIndex == 0)
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &AReadyUpPlayerController::AutoJoinPlayerOne);
	}
}

void AReadyUpPlayerController::SetupInputComponent()
{
	APlayerController::SetupInputComponent();

	ULocalPlayer* LocalPlayer = GetLocalPlayer();

	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();

	if (InputSubsystem && LobbyMappingContext)
	{
		InputSubsystem->RemoveMappingContext(LobbyMappingContext);
		InputSubsystem->AddMappingContext(LobbyMappingContext, 0);
	}

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);

	if (!EnhancedInputComponent)
	{
		return;
	}

	if (ReadyAction)
	{
		EnhancedInputComponent->BindAction(ReadyAction, ETriggerEvent::Started, this, &AReadyUpPlayerController::HandleReadyPressed);
		EnhancedInputComponent->BindAction(ReadyAction, ETriggerEvent::Completed, this, &AReadyUpPlayerController::HandleReadyReleased);
		EnhancedInputComponent->BindAction(ReadyAction, ETriggerEvent::Canceled, this, &AReadyUpPlayerController::HandleReadyReleased);
	}

	if (UnreadyAction)
	{
		EnhancedInputComponent->BindAction(UnreadyAction, ETriggerEvent::Started, this, &AReadyUpPlayerController::HandleUnready);
	}

	if (PreviousCosmeticAction)
	{
		EnhancedInputComponent->BindAction(PreviousCosmeticAction, ETriggerEvent::Started, this, &AReadyUpPlayerController::HandlePreviousCosmetic);
	}

	if (NextCosmeticAction)
	{
		EnhancedInputComponent->BindAction(NextCosmeticAction, ETriggerEvent::Started, this, &AReadyUpPlayerController::HandleNextCosmetic);
	}

	if (NavigateCustomisationAction)
	{
		EnhancedInputComponent->BindAction(NavigateCustomisationAction, ETriggerEvent::Started, this, &AReadyUpPlayerController::HandleNavigateCustomisation);
	}

	if (RotateSpud)
	{
		EnhancedInputComponent->BindAction(RotateSpud, ETriggerEvent::Triggered, this, &AReadyUpPlayerController::HandleRotateSpud);
	}
}

void AReadyUpPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();

	if (LocalPlayer)
	{
		UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();

		if (InputSubsystem && LobbyMappingContext)
		{
			InputSubsystem->RemoveMappingContext(LobbyMappingContext);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AReadyUpPlayerController::AutoJoinPlayerOne()
{
	UMultiplayerGameInstance* MultiplayerGameInstance = GetMultiplayerGameInstance();

	if (!MultiplayerGameInstance)
	{
		return;
	}

	const FPlatformUserId PlatformUserId = GetPlatformUserId();

	if (!PlatformUserId.IsValid())
	{
		return;
	}

	if (MultiplayerGameInstance->GetPlayerNumberForPlatformUser(PlatformUserId.GetInternalId()) != INDEX_NONE)
	{
		return;
	}

	MultiplayerGameInstance->JoinPlayer(PlatformUserId.GetInternalId());
}

int32 AReadyUpPlayerController::GetPlayerNumber() const
{
	const FPlatformUserId PlatformUserId = GetPlatformUserId();

	if (!PlatformUserId.IsValid())
	{
		return INDEX_NONE;
	}

	const UMultiplayerGameInstance* MultiplayerGameInstance = GetMultiplayerGameInstance();

	if (!MultiplayerGameInstance)
	{
		return INDEX_NONE;
	}

	return MultiplayerGameInstance->GetPlayerNumberForPlatformUser(PlatformUserId.GetInternalId());
}

UMultiplayerGameInstance* AReadyUpPlayerController::GetMultiplayerGameInstance() const
{
	return Cast<UMultiplayerGameInstance>(GetGameInstance());
}

void AReadyUpPlayerController::HandleReadyPressed()
{
	UMultiplayerGameInstance* MultiplayerGameInstance = GetMultiplayerGameInstance();

	if (!MultiplayerGameInstance)
	{
		return;
	}

	const FPlatformUserId PlatformUserId = GetPlatformUserId();

	if (!PlatformUserId.IsValid())
	{
		return;
	}

	const int32 PlayerNumber = GetPlayerNumber();

	// the first A press joins without starting the ready hold.
	if (PlayerNumber == INDEX_NONE)
	{
		MultiplayerGameInstance->JoinPlayer(PlatformUserId.GetInternalId());
		return;
	}

	const TArray<FSessionPlayerData>& SessionPlayers = MultiplayerGameInstance->GetSessionPlayers();

	if (!SessionPlayers.IsValidIndex(PlayerNumber) || SessionPlayers[PlayerNumber].bReady)
	{
		return;
	}

	bHoldingReadyButton = true;
	ReadyHoldTime = 0.0f;
	ReadyHoldProgress = 0.0f;
}

void AReadyUpPlayerController::HandleReadyReleased()
{
	if (!bHoldingReadyButton)
	{
		return;
	}

	bHoldingReadyButton = false;
	ReadyHoldTime = 0.0f;
	ReadyHoldProgress = 0.0f;
}

void AReadyUpPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bHoldingReadyButton)
	{
		return;
	}

	const int32 PlayerNumber = GetPlayerNumber();

	if (PlayerNumber == INDEX_NONE)
	{
		bHoldingReadyButton = false;
		ReadyHoldTime = 0.0f;
		ReadyHoldProgress = 0.0f;
		return;
	}

	UMultiplayerGameInstance* MultiplayerGameInstance = GetMultiplayerGameInstance();

	if (!MultiplayerGameInstance)
	{
		return;
	}

	const TArray<FSessionPlayerData>& SessionPlayers = MultiplayerGameInstance->GetSessionPlayers();

	if (!SessionPlayers.IsValidIndex(PlayerNumber))
	{
		return;
	}

	if (!SessionPlayers[PlayerNumber].bConnected || SessionPlayers[PlayerNumber].bReady)
	{
		bHoldingReadyButton = false;
		ReadyHoldTime = 0.0f;
		ReadyHoldProgress = SessionPlayers[PlayerNumber].bReady ? 1.0f : 0.0f;
		return;
	}

	ReadyHoldTime += DeltaSeconds;
	ReadyHoldProgress = FMath::Clamp(ReadyHoldTime / RequiredReadyHoldTime, 0.0f, 1.0f);

	if (ReadyHoldTime < RequiredReadyHoldTime)
	{
		return;
	}

	MultiplayerGameInstance->ReadyPlayer(PlayerNumber);

	bHoldingReadyButton = false;
	ReadyHoldTime = RequiredReadyHoldTime;
	ReadyHoldProgress = 1.0f;
}

void AReadyUpPlayerController::HandleUnready()
{
	const int32 PlayerNumber = GetPlayerNumber();
	UMultiplayerGameInstance* MultiplayerGameInstance = GetMultiplayerGameInstance();

	if (PlayerNumber == INDEX_NONE || !MultiplayerGameInstance)
	{
		return;
	}

	if (MultiplayerGameInstance->UnreadyPlayer(PlayerNumber))
	{
		bHoldingReadyButton = false;
		ReadyHoldTime = 0.0f;
		ReadyHoldProgress = 0.0f;
	}
}

void AReadyUpPlayerController::HandlePreviousCosmetic()
{
	CycleSelectedCosmetic(-1);
}

void AReadyUpPlayerController::HandleNextCosmetic()
{
	CycleSelectedCosmetic(1);
}

void AReadyUpPlayerController::CycleSelectedCosmetic(int32 Direction)
{
	const int32 PlayerNumber = GetPlayerNumber();
	UMultiplayerGameInstance* MultiplayerGameInstance = GetMultiplayerGameInstance();

	if (PlayerNumber == INDEX_NONE || !MultiplayerGameInstance)
	{
		return;
	}

	MultiplayerGameInstance->CycleCosmetic(PlayerNumber, Direction);
}

void AReadyUpPlayerController::HandleNavigateCustomisation(const FInputActionValue& Value)
{
	const int32 PlayerNumber = GetPlayerNumber();

	if (PlayerNumber == INDEX_NONE)
	{
		return;
	}

	const float AxisValue = Value.Get<float>();

	if (FMath::IsNearlyZero(AxisValue))
	{
		return;
	}

	// positive input moves up and negative input moves down.
	const int32 Direction = AxisValue > 0.0f ? -1 : 1;
	UMultiplayerGameInstance* MultiplayerGameInstance = GetMultiplayerGameInstance();

	if (!MultiplayerGameInstance)
	{
		return;
	}

	MultiplayerGameInstance->MoveCustomisationSelection(PlayerNumber, Direction);
}

void AReadyUpPlayerController::HandleRotateSpud(const FInputActionValue& Value)
{
	const int32 PlayerNumber = GetPlayerNumber();

	if (PlayerNumber == INDEX_NONE)
	{
		return;
	}

	const float StickX = Value.Get<float>();
	const float DeltaYaw = StickX * 120.0f * GetWorld()->GetDeltaSeconds();

	RotatePreviewCharacter(PlayerNumber, DeltaYaw);
}