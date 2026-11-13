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
	float RequiredSize = FindRequiredHorizontalSize();

	float NewSize = FMath::FInterpTo(
		Camera->OrthoWidth,
		RequiredSize,
		DeltaTime,
		1.0f / HorizontalZoomDampTime
	);

	Camera->SetOrthoWidth(NewSize);
}


float ADynamicCamera::FindRequiredHorizontalSize()
{
	FTransform CameraTransform = Camera->GetComponentTransform();

	float MinY = FLT_MAX;
	float MaxY = -FLT_MAX;

	for (AActor* Target : Targets)
	{
		if (!IsValid(Target) || Target->IsHidden())
			continue;

		FVector LocalPosition = CameraTransform.InverseTransformPosition(
			Target->GetActorLocation()
		);

		MinY = FMath::Min(MinY, LocalPosition.Y);
		MaxY = FMath::Max(MaxY, LocalPosition.Y);
	}

	if (MinY == FLT_MAX)
		return MinSize;

	float Width = MaxY - MinY;

	return FMath::Max(
		Width + ScreenEdgeBuffer * 2.0f,
		MinSize
	);
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


