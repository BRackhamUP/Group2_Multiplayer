#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "PlayerCharacterBase.generated.h"

class UInputAction;
class UInputMappingContext;

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

};
