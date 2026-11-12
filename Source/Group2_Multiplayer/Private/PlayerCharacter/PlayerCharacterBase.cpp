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

#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Items/ItemBase.h"
#include "Items/WeaponBase.h"
#include "Multiplayer/MultiplayerGameInstance.h"

namespace
{
	// how far the bottom of the capsule is above whatever is below it
	// returns -1 if there is nothing within MaxDistance (if player is falling off platform)
	float GetHeightAboveFloor(const ACharacter* Character, float MaxDistance)
	{
		const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Start = Character->GetActorLocation();
		const FVector End = Start - FVector(0.f, 0.f, HalfHeight + MaxDistance);

		FHitResult Hit;

		// ignore our own capsule, the trace starts inside it
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(Character);
		if (!Character->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			return -1.f;
		}

		return FMath::Max(0.f, Hit.Distance - HalfHeight);
	}
}

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

	// ring decal: pitched down so it projects onto the floor
	// DecalSize = (projection depth, radius, radius) depth reaches well below the capsule
	// so the ring stays on the floor while jumping
	RingDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("RingDecal"));
	RingDecal->SetupAttachment(GetCapsuleComponent());
	RingDecal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	RingDecal->DecalSize = FVector(300.f, 60.f, 60.f);

	// ghost ring: purely visual, disabled collision, shadows and navigation
	// position and size are from tick (see UpdateRings)
	GhostRingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostRingMesh"));
	GhostRingMesh->SetupAttachment(GetCapsuleComponent());
	GhostRingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GhostRingMesh->SetGenerateOverlapEvents(false);
	GhostRingMesh->SetCastShadow(false);
	GhostRingMesh->SetCanEverAffectNavigation(false);
}

// Called when the game starts or when spawned
void APlayerCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// bound here rather than the constructor, which is the safer place for AddDynamic
	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &APlayerCharacterBase::OnPickupSphereBeginOverlap);
	PickupSphere->OnComponentEndOverlap.AddDynamic(this, &APlayerCharacterBase::OnPickupSphereEndOverlap);

	// stop rings from painting onto character meshes
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		Primitive->SetReceivesDecals(false);
	}

	// apply the current colour
	SetRingColour(RingColour);

	// give this player their fists for when they arent holding a weapon
	SpawnUnarmedWeapon();
}

void APlayerCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	ApplyRingColourFromSession();
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

	// fists only exist for this player, so remove them rather than dropping them
	if (IsValid(UnarmedWeapon))
	{
		UnarmedWeapon->Destroy();
	}
	UnarmedWeapon = nullptr;

	Super::EndPlay(EndPlayReason);
}

// Called every frame
void APlayerCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateFacing(DeltaTime);

	UpdateNearbyItems();

	// shrink the ring the further we are off the ground, and keep the ghost ring matching it
	UpdateRings();

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

	// drop held weapon
	if (DropAction)
	{
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Started, this, &APlayerCharacterBase::HandleDropPressed);
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




