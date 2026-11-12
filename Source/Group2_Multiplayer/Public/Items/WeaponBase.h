#pragma once

#include "CoreMinimal.h"
#include "Items/ItemBase.h"
#include "WeaponBase.generated.h"

class UBoxComponent;
class UAnimMontage;

UENUM(BlueprintType)
enum class EWeaponGrip : uint8
{
	OneHanded UMETA(DisplayName = "1H"),
	TwoHanded UMETA(DisplayName = "2H")
};

UENUM(BlueprintType)
enum class EWeaponReach : uint8
{
	None UMETA(DisplayName = "N/A"),
	Short,
	Medium,
	Long
};

UENUM(BlueprintType)
enum class EWeaponAttackType : uint8
{
	None,
	Primary,
	Secondary
};

USTRUCT(BlueprintType)
struct FWeaponAttackData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	TObjectPtr<UAnimMontage> AttackMontage = nullptr;

	// can one swing hit more than one player
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	bool bCanHitMultiple = false;

	// horizontal knockback strength
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Knockback", meta = (ClampMin = "0.0"))
	float KnockbackForce = 1000.f;

	// upward force
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Knockback", meta = (ClampMin = "0.0"))
	float KnockbackUpForce = 200.f;

	// attack cooldown
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0.0", Units = "s"))
	float Cooldown = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Charge")
	bool bChargeable = false;

	// how long the button has to be held to reach full charge
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Charge", meta = (EditCondition = "bChargeable", ClampMin = "0.01", Units = "s"))
	float MaxChargeTime = 1.f;

	// knockback multiplier at full charge (1 = no bonus), linear scaling with charge
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Charge", meta = (EditCondition = "bChargeable", ClampMin = "1.0"))
	float FullChargeKnockbackMultiplier = 2.f;

	// area of effect knockback when fully charged (golden hammer)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Shockwave", meta = (EditCondition = "bChargeable"))
	bool bShockwaveOnFullCharge = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Shockwave", meta = (EditCondition = "bChargeable && bShockwaveOnFullCharge", ClampMin = "0.0", Units = "cm"))
	float ShockwaveRadius = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Shockwave", meta = (EditCondition = "bChargeable && bShockwaveOnFullCharge", ClampMin = "0.0"))
	float ShockwaveForce = 1500.f;
};


UCLASS(Abstract, Blueprintable)
class GROUP2_MULTIPLAYER_API AWeaponBase : public AItemBase
{
	GENERATED_BODY()

public:
	AWeaponBase();

protected:
	virtual void BeginPlay() override;

public:
	// the hit collider is separate from mesh (the mesh is used for pickup and physics)
	// off by default, only turned on between OpenHitWindow and CloseHitWindow so weapons
	// lying on the floor never knock anyone back
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	TObjectPtr<UBoxComponent> HitCollider;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|General")
	EWeaponGrip Grip = EWeaponGrip::OneHanded;

	// for picking animations, the actual reach is the HitCollider size
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|General")
	EWeaponReach Reach = EWeaponReach::Medium;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Attacks")
	FWeaponAttackData PrimaryAttack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Attacks")
	FWeaponAttackData SecondaryAttack;

	// secondary input blocks instead of attacking (for boxing gloves, shield)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Block")
	bool bSecondaryIsBlock = false;

	// 0 = no reduction, 1 = no knockback at all while blocking
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Block", meta = (EditCondition = "bSecondaryIsBlock", ClampMin = "0.0", ClampMax = "1.0"))
	float BlockKnockbackReduction = 0.5f;

