// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "DynamicCamera.generated.h"

/**
 * 
 */
UCLASS()
class GROUP2_MULTIPLAYER_API ADynamicCamera : public AActor
{
	GENERATED_BODY()
	
	
	
public:
	ADynamicCamera();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Numbers")
	float DampTime = 0.2;                 // Approximate time for the camera to refocus.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Numbers")
	float ScreenEdgeBuffer = 10;           // Space between the top/bottom most target and the screen edge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Numbers")
	float MinSize = 6.5;
	
	UPROPERTY()
	TArray<TObjectPtr<AActor>> Targets;
	
private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;                    // Used for referencing the camera.
	float ZoomSpeed;                      // Reference speed for the smooth damping of the orthographic size.
	FVector MoveVelocity;                 // Reference velocity for the smooth damping of the position.
	FVector DesiredPosition; 
	
protected:
	virtual void BeginPlay() override;
	
public:
	virtual void Tick(float DeltaTime) override;
	
	void Move(float DeltaTime);
	
	void Zoom(float DeltaTime);
	
	void FindAveragePosition();
	
	float FindRequiredSize();
	
	void SetStartPositionAndSize ();
};
