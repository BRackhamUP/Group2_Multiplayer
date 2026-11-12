#include "Items/WeaponBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"

AWeaponBase::AWeaponBase()
{
	// ItemBase sets tick off, if a weapon needs it turn on in its bp

	// attach to ItemBase root so it follows the weapon in the holders hand
	HitCollider = CreateDefaultSubobject<UBoxComponent>(TEXT("HitCollider"));
	HitCollider->SetupAttachment(Mesh);
	HitCollider->SetBoxExtent(FVector(10.f, 10.f, 40.f));

	// only overlaps Pawn, WorldDynamic type so the pickup spheres
	// never see it (they only look for PhysicsBody)
	HitCollider->SetCollisionObjectType(ECC_WorldDynamic);
	HitCollider->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitCollider->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	HitCollider->SetGenerateOverlapEvents(true);
	HitCollider->SetCanEverAffectNavigation(false);

	// off until a swing opens the hit window
	HitCollider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();

	CurrentDurability = MaxDurability;

	HitCollider->OnComponentBeginOverlap.AddDynamic(this, &AWeaponBase::OnHitColliderBeginOverlap);
}

void AWeaponBase::OpenHitWindow(EWeaponAttackType AttackType)
{
	if (AttackType == EWeaponAttackType::None)
	{
		return;
	}

	CurrentAttack = AttackType;
	HitActorsThisSwing.Reset();
	bHitWindowOpen = true;

	// turning collision on also fires overlaps for players already inside the collider
	HitCollider->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AWeaponBase::CloseHitWindow()
{
	bHitWindowOpen = false;
	HitCollider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CurrentAttack = EWeaponAttackType::None;
	ChargeAlpha = 0.f;
}

FWeaponAttackData AWeaponBase::GetAttackData(EWeaponAttackType AttackType) const
{
	return AttackType == EWeaponAttackType::Secondary ? SecondaryAttack : PrimaryAttack;
}

void AWeaponBase::OnHitColliderBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bHitWindowOpen || !IsValid(OtherActor))
	{
		return;
	}

	// ignore ourselves and any other items
	if (OtherActor == this || OtherActor == GetHolder() || OtherActor->IsA<AItemBase>())
	{
		return;
	}

	// unreal fires overlap for each component that enters the box, this
	// fixes one swing applying multiple knockbacks
	if (HitActorsThisSwing.Contains(OtherActor))
	{
		return;
	}

	HitActorsThisSwing.Add(OtherActor);

	const EWeaponAttackType AttackType = CurrentAttack;

	// ignore everything else until the next swing
	// collider stays on until CloseHitWindow
	if (!GetAttackData(AttackType).bCanHitMultiple)
	{
		bHitWindowOpen = false;
	}

	OnWeaponHit(OtherActor, OtherComp, AttackType, ChargeAlpha);
}

bool AWeaponBase::StartCharging(EWeaponAttackType AttackType)
{
	if (AttackType == EWeaponAttackType::None || !GetAttackData(AttackType).bChargeable)
	{
		return false;
	}

	ChargingAttack = AttackType;
	ChargeStartTime = GetWorld()->GetTimeSeconds();
	ChargeAlpha = 0.f;
	bIsCharging = true;
	return true;
}

float AWeaponBase::StopCharging()
{
	if (bIsCharging)
	{
		ChargeAlpha = CalculateChargeAlpha();
		bIsCharging = false;
	}

	return ChargeAlpha;
}

float AWeaponBase::GetChargeAlpha() const
{
	return bIsCharging ? CalculateChargeAlpha() : ChargeAlpha;
}

float AWeaponBase::CalculateChargeAlpha() const
{
	const float MaxTime = FMath::Max(GetAttackData(ChargingAttack).MaxChargeTime, KINDA_SMALL_NUMBER);
	const float HeldTime = GetWorld()->GetTimeSeconds() - ChargeStartTime;
	return FMath::Clamp(HeldTime / MaxTime, 0.f, 1.f);
}

bool AWeaponBase::ShouldTriggerShockwave(EWeaponAttackType AttackType, float InChargeAlpha) const
{
	const FWeaponAttackData Attack = GetAttackData(AttackType);
	return Attack.bChargeable && Attack.bShockwaveOnFullCharge && InChargeAlpha >= 1.f;
}

FVector AWeaponBase::CalculateKnockback(const AActor* Target, EWeaponAttackType AttackType, float InChargeAlpha) const
{
	if (!IsValid(Target))
	{
		return FVector::ZeroVector;
	}

	const FWeaponAttackData Attack = GetAttackData(AttackType);

	// push away from the holder, or the weapon itself if nobody is holding it
	// I had some crashes where GetHolder would return null when dropping or swapping mid swing
	// this fixes that
	const AActor* Source = GetHolder() ? static_cast<const AActor*>(GetHolder()) : this;

	// flattened so knockback is horizontal, upward force is added separately
	FVector Direction = Target->GetActorLocation() - Source->GetActorLocation();
	Direction.Z = 0.f;
	Direction = Direction.GetSafeNormal();

	if (Direction.IsNearlyZero())
	{
		Direction = Source->GetActorForwardVector();
	}

	const float ChargeMultiplier = Attack.bChargeable
		? FMath::Lerp(1.f, Attack.FullChargeKnockbackMultiplier, FMath::Clamp(InChargeAlpha, 0.f, 1.f))
		: 1.f;

	return Direction * Attack.KnockbackForce * ChargeMultiplier + FVector::UpVector * Attack.KnockbackUpForce;
}

bool AWeaponBase::StartBlocking()
{
	if (!bSecondaryIsBlock)
	{
		return false;
	}

	bIsBlocking = true;
	return true;
}

void AWeaponBase::StopBlocking()
{
	bIsBlocking = false;
}

float AWeaponBase::GetIncomingKnockbackMultiplier() const
{
	return bIsBlocking ? 1.f - BlockKnockbackReduction : 1.f;
}

bool AWeaponBase::RollForStun() const
{
	return StunChance > 0.f && FMath::FRand() < StunChance;
}

bool AWeaponBase::ReduceDurability(int32 Amount)
{
	if (MaxDurability <= 0 || Amount <= 0 || IsBroken())
	{
		return false;
	}

	CurrentDurability = FMath::Max(CurrentDurability - Amount, 0);

	if (CurrentDurability == 0)
	{
		OnWeaponBroken();
		return true;
	}

	return false;
}

bool AWeaponBase::IsBroken() const
{
	return MaxDurability > 0 && CurrentDurability <= 0;
}
