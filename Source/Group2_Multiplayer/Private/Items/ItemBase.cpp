#include "Items/ItemBase.h"
#include "Components/StaticMeshComponent.h"

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
}
