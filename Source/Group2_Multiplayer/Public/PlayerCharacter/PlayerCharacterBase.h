#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "PlayerCharacterBase.generated.h"

class UInputAction;
class UInputMappingContext;
class USphereComponent;
class UPrimitiveComponent;
class AItemBase;

UCLASS()
class GROUP2_MULTIPLAYER_API APlayerCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	// sets default values for this character's properties
	APlayerCharacterBase();

protected:
	// called when the game starts or when spawned
	virtual void BeginPlay() override;

	// drop anything we're holding if this character is removed (e.g. falls out of the arena)
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// the input mapping context assigned in the blueprint inspector panel
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	// movement input
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	// aiming input
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> FaceDirectionAction;

	// jummp input
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	// interact input, picks up the closest weapon in range (swapping it for the held one if needed)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;


	// minimum sitck magnitude required before facing input is accepeted
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement Facing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FacingDeadZone = 0.20f;

	// controls how quickyl the player rotates towards desired location
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement Facing", meta = (ClampMin = "0.0"))
	float FacingInterpSpeed = 12.0f;

	// how long a jump input is remembered before landing
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement Jump",
		meta = (ClampMin = "0.0"))
	float JumpBufferTime = 0.12f;






	// items inside this sphere can be picked up with interact (or swapped if a weapon is already held)
	// change the radius in the BP to tune pickup range
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Pickup")
	TObjectPtr<USphereComponent> PickupSphere;

	// socket on the character skeletal mesh that the weapon attaches to
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Pickup")
	FName WeaponSocketName = TEXT("WeaponSocket");

	// if the mesh has no WeaponSocket (our capsule), weapon is held at this offset from the capsule
	// (X = forward, Y = right, Z = up)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Pickup")
	FVector HeldItemFallbackOffset = FVector(50.f, 35.f, 0.f);

	// how far above the swapped weapons old position the dropped weapon is placed (to stop colliding on swap)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Pickup", meta = (Units = "cm"))
	float DropHeightOffset = 20.f;

	// shows the pickup/swap prompt as on screen debug text until a proper UI widget exists
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Pickup")
	bool bShowDebugPickupPrompt = true;

	// called when the pickup prompt should appear/disappear (or point at a different item)
	// use GetHeldItem to decide between pickup and swap
	// build the actual prompt UI in BP from this event
	UFUNCTION(BlueprintImplementableEvent, Category = "Item Pickup")
	void OnPickupPromptChanged(bool bVisible, AItemBase* PromptItem);





	// handle left stick movement
	void Move(const FInputActionValue& Value);

	// handle right stick facing
	void FaceDirection(const FInputActionValue& Value);


	// covnert 2D controller stick direciton into a world space direction based of camera yaw
	FVector GetCameraRelativeDirection(const FVector2D& Input) const;

	// update the characters facing direciton each frame
	void UpdateFacing(float DeltaTime);

	// handle jump input and allow a short buffered jump window
	void HandleJumpPressed();


	// current left stick vlaue
	FVector2D MoveInput = FVector2D::ZeroVector;

	// current right stick value
	FVector2D FaceInput = FVector2D::ZeroVector;

	// remaining time for a buffered jump request
	float JumpBufferTimer = 0.0f;



public:	

	// called every frame
	virtual void Tick(float DeltaTime) override;

	// called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "Item Pickup")
	AItemBase* GetHeldItem() const { return HeldItem; }

	UFUNCTION(BlueprintPure, Category = "Item Pickup")
	bool IsHoldingItem() const { return HeldItem != nullptr; }

private:
	// these must be UFUNCTIONs so they can be bound with AddDynamic
	UFUNCTION()
	void OnPickupSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnPickupSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// interact pressed, pick up the closest weapon in range (swapping if one is already held)
	void HandleInteractPressed();

	// attach item to this character. This does NOT check HeldItem because callers decide when it is allowed
	bool EquipItem(AItemBase* Item);

	// equip new item and drop the current weapon where new item was
	void SwapToItem(AItemBase* NewItem);

	// every tick, keep the pickup prompt pointing at the closest weapon in range
	void UpdateNearbyItems();

	// closest valid item in range. remove any destroyed/held items from NearbyItems
	AItemBase* FindClosestNearbyItem();

	// only fires the prompt event when the prompted item or the held item actually changes
	void SetPromptedItem(AItemBase* Item);

	// the weapon in this players hand (only 1 at a time)
	UPROPERTY(Transient)
	TObjectPtr<AItemBase> HeldItem;

	// items currently overlapping the pickup sphere.
	// weak pointer because they may be destroyed or picked up by someone else
	TArray<TWeakObjectPtr<AItemBase>> NearbyItems;

	// item that the pickup prompt is currently showing, null means no prompt showing
	TWeakObjectPtr<AItemBase> PromptedItem;

	// what we were holding when the prompt last updated, so pick up / swap wording stays correct
	TWeakObjectPtr<AItemBase> PromptedHeldItem;
};
