#include "Spawning/ItemSpawnPoint.h"
#include "Items/ItemBase.h"
#include "Components/SceneComponent.h"

// Billboard and arrows for spawnpoint placing in editor only
#if WITH_EDITORONLY_DATA
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#endif

AItemSpawnPoint::AItemSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

#if WITH_EDITORONLY_DATA
	// CreateEditorOnlyDefaultSubobject returns null in non editor builds, hence the checks
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(RootComponent);
	}

	Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (Arrow)
	{
		Arrow->SetupAttachment(RootComponent);
		Arrow->ArrowColor = FColor::Yellow;
	}
#endif
}

bool AItemSpawnPoint::IsAvailable() const
{
	return bEnabled && !SpawnedItem.IsValid();
}

bool AItemSpawnPoint::CanSpawnItem(TSubclassOf<AItemBase> ItemClass) const
{
	if (!ItemClass)
	{
		return false;
	}

	if (AllowedItemClasses.IsEmpty())
	{
		return true;
	}

	for (const TSubclassOf<AItemBase>& Allowed : AllowedItemClasses)
	{
		if (Allowed && ItemClass->IsChildOf(Allowed))
		{
			return true;
		}
	}

	return false;
}

void AItemSpawnPoint::SetSpawnedItem(AItemBase* Item)
{
	// Stop listening to any previous item
	if (AItemBase* OldItem = SpawnedItem.Get())
	{
		OldItem->OnPickedUp.RemoveDynamic(this, &AItemSpawnPoint::HandleSpawnedItemPickedUp);
	}

	SpawnedItem = Item;

	if (Item)
	{
		Item->OnPickedUp.AddUniqueDynamic(this, &AItemSpawnPoint::HandleSpawnedItemPickedUp);
	}
}

void AItemSpawnPoint::HandleSpawnedItemPickedUp(AItemBase* Item, ACharacter* Holder)
{
	// Picked up = gone from this point, so the manager can spawn here again.
	// If the item is later dropped somewhere else it no longer belongs to this point
	if (Item && Item == SpawnedItem.Get())
	{
		Item->OnPickedUp.RemoveDynamic(this, &AItemSpawnPoint::HandleSpawnedItemPickedUp);
		SpawnedItem.Reset();
	}
}

void AItemSpawnPoint::SetSpawnPointEnabled(bool bNewEnabled)
{
	bEnabled = bNewEnabled;
}

FTransform AItemSpawnPoint::GetSpawnTransform() const
{
	FTransform SpawnTransform = GetActorTransform();
	SpawnTransform.AddToTranslation(FVector(0.f, 0.f, SpawnHeightOffset));
	return SpawnTransform;
}
