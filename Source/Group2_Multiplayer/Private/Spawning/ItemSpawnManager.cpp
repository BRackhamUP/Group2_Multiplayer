#include "Spawning/ItemSpawnManager.h"
#include "Spawning/ItemSpawnPoint.h"
#include "Items/ItemBase.h"
#include "EngineUtils.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogItemSpawning, Log, All); // Temp logging for debugging

AItemSpawnManager::AItemSpawnManager()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AItemSpawnManager::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoCollectSpawnPoints && SpawnPoints.IsEmpty())
	{
		for (TActorIterator<AItemSpawnPoint> It(GetWorld()); It; ++It)
		{
			SpawnPoints.Add(*It);
		}
	}

	// Trying to be designer friendly
	if (SpawnPoints.IsEmpty())
	{
		UE_LOG(LogItemSpawning, Warning, TEXT("%s has no spawn points"), *GetName());
	}
	if (ItemClasses.IsEmpty())
	{
		UE_LOG(LogItemSpawning, Warning, TEXT("%s has no item classes assigned"), *GetName());
	}

	if (bStartSpawningOnBeginPlay)
	{
		StartSpawning();
	}
}

void AItemSpawnManager::StartSpawning()
{
	FTimerManager& TimerManager = GetWorldTimerManager();
	TimerManager.ClearTimer(SpawnTimerHandle);

	if (SpawnInterval > 0.f)
	{
		// Looping timer: first fire after InitialSpawnDelay, then every SpawnInterval
		TimerManager.SetTimer(SpawnTimerHandle, this, &AItemSpawnManager::HandleSpawnTimer,
			SpawnInterval, true, InitialSpawnDelay);
	}
	else if (InitialSpawnDelay > 0.f)
	{
		// After a delay
		TimerManager.SetTimer(SpawnTimerHandle, this, &AItemSpawnManager::HandleSpawnTimer,
			InitialSpawnDelay, false);
	}
	else
	{
		// Immediately spawn
		HandleSpawnTimer();
	}
}

void AItemSpawnManager::StopSpawning()
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
}

void AItemSpawnManager::HandleSpawnTimer()
{
	SpawnItem();
}

AItemBase* AItemSpawnManager::SpawnItem()
{
	TArray<AItemSpawnPoint*> Candidates = GetAvailableSpawnPoints();

	// Try free points in random order until one accepts at least one item class
	while (!Candidates.IsEmpty())
	{
		const int32 PointIndex = FMath::RandRange(0, Candidates.Num() - 1);
		AItemSpawnPoint* SpawnPoint = Candidates[PointIndex];
		Candidates.RemoveAtSwap(PointIndex);

		TArray<TSubclassOf<AItemBase>> CompatibleClasses;
		for (const TSubclassOf<AItemBase>& ItemClass : ItemClasses)
		{
			if (SpawnPoint->CanSpawnItem(ItemClass))
			{
				CompatibleClasses.Add(ItemClass);
			}
		}

		if (CompatibleClasses.IsEmpty())
		{
			continue;
		}

		// Equal chance random choice for now, can replace this when adding rarity spawns
		const TSubclassOf<AItemBase> ChosenClass =
			CompatibleClasses[FMath::RandRange(0, CompatibleClasses.Num() - 1)];

		return SpawnItemAtPoint(SpawnPoint, ChosenClass);
	}

	// Every point is full or disabled
	return nullptr;
}

AItemBase* AItemSpawnManager::SpawnItemAtPoint(AItemSpawnPoint* SpawnPoint, TSubclassOf<AItemBase> ItemClass)
{
	if (!IsValid(SpawnPoint) || !ItemClass)
	{
		return nullptr;
	}

	if (!SpawnPoint->IsAvailable() || !SpawnPoint->CanSpawnItem(ItemClass))
	{
		UE_LOG(LogItemSpawning, Verbose, TEXT("Spawn point %s can't take %s."),
			*SpawnPoint->GetName(), *ItemClass->GetName());
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	// If a player is standing on the point, nudge item rather than failing spawn
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AItemBase* NewItem = GetWorld()->SpawnActor<AItemBase>(
		ItemClass, SpawnPoint->GetSpawnTransform(), SpawnParams);

	if (NewItem)
	{
		SpawnPoint->SetSpawnedItem(NewItem);
		// Dont remove this log until we are bug fixing and polishing
		UE_LOG(LogItemSpawning, Log, TEXT("Spawned %s at %s."),
			*NewItem->GetName(), *SpawnPoint->GetName());
	}

	return NewItem;
}

TArray<AItemSpawnPoint*> AItemSpawnManager::GetAvailableSpawnPoints() const
{
	TArray<AItemSpawnPoint*> Available;
	for (AItemSpawnPoint* SpawnPoint : SpawnPoints)
	{
		if (IsValid(SpawnPoint) && SpawnPoint->IsAvailable())
		{
			Available.Add(SpawnPoint);
		}
	}
	return Available;
}
