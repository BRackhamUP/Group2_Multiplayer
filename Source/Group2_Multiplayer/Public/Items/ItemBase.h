#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemBase.generated.h"

class UStaticMeshComponent;

// Designers create blueprint child and assign a mesh there
UCLASS(Abstract)
class GROUP2_MULTIPLAYER_API AItemBase : public AActor
{
	GENERATED_BODY()

public:
	AItemBase();

protected:
	// VisibleAnywhere to allow mesh, material and mass editable in blueprint children
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMeshComponent> Mesh;
};