	// 0-1 chance to stun on hit (frying pan would be 0.2), not stacking the duration is handled by the stunned player
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Stun", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StunChance = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Stun", meta = (ClampMin = "0.0", Units = "s"))
	float StunDuration = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Grapple")
	bool bCanGrapple = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Grapple", meta = (EditCondition = "bCanGrapple", ClampMin = "0.0", Units = "cm"))
	float LeashLength = 500.f;

	// how many stick wiggles the leashed player needs to break free
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Grapple", meta = (EditCondition = "bCanGrapple", ClampMin = "1"))
	int32 WigglesToBreakFree = 10;

	// destroy this weapon after a successful leash + knockout
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Grapple", meta = (EditCondition = "bCanGrapple"))
	bool bDestroyOnGrappleKO = true;

	// uses before the weapon breaks, 0 = unbreakable
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Durability", meta = (ClampMin = "0"))
	int32 MaxDurability = 10;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon|Durability")
	int32 CurrentDurability = 0;

	// (who is holding the weapon comes from ItemBase, use GetHolder)
	// player currently leashed by this weapon
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Transient, Category = "Weapon|State")
	TObjectPtr<AActor> GrappledActor = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Weapon|State")
	EWeaponAttackType CurrentAttack = EWeaponAttackType::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Weapon|State")
	bool bIsCharging = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Weapon|State")
	bool bIsBlocking = false;

	// actors already hit during the current swing, stops one swing hitting the same player twice
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Weapon|State")
	TArray<TObjectPtr<AActor>> HitActorsThisSwing;

	// call from an Anim Notify State begin, turns the HitCollider on and clears the hit list
	UFUNCTION(BlueprintCallable, Category = "Weapon|Attack")
	void OpenHitWindow(EWeaponAttackType AttackType);

	// call from an Anim Notify State end, turns the HitCollider off and resets the charge
	UFUNCTION(BlueprintCallable, Category = "Weapon|Attack")
	void CloseHitWindow();

	UFUNCTION(BlueprintPure, Category = "Weapon|Attack")
	FWeaponAttackData GetAttackData(EWeaponAttackType AttackType) const;

	// returns false if this attack isnt chargeable
	UFUNCTION(BlueprintCallable, Category = "Weapon|Charge")
	bool StartCharging(EWeaponAttackType AttackType);

	// locks in and returns the charge (0-1)
	UFUNCTION(BlueprintCallable, Category = "Weapon|Charge")
	float StopCharging();

	// live value while charging (for UI or VFX)
	UFUNCTION(BlueprintPure, Category = "Weapon|Charge")
	float GetChargeAlpha() const;

	UFUNCTION(BlueprintPure, Category = "Weapon|Charge")
	bool ShouldTriggerShockwave(EWeaponAttackType AttackType, float InChargeAlpha) const;

	// knockback vector to apply to target (Launch Character / Add Impulse in BP), pushes away from the holder
	UFUNCTION(BlueprintPure, Category = "Weapon|Knockback")
	FVector CalculateKnockback(const AActor* Target, EWeaponAttackType AttackType, float InChargeAlpha) const;

	// returns false if this weapon cant block
	UFUNCTION(BlueprintCallable, Category = "Weapon|Block")
	bool StartBlocking();

	UFUNCTION(BlueprintCallable, Category = "Weapon|Block")
	void StopBlocking();

	// call on the DEFENDERS weapon, multiply incoming knockback by this (1 when not blocking)
	UFUNCTION(BlueprintPure, Category = "Weapon|Block")
	float GetIncomingKnockbackMultiplier() const;

	// random roll against StunChance. not pure so it only rolls once per call
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Weapon|Stun")
	bool RollForStun() const;

	// returns true if this broke the weapon (also fires OnWeaponBroken)
	UFUNCTION(BlueprintCallable, Category = "Weapon|Durability")
	bool ReduceDurability(int32 Amount = 1);

	UFUNCTION(BlueprintPure, Category = "Weapon|Durability")
	bool IsBroken() const;

	// a valid player was hit during the hit window, apply knockback / stun / grapple / durability here
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon|Events")
	void OnWeaponHit(AActor* HitActor, UPrimitiveComponent* HitComponent, EWeaponAttackType AttackType, float InChargeAlpha);

	// durability hit 0, effects + Destroy Actor go here
	// (destroying while held is safe, the player character clears its held item when it becomes invalid)
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon|Events")
	void OnWeaponBroken();

private:
	UFUNCTION()
	void OnHitColliderBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	float CalculateChargeAlpha() const;

	// false outside the hit window, or after the first hit on a single target attack
	bool bHitWindowOpen = false;

	float ChargeAlpha = 0.f;
	float ChargeStartTime = 0.f;
	EWeaponAttackType ChargingAttack = EWeaponAttackType::None;
};
