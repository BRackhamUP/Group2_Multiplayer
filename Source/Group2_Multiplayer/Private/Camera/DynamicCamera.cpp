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
	FVector newPosition = FMath::VInterpTo(CurrentPosition,DesiredPosition,DeltaTime, 1.0f/ DampTime);
	
	Camera->SetWorldLocation(newPosition);
}

void ADynamicCamera::Zoom(float DeltaTime)
{
	// Find the required size based on the desired position.
	float RequiredSize = FindRequiredSize();

	// Smoothly transition to the required size.
	float NewSize = FMath::FInterpTo(
		Camera->OrthoWidth,
		RequiredSize,
		DeltaTime,
		1.0f/ DampTime
	);

	Camera->SetOrthoWidth(NewSize);
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

float ADynamicCamera::FindRequiredSize()
{// Find the position the camera is moving towards in local space.
	FVector DesiredLocalPos = GetActorTransform().InverseTransformPosition(DesiredPosition);

	// Start the camera's size calculation at zero.
	float Size = 0.0f;

	float HalfWidth = 0.0f;
    float HalfHeight = 0.0f;

    for (int i = 0; i < Targets.Num(); i++)
    {
        if (!IsValid(Targets[i]) || Targets[i]->IsHidden())
            continue;

        FVector TargetLocalPos =
            GetActorTransform().InverseTransformPosition(
                Targets[i]->GetActorLocation()
            );

        FVector RelativePos = TargetLocalPos - DesiredLocalPos;

        // Find the maximum horizontal distance.
        HalfWidth = FMath::Max(HalfWidth, FMath::Abs(RelativePos.Y));

        // Find the maximum vertical distance.
        HalfHeight = FMath::Max(HalfHeight, FMath::Abs(RelativePos.Z));
    }

    // Account for the camera's aspect ratio.
    float AspectRatio = Camera->AspectRatio;

    // Convert vertical distance into the equivalent horizontal distance.
    float RequiredWidth = FMath::Max(
        HalfWidth,
        HalfHeight * AspectRatio
    );

    // Add the edge buffer and account for the full width.
    RequiredWidth = (RequiredWidth + ScreenEdgeBuffer) * 2.0f;

    // Respect minimum camera size.
    RequiredWidth = FMath::Max(RequiredWidth, MinSize);

    return RequiredWidth;

	// Add the edge buffer.
	Size += ScreenEdgeBuffer;

	// Make sure the camera doesn't zoom in beyond the minimum size.
	Size = FMath::Max(Size, MinSize);

	return Size;
}

void ADynamicCamera::SetStartPositionAndSize()
{
	// Find the desired position.
	FindAveragePosition();

	// Set the camera actor's position without damping.
	SetActorLocation(DesiredPosition);

	// Find and set the required size of the camera.
	float RequiredSize = FindRequiredSize();

	Camera->SetOrthoWidth(RequiredSize);
}


