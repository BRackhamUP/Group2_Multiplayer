#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyClass.generated.h"

UCLASS()
class GROUP2_MULTIPLAYER_API AMyClass : public AActor
{
    GENERATED_BODY()

public:
    AMyClass();

protected:
    virtual void BeginPlay() override;
};