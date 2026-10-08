// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerCharacter/PlayerCharacterBase.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

#include "Camera/PlayerCameraManager.h"

#include "InputAction.h"
#include "InputMappingContext.h"

// settin default values
APlayerCharacterBase::APlayerCharacterBase()
{
	// because its a competitive game the characters facing direction is really important, tick will smoothly update facing every frame
	PrimaryActorTick.bCanEverTick = true;

	// dont allow the character to autmoatically rotate
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// because we are manually controlling the player facing direction
	// disable this so we can control when and how the player faces
	GetCharacterMovement()->bOrientRotationToMovement = false;
}

// Called when the game starts or when spawned
void APlayerCharacterBase::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void APlayerCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateFacing(DeltaTime);

	// count down any buffered jump request. the jump was feeling really unresponsive in testing hence the jump buffer
	if (JumpBufferTimer > 0.0f)
	{
		JumpBufferTimer -= DeltaTime;

		// if we have now reached the ground during the buffer,
		// immediately perform the jump.
		if (GetCharacterMovement()->IsMovingOnGround())
		{
			Jump();
			JumpBufferTimer = 0.0f;
		}
	}
}

void APlayerCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);


	// enhanced input mappin context

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
			{
				if (DefaultMappingContext)
				{
					InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
				}
			}
		}
	}

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);

	if (!EnhancedInputComponent)
	{
		return;
	}


	// movment
	if (MoveAction)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacterBase::Move);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &APlayerCharacterBase::Move);
	}

	//direction facing
	if (FaceDirectionAction)
	{
		EnhancedInputComponent->BindAction(FaceDirectionAction,	ETriggerEvent::Triggered, this,	&APlayerCharacterBase::FaceDirection);

		EnhancedInputComponent->BindAction(FaceDirectionAction,	ETriggerEvent::Completed, this,	&APlayerCharacterBase::FaceDirection);
	}

	// jump
	if (JumpAction)
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &APlayerCharacterBase::HandleJumpPressed);

		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
}


// move aciton
void APlayerCharacterBase::Move(const FInputActionValue& Value)
{
	// read the 2D left-stick value
	MoveInput = Value.Get<FVector2D>();

	// ignore effectively zero input
	if (MoveInput.IsNearlyZero())
	{
		return;
	}

	// convert controller-stick input into a world direction
	// relative to the arena camera
	const FVector MovementDirection = GetCameraRelativeDirection(MoveInput);


	// addMovementInput feeds the desired movement direction into
	// CharacterMovementComponent
	AddMovementInput(MovementDirection);
}

void APlayerCharacterBase::HandleJumpPressed()
{
	// if we can jump right now, do it immediately.
	if (CanJump())
	{
		Jump();
		JumpBufferTimer = 0.0f;
		return;
	}

	// otherwise remember the player's jump request briefly.
	JumpBufferTimer = JumpBufferTime;
}



void APlayerCharacterBase::FaceDirection(const FInputActionValue& Value)
{
	// store the rightstick value, not imediately used as thats in tick
	FaceInput = Value.Get<FVector2D>();
}


FVector APlayerCharacterBase::GetCameraRelativeDirection(const FVector2D& Input) const
{
	// use player controller to access the camera relative direciton
	const APlayerController* PlayerController =	Cast<APlayerController>(Controller);

	if (!PlayerController)
	{
		return FVector::ZeroVector;
	}

	// get the current camera manager
	const APlayerCameraManager* CameraManager =	PlayerController->PlayerCameraManager;

	if (!CameraManager)
	{
		return FVector::ZeroVector;
	}

	// get camera rotation
	const FRotator CameraRotation =	CameraManager->GetCameraRotation();


	// only need camera yaw, if camera is 2.5D other values could effect player movement negatively
	const FRotator CameraYawRotation(0.0f, CameraRotation.Yaw, 0.0f);


	// find the camera-relative forward direction
	const FVector ForwardDirection = FRotationMatrix(CameraYawRotation).GetUnitAxis(EAxis::X);


	// find the camera-relative right direction.
	const FVector RightDirection = FRotationMatrix(CameraYawRotation).GetUnitAxis(EAxis::Y);


	// build final movement from stick direciton
	FVector Direction =(ForwardDirection * Input.Y) + (RightDirection * Input.X);


	// prevent diagonal input from producing extra movement magnitude.
	Direction = Direction.GetClampedToMaxSize(1.0f);

	return Direction;
}

// update the players facing direciotn for throwing override
void APlayerCharacterBase::UpdateFacing(float DeltaTime)
{

	const float DeadZoneSquared = FMath::Square(FacingDeadZone);

	const bool bHasFaceInput = FaceInput.SizeSquared() > DeadZoneSquared;

	const bool bHasMoveInput = MoveInput.SizeSquared() > DeadZoneSquared;

	FVector2D DesiredFacingInput = FVector2D::ZeroVector;

	if (bHasFaceInput)
	{
		DesiredFacingInput = FaceInput;
	}

	// if right stick isnt being used then use left stick facing direction
	else if (bHasMoveInput)
	{
		DesiredFacingInput = MoveInput;
	}

	// if nothing then just remain facing last direciton
	else
	{
		return;
	}

	const FVector DesiredWorldDirection = GetCameraRelativeDirection(DesiredFacingInput);


	if (DesiredWorldDirection.IsNearlyZero())
	{
		return;
	}

	const float TargetYaw =	DesiredWorldDirection.Rotation().Yaw;


	const FRotator TargetRotation(0.0f, TargetYaw, 0.0f);


	// get the new rotation an intrp for smooth facing 
	const FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaTime, FacingInterpSpeed);

	SetActorRotation(NewRotation);
}