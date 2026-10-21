#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemBase.generated.h"

class UStaticMeshComponent;
class ACharacter;
class AItemBase;

// when a player picks an item up, fire this. Spawn points listen to this so they
// free themselves for new spawns.
// designers can bind to it in Blueprint for pickup sounds
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemPickedUp, AItemBase*, Item, ACharacter*, Holder);

// Designers create blueprint child and assign a mesh there
UCLASS(Abstract)
class GROUP2_MULTIPLAYER_API AItemBase : public AActor
{
	GENERATED_BODY()

public:
	AItemBase();

	// true when player is holding this item
	UFUNCTION(BlueprintPure, Category = "Item")
	bool IsHeld() const { return Holder != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Item")
	ACharacter* GetHolder() const { return Holder; }

	// name shown in UI prompts. If display name is empty, fallback to the class name
	UFUNCTION(BlueprintPure, Category = "Item")
	FText GetItemDisplayName() const;

	// C++ ONLY: called by the player character when it equips/drops this item
	// not exposed to BP so the held item cant get out of sync with the item
	// turns off physics + collision and attaches the item to the holder
	void AttachToHolder(ACharacter* NewHolder, USceneComponent* AttachParent, FName SocketName);

	// detaches the item, places it at DropLocation and turns physics + collision back on
	void DetachFromHolder(const FVector& DropLocation);

	UPROPERTY(BlueprintAssignable, Category = "Item")
	FOnItemPickedUp OnPickedUp;

protected:
	virtual void BeginPlay() override;

	// VisibleAnywhere for BP children
	// NOTE: pickup detection relies on the mesh using the PhysicsBody object type
	// (I have made sure the default PhysicsActor collision profile does this)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// used for the swap prompt
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	// item despawn time in seconds. 0 = never.
	// the timer only runs while the item is on the floor, picking it up cancels it and
	// dropping it starts a fresh countdown
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Despawn", meta = (ClampMin = "0.0", Units = "s"))
	float DespawnTime = 0.f;

private:
	// who is holding the item (null when laying in the arena)
	UPROPERTY(Transient)
	TObjectPtr<ACharacter> Holder;

	// collision state before pickup, restored on drop so BP changes are not lost
	TEnumAsByte<ECollisionEnabled::Type> CollisionBeforePickup = ECollisionEnabled::QueryAndPhysics;

	// start despawn countdown again, only called when item is on the floor
	void StartDespawnTimer();

	void HandleDespawnTimer();

	FTimerHandle DespawnTimerHandle;
};
