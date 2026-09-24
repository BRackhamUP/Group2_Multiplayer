#include "MyClass.h"
#include "Engine/Engine.h"

AMyClass::AMyClass()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AMyClass::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("C++ BeginPlay executed successfully."));

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            5.0f,
            FColor::Green,
            TEXT("C++ is working! Brad is the goat")
        );
    }
}