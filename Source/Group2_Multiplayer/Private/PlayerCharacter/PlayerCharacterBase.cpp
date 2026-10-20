// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerCharacter/PlayerCharacterBase.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

#include "Camera/PlayerCameraManager.h"

#include "InputAction.h"
#include "InputMappingContext.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"

#include "Items/ItemBase.h"

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

	// pickup sphere: only overlaps physics bodies (items use the PhysicsActor profile = PhysicsBody object type).
	// It ignores everything else so it never blocks movement, other players or traces
	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->SetupAttachment(GetCapsuleComponent());
	PickupSphere->InitSphereRadius(75.f);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionObjectType(ECC_WorldDynamic);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	PickupSphere->SetGenerateOverlapEvents(true);
	PickupSphere->SetCanEverAffectNavigation(false);
}

// Called when the game starts or when spawned
void APlayerCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// bound here rather than the constructor, which is the safer place for AddDynamic
	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &APlayerCharacterBase::OnPickupSphereBeginOverlap);
	PickupSphere->OnComponentEndOverlap.AddDynamic(this, &APlayerCharacterBase::OnPickupSphereEndOverlap);
}

void APlayerCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// drop the weapon where we are, otherwise it would be left floating with no collision
	if (IsValid(HeldItem))
	{
		HeldItem->DetachFromHolder(HeldItem->GetActorLocation());
	}
	HeldItem = nullptr;

	SetPromptedItem(nullptr);

	Super::EndPlay(EndPlayReason);
}

// Called every frame
void APlayerCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateFacing(DeltaTime);

	UpdateNearbyItems();

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

	// interact (weapon pickup / swap)
	if (InteractAction)
	{
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &APlayerCharacterBase::HandleInteractPressed);
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



void APlayerCharacterBase::OnPickupSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AItemBase* Item = Cast<AItemBase>(OtherActor);
	if (!Item || Item->IsHeld())
	{
		return;
	}

	// in range only, nothing is picked up until interact is pressed (see UpdateNearbyItems for the prompt)
	NearbyItems.AddUnique(Item);
}

void APlayerCharacterBase::OnPickupSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (AItemBase* Item = Cast<AItemBase>(OtherActor))
	{
		NearbyItems.Remove(Item);
	}
}

void APlayerCharacterBase::HandleInteractPressed()
{
	AItemBase* Target = FindClosestNearbyItem();
	if (!Target)
	{
		return;
	}

	// holding a weapon = swap, empty handed = pickup
	if (HeldItem)
	{
		SwapToItem(Target);
	}
	else
	{
		EquipItem(Target);
	}
}

bool APlayerCharacterBase::EquipItem(AItemBase* Item)
{
	if (!IsValid(Item) || Item->IsHeld())
	{
		return false;
	}

	USkeletalMeshComponent* CharacterMesh = GetMesh();
	const bool bHasSocket = CharacterMesh && CharacterMesh->DoesSocketExist(WeaponSocketName);

	if (bHasSocket)
	{
		Item->AttachToHolder(this, CharacterMesh, WeaponSocketName);
	}
	else
	{
		// no socket yet, hold it at an offset from the capsule.
		// since the capsule rotates with UpdateFacing, the weapon still points where the player faces
		Item->AttachToHolder(this, GetCapsuleComponent(), NAME_None);
		Item->SetActorRelativeLocation(HeldItemFallbackOffset);
	}

	HeldItem = Item;
	NearbyItems.Remove(Item);
	return true;
}

void APlayerCharacterBase::SwapToItem(AItemBase* NewItem)
{
	AItemBase* OldItem = HeldItem;
	if (!IsValid(NewItem) || NewItem == OldItem)
	{
		return;
	}

	// the old weapon is dropped where the new one was lying, which we know is a clear spot
	const FVector DropLocation = NewItem->GetActorLocation() + FVector(0.f, 0.f, DropHeightOffset);

	// equip first so if the new weapon cant be taken (another player grabbed it
	// this frame) we keep the one we have instead of ending up empty handed
	if (!EquipItem(NewItem))
	{
		return;
	}

	if (IsValid(OldItem))
	{
		OldItem->DetachFromHolder(DropLocation);

		// even if the overlap event is late, make sure the prompt appears for swapping back to the dropped weapon
		NearbyItems.AddUnique(OldItem);
	}
}

void APlayerCharacterBase::UpdateNearbyItems()
{
	// held weapon destroyed (future proofing for shield breaking, for example), treat as empty handed
	if (HeldItem && !IsValid(HeldItem))
	{
		HeldItem = nullptr;
	}

	// prompt for the closest weapon in range, held or not (null hides the prompt)
	SetPromptedItem(FindClosestNearbyItem());
}

AItemBase* APlayerCharacterBase::FindClosestNearbyItem()
{
	// drop anything destroyed or picked up by another player since it entered range
	NearbyItems.RemoveAll([](const TWeakObjectPtr<AItemBase>& Item)
	{
		return !Item.IsValid() || Item->IsHeld();
	});

	AItemBase* Closest = nullptr;
	float ClosestDistSq = TNumericLimits<float>::Max();
	const FVector MyLocation = GetActorLocation();

	for (const TWeakObjectPtr<AItemBase>& WeakItem : NearbyItems)
	{
		AItemBase* Item = WeakItem.Get();
		const float DistSq = FVector::DistSquared(MyLocation, Item->GetActorLocation());
		if (DistSq < ClosestDistSq)
		{
			ClosestDistSq = DistSq;
			Closest = Item;
		}
	}

	return Closest;
}

void APlayerCharacterBase::SetPromptedItem(AItemBase* Item)
{
	if (PromptedItem.Get() == Item && PromptedHeldItem.Get() == HeldItem)
	{
		return;
	}

	PromptedItem = Item;
	PromptedHeldItem = HeldItem;
	OnPickupPromptChanged(Item != nullptr, Item);

#if !UE_BUILD_SHIPPING
	// temporary prompt until a UI widget is hooked up to OnPickupPromptChanged.
	// one message per player so each player prompt replaces only their own line
	if (bShowDebugPickupPrompt && GEngine)
	{
		const uint64 MessageKey = static_cast<uint64>(GetUniqueID());
		if (Item)
		{
			const FString Message = HeldItem
				? FString::Printf(TEXT("%s: press Interact to swap %s for %s"),
					*GetActorNameOrLabel(),
					*HeldItem->GetItemDisplayName().ToString(),
					*Item->GetItemDisplayName().ToString())
				: FString::Printf(TEXT("%s: press Interact to pick up %s"),
					*GetActorNameOrLabel(),
					*Item->GetItemDisplayName().ToString());
			GEngine->AddOnScreenDebugMessage(MessageKey, 9999.f, FColor::Yellow, Message);
		}
		else
		{
			GEngine->RemoveOnScreenDebugMessage(MessageKey);
		}
	}
#endif
}