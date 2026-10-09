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
