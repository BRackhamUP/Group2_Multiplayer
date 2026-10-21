#include "Items/ItemBase.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

AItemBase::AItemBase()
{
	// Items shouldnt need to tick, turn it on in a child class if one needs it
	PrimaryActorTick.bCanEverTick = false;

	// Keep physics mesh as the root, otherwise the
	// actors transform and the mesh position drift apart
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);

	// static meshes do not generate overlap events by default.
	// the player pickup sphere needs them to detect items, please do not edit or remove this.
	Mesh->SetGenerateOverlapEvents(true);
}

void AItemBase::BeginPlay()
{
	Super::BeginPlay();

	// countdown starts as soon as the item lands in the arena
	StartDespawnTimer();
}

void AItemBase::StartDespawnTimer()
{
	if (DespawnTime > 0.f && !IsHeld())
	{
		// SetTimer on an existing handle replaces it, so this also acts as resetting the countdown
		GetWorldTimerManager().SetTimer(DespawnTimerHandle, this, &AItemBase::HandleDespawnTimer, DespawnTime, false);
	}
}

void AItemBase::HandleDespawnTimer()
{
	// safety net: never despawn a weapon being held
	if (!IsHeld())
	{
		Destroy();
	}
}

FText AItemBase::GetItemDisplayName() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}

	// prompt player for swapping weapon, this is a lazy solution for now lol
	// for example BP_FryingPan_C becomes BP_FryingPan
	FString ClassName = GetClass()->GetName();
	ClassName.RemoveFromEnd(TEXT("_C"));
	return FText::FromString(ClassName);
}

void AItemBase::AttachToHolder(ACharacter* NewHolder, USceneComponent* AttachParent, FName SocketName)
{
	if (!NewHolder || !AttachParent || IsHeld())
	{
		return;
	}

	Holder = NewHolder;
	SetOwner(NewHolder);

	// held weapons never despawn
	GetWorldTimerManager().ClearTimer(DespawnTimerHandle);

	// physics must be off before attaching, a simulating body ignores its attach parent
	CollisionBeforePickup = Mesh->GetCollisionEnabled();
	Mesh->SetSimulatePhysics(false);

	// no collision while held to stop held item pushing player, and other
	// player pickup spheres stop seeing it (so nobody can grab a held weapon)
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// snap location/rotation to the socket but keep the item scale
	AttachToComponent(AttachParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);

	// lets the spawn point that spawned this item know it is free again
	OnPickedUp.Broadcast(this, NewHolder);
}

void AItemBase::DetachFromHolder(const FVector& DropLocation)
{
	if (!IsHeld())
	{
		return;
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Holder = nullptr;
	SetOwner(nullptr);

	// keep the yaw so it lands facing roughly the way it was held (fixes dropped items looking like they bug out)
	SetActorLocationAndRotation(DropLocation, FRotator(0.f, GetActorRotation().Yaw, 0.f),
		false, nullptr, ETeleportType::TeleportPhysics);

	Mesh->SetCollisionEnabled(CollisionBeforePickup);
	Mesh->SetSimulatePhysics(true);

	// item is on the floor, start despawn timer again
	StartDespawnTimer();
}
