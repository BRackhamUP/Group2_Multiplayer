#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemSpawnPoint.generated.h"

class AItemBase;
class UArrowComponent;
class UBillboardComponent;

UCLASS()
class GROUP2_MULTIPLAYER_API AItemSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AItemSpawnPoint();

	// BlueprintPure so it shows as a node without exec pins
	UFUNCTION(BlueprintPure, Category = "Item Spawning")
	bool IsAvailable() const;

	UFUNCTION(BlueprintPure, Category = "Item Spawning")
	bool CanSpawnItem(TSubclassOf<AItemBase> ItemClass) const;

	// Turn the point on/off at runtime (allows us to stop spawning items at spawn points that are not reachable anymore)
	UFUNCTION(BlueprintCallable, Category = "Item Spawning")
	void SetSpawnPointEnabled(bool bNewEnabled);

	UFUNCTION(BlueprintPure, Category = "Item Spawning")
	AItemBase* GetSpawnedItem() const { return SpawnedItem.Get(); }

	// C++ ONLY: ONLY the manager should claim a spawn point. Exposing
	// this to blueprint seems to desync the manager, do not touch please lol
	void SetSpawnedItem(AItemBase* Item) { SpawnedItem = Item; }

	// Where the item should appear (this actors transform plus a height offset)
	FTransform GetSpawnTransform() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawning")
	bool bEnabled = true;

	// Empty = any item may spawn here
	// Otherwise an item may spawn if it IS one of these classes
	// OR a child of one, allows every weapon in one entry
	// This is the basics of allowing certain spawn points to only
	// spawn certain items
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawning")
	TArray<TSubclassOf<AItemBase>> AllowedItemClasses;

	// Lifts spawn location so items dont spawn inside the floor
	// when designers place the point flush with ground
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawning", meta = (Units = "cm"))
	float SpawnHeightOffset = 50.f;

private:
	// The item currently occupying this point
	// Must use weak pointer, if the item is
	// destroyed (lava), this automatically becomes
	// null and the point is free again
	TWeakObjectPtr<AItemBase> SpawnedItem;

#if WITH_EDITORONLY_DATA
	// Editor only visuals for designers placing points
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;

	UPROPERTY()
	TObjectPtr<UArrowComponent> Arrow;
#endif
};
