// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/DynamicCamera.h"

#include "EngineUtils.h"
#include "ToolBuilderUtil.h"
#include "Camera/CameraCentreActor.h"


ADynamicCamera::ADynamicCamera()
{
	PrimaryActorTick.bCanEverTick = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	RootComponent = Camera;
	Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
}

void ADynamicCamera::BeginPlay()
{
	Super::BeginPlay();
	
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();

		if (PC && PC->IsLocalController())
		{
			// Add their pawn to the camera targets
			if (PC->GetPawn())
			{
				Targets.Add(PC->GetPawn());
			}

			// Make this the active camera
			PC->SetViewTarget(this);
		}
	}
	
	for (TActorIterator<ACameraCentreActor> It(GetWorld()); It; ++It)
	{
			Targets.Add(*It);
	}
	
	
	SetStartPositionAndSize();
	
	
}

void ADynamicCamera::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	Move(DeltaTime);
	
	Zoom(DeltaTime);
	
}

void ADynamicCamera::Move(float DeltaTime)
{
	FindAveragePosition();
	
	FVector CurrentPosition = Camera->GetComponentLocation();
	FVector newPosition = FMath::VInterpTo(CurrentPosition,DesiredPosition,DeltaTime, 1.0f/ MovementDampTime);
	
	Camera->SetWorldLocation(newPosition);
}

void ADynamicCamera::Zoom(float DeltaTime)
{
	float requiredSize = FindRequiredHorizontalSize();
	
	float newSize = FMath::FInterpTo(
		Camera->OrthoWidth,
		requiredSize,
		DeltaTime,
		1.0f / MovementDampTime
	);
	
	Camera->SetOrthoWidth(newSize);
}

float ADynamicCamera::FindRequiredSize()
{// Find the position the camera is moving towards in local space    
	FVector DesiredLocalPos = GetActorTransform().InverseTransformPosition(DesiredPosition);

	// Start the camera's size calculation at zero  
	float Size = 0.0f;

	// Go through all targets    
	for (int i = 0; i < Targets.Num(); i++)
	{
		// Skip invalid or hidden targets       
		if (!IsValid(Targets[i]) || Targets[i]->IsHidden())
		{
			continue;
		}

		// Find the target position in camera local space       
		FVector TargetLocalPos = GetActorTransform().InverseTransformPosition(Targets[i]->GetActorLocation());

		// Find the position of the target relative to the desired camera position.      
		FVector DesiredPosToTarget = TargetLocalPos - DesiredLocalPos;

		// Find the largest vertical distance
		Size = FMath::Max(Size, FMath::Abs(DesiredPosToTarget.Z));

		// Find the largest horizontal distance    
		Size = FMath::Max(Size, FMath::Abs(DesiredPosToTarget.Y));

	}

	// Add the edge buffer   
	Size += ScreenEdgeBuffer;

	// Make sure the camera doesn't zoom in beyond the minimum size
	Size = FMath::Max(Size, MinSize);

	return Size;
}


void ADynamicCamera::FindAveragePosition()
{
	FVector averagePos = FVector::ZeroVector;
	int numTargets = 0;
	
	for (int i = 0; i < Targets.Num(); i++)
	{
		// If the target is invalid or destroyed, skip it.
		if (!IsValid(Targets[i]))
			continue;

		// If the target is hidden, skip it.
		if (Targets[i]->IsHidden())
			continue;

		// Add to the average and increment the target count.
		averagePos += Targets[i]->GetActorLocation();
		numTargets++;
	}
	
	// If there are targets, calculate the average position.
	if (numTargets > 0)
		averagePos /= numTargets;

	// Keep the same Z value.
	averagePos.Z = GetActorLocation().Z;

	// The desired position is the average position.
	DesiredPosition = averagePos;

}


void ADynamicCamera::SetStartPositionAndSize()
{
	// Find the desired position.
	FindAveragePosition();

	// Set the camera actor's position without damping.
	SetActorLocation(DesiredPosition);

	// Find and set the required size of the camera.
	float RequiredSize = FindRequiredHorizontalSize();

	Camera->SetOrthoWidth(RequiredSize);
	//bleh
}