void APlayerCharacterBase::SetRingColour(FLinearColor NewColour)
{
	RingColour = NewColour;

	// make our own copy of the ring material the first time, so changing
	// our colour does not change every other players ring too
	if (!RingMaterialInstance && RingDecal && RingDecal->GetDecalMaterial())
	{
		RingMaterialInstance = RingDecal->CreateDynamicMaterialInstance();
	}

	if (RingMaterialInstance)
	{
		RingMaterialInstance->SetVectorParameterValue(RingColourParameterName, RingColour);
	}

	// same again but for the ghost ring
	if (!GhostRingMaterialInstance && GhostRingMesh && GhostRingMesh->GetMaterial(0))
	{
		GhostRingMaterialInstance = GhostRingMesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (GhostRingMaterialInstance)
	{
		GhostRingMaterialInstance->SetVectorParameterValue(RingColourParameterName, RingColour);
	}
}

void APlayerCharacterBase::UpdateRings()
{
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	float RingScale = 1.f;

	// how far below the bottom of the capsule the floor is, on the ground its right under us
	float HeightAboveFloor = 0.f;
	bool bHasFloor = true;

	// on the ground = full size, no need to trace
	if (!GetCharacterMovement()->IsMovingOnGround())
	{
		// trace as far as the decal can project (X is its projection depth), so the ghost ring
		// still finds the floor after the ring has fully shrunk
		float TraceDistance = RingFullyShrunkHeight;
		if (RingDecal)
		{
			TraceDistance = FMath::Max(TraceDistance, RingDecal->DecalSize.X * RingDecal->GetRelativeScale3D().X - HalfHeight);
		}

		HeightAboveFloor = GetHeightAboveFloor(this, TraceDistance);
		bHasFloor = HeightAboveFloor >= 0.f;

		// nothing below us within range counts as fully shrunk
		const float Alpha = bHasFloor ? FMath::Clamp(HeightAboveFloor / RingFullyShrunkHeight, 0.f, 1.f) : 1.f;
		RingScale = FMath::Lerp(1.f, RingMinScale, Alpha);
	}

	// this has to be scaled relative to each components default scale (from the BP), so any scale set there is respected
	const APlayerCharacterBase* Defaults = GetClass()->GetDefaultObject<APlayerCharacterBase>();

	if (RingDecal)
	{
		// only Y/Z are the ring size, X is the projection depth so dont change it
		const FVector BaseScale = (Defaults && Defaults->RingDecal) ? Defaults->RingDecal->GetRelativeScale3D() : FVector::OneVector;
		const FVector NewScale(BaseScale.X, BaseScale.Y * RingScale, BaseScale.Z * RingScale);

		// dont update the decal render state every frame if we are standing still
		if (!RingDecal->GetRelativeScale3D().Equals(NewScale, KINDA_SMALL_NUMBER))
		{
			RingDecal->SetRelativeScale3D(NewScale);
		}
	}

	if (GhostRingMesh)
	{
		// nothing to sit on (falling off a platform), so hide it instead of leaving it floating
		GhostRingMesh->SetVisibility(bHasFloor);

		if (bHasFloor)
		{
			// the mesh is flat, so X/Y are the ring size
			const FVector BaseScale = (Defaults && Defaults->GhostRingMesh) ? Defaults->GhostRingMesh->GetRelativeScale3D() : FVector::OneVector;
			const FVector NewScale(BaseScale.X * RingScale, BaseScale.Y * RingScale, BaseScale.Z);

			// keep it on the floor rather than following player up into the air
			// the capsule only ever yaws, so relative Z is the same as world Z
			const FVector NewLocation(0.f, 0.f, -HalfHeight - HeightAboveFloor + GhostRingFloorOffset);

			if (!GhostRingMesh->GetRelativeScale3D().Equals(NewScale, KINDA_SMALL_NUMBER))
			{
				GhostRingMesh->SetRelativeScale3D(NewScale);
			}

			if (!GhostRingMesh->GetRelativeLocation().Equals(NewLocation, KINDA_SMALL_NUMBER))
			{
				GhostRingMesh->SetRelativeLocation(NewLocation);
			}
		}
	}
}

void APlayerCharacterBase::ApplyRingColourFromSession()
{
	const UMultiplayerGameInstance* MultiplayerGameInstance = Cast<UMultiplayerGameInstance>(GetGameInstance());

	if (!MultiplayerGameInstance)
	{
		return;
	}

	const int32 PlayerNumber = MultiplayerGameInstance->GetPlayerNumberForController(GetController());

	SetRingColour(MultiplayerGameInstance->GetRingColourForPlayer(PlayerNumber));
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

void APlayerCharacterBase::HandleDropPressed()
{
	if (!IsValid(HeldItem))
	{
		return;
	}

	AItemBase* ItemToDrop = HeldItem;

	// aim to drop it a short way in front of us, at capsule height so it falls to the floor
	const FVector Start = GetActorLocation();
	const FVector Forward = GetActorForwardVector();
	FVector DropLocation = Start + Forward * DropForwardDistance;

	// if a wall is in the way, drop it just in front of the wall instead of inside it
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponDrop), false, this);
	Params.AddIgnoredActor(ItemToDrop);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, DropLocation, ECC_Visibility, Params))
	{
		DropLocation = Hit.Location - Forward * 30.f;
	}

	HeldItem = nullptr;
	ItemToDrop->DetachFromHolder(DropLocation);

	// it lands in our pickup range, so make sure the pickup prompt appears straight away
	NearbyItems.AddUnique(ItemToDrop);
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

void APlayerCharacterBase::SpawnUnarmedWeapon()
{
	if (!UnarmedWeaponClass)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UnarmedWeapon = GetWorld()->SpawnActor<AWeaponBase>(UnarmedWeaponClass, GetActorTransform(), Params);
	if (!UnarmedWeapon)
	{
		return;
	}

	// same attach rules as EquipItem: weapon socket if the mesh has one, otherwise the capsule offset.
	// AttachToHolder also stops it despawning and hides it from pickup spheres
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (CharacterMesh && CharacterMesh->DoesSocketExist(WeaponSocketName))
	{
		UnarmedWeapon->AttachToHolder(this, CharacterMesh, WeaponSocketName);
	}
	else
	{
		UnarmedWeapon->AttachToHolder(this, GetCapsuleComponent(), NAME_None);
		UnarmedWeapon->SetActorRelativeLocation(HeldItemFallbackOffset);
	}
}

AWeaponBase* APlayerCharacterBase::GetActiveWeapon() const
{
	if (IsValid(HeldItem))
	{
		if (AWeaponBase* HeldWeapon = Cast<AWeaponBase>(HeldItem))
		{
			return HeldWeapon;
		}
	}

	return UnarmedWeapon;
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