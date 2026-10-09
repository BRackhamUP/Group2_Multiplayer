#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemSpawnManager.generated.h"

class AItemBase;
class AItemSpawnPoint;

UCLASS()
class GROUP2_MULTIPLAYER_API AItemSpawnManager : public AActor
{
	GENERATED_BODY()

public:
	AItemSpawnManager();

	// Spawn one item at a random free, compatible spawn point
	// Will return nullptr if nothing could spawn
	UFUNCTION(BlueprintCallable, Category = "Item Spawning")
	AItemBase* SpawnItem();

	// Spawn a specific item at a specific point
	UFUNCTION(BlueprintCallable, Category = "Item Spawning")
	AItemBase* SpawnItemAtPoint(AItemSpawnPoint* SpawnPoint, TSubclassOf<AItemBase> ItemClass);

	// Start/stop the spawn timer (round start/end)
	UFUNCTION(BlueprintCallable, Category = "Item Spawning")
	void StartSpawning();

	UFUNCTION(BlueprintCallable, Category = "Item Spawning")
	void StopSpawning();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Item Spawning|Setup")
	TArray<TObjectPtr<AItemSpawnPoint>> SpawnPoints;

	// If SpawnPoints is left empty, gather every placed point in the level on BeginPlay
	// This will save designers manually assigning every point
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawning|Setup")
	bool bAutoCollectSpawnPoints = true;

	// Which item types this arena can spawn (in case we do a second level)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawning|Items")
	TArray<TSubclassOf<AItemBase>> ItemClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawning|Timing")
	bool bStartSpawningOnBeginPlay = true;

	// Delay before the first spawn (change as needed)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawning|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float InitialSpawnDelay = 2.f;

	// Time between spawn attempts. Using 0 = spawn only once
	// Spawns are capped by the number of free spawn points
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Spawning|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float SpawnInterval = 10.f;

private:
	TArray<AItemSpawnPoint*> GetAvailableSpawnPoints() const;

	void HandleSpawnTimer();

	FTimerHandle SpawnTimerHandle;
};
