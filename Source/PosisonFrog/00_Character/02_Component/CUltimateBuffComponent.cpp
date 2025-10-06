// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/02_Component/CUltimateBuffComponent.h"

#include "00_Character/02_Component/CHealthComponent.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"


UCUltimateBuffComponent::UCUltimateBuffComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCUltimateBuffComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerChar = Cast<ACPlayerCharacter>(GetOwner());
	if (IsValid(OwnerChar))
	{
		MovementComponent = OwnerChar->GetCharacterMovement();
		if (MovementComponent)
			BaseMaxWalkSpeed = MovementComponent->MaxWalkSpeed;

		HealthComponent = OwnerChar->FindComponentByClass<UCHealthComponent>();
		if (HealthComponent)
			BaseMaxHealth = HealthComponent->GetMaxHealth();
	}
}

void UCUltimateBuffComponent::ActivateUltimate()
{
	ApplyAll();
}

void UCUltimateBuffComponent::DeactivateUltimate()
{
	RestoreAll();
}

void UCUltimateBuffComponent::ApplyAll()
{
	bIsActive = true;
	
	// 이동 속도
	if (MovementComponent)
	{
		MovementComponent->MaxWalkSpeed = BaseMaxWalkSpeed * MoveSpeedMul;
		UE_LOG(LogTemp, Log, TEXT("[ULT][On] MoveSpeed %.1f -> %.1f (x%.2f)"), BaseMaxWalkSpeed, MovementComponent->MaxWalkSpeed, MoveSpeedMul);
	}

	OutgoingDamageMul = DamageMul;
	UE_LOG(LogTemp, Log, TEXT("[ULT][On] OutgoingDamageMul=%.2f"), OutgoingDamageMul);
	
	// 최대 HP 배수 + 1회 회복
	if (HealthComponent)
	{
		const float NewMax = FMath::Max(1.0f, BaseMaxHealth * MaxHpMul);
		HealthComponent->SetMaxHealth(NewMax);

		if (bHealOnActivate)
		{
			float HealAmount = NewMax * HealPercent;
			HealthComponent->Healing(HealAmount);
			UE_LOG(LogTemp, Log, TEXT("[ULT][On] MaxHP %.1f -> %.1f, Heal=%.1f"), BaseMaxHealth, NewMax, HealAmount);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[ULT][On] MaxHP %.1f -> %.1f"), BaseMaxHealth, NewMax);
		}
	}

	switch (DefenseMode)
	{
	case EUltDefenseMode::PercentReduction:
		IncomingDamageScale = 1.0f - FMath::Clamp(DamageReduction01, 0.0f, 1.0f);
		break;
	case EUltDefenseMode::ArmorStat:
		IncomingDamageScale = FMath::Max(MinDamageScale, 1.0f / (1.0f + ArmorValue / ArmorK));
		break;
	default:
		break;
	}
	UE_LOG(LogTemp, Log, TEXT("[ULT][On] IncomingScale %.2f (Mode=%d)"), IncomingDamageScale, DefenseMode);
}

void UCUltimateBuffComponent::RestoreAll()
{
	bIsActive = false;
	
	if (MovementComponent && BaseMaxWalkSpeed > 0.0f)
	{
		MovementComponent->MaxWalkSpeed = BaseMaxWalkSpeed;
		UE_LOG(LogTemp, Log, TEXT("[ULT][Off] MoveSpeed restore -> %.1f"), BaseMaxWalkSpeed);
	}

	OutgoingDamageMul = DefaultDamageMultiplier;
	UE_LOG(LogTemp, Log, TEXT("[ULT][Off] OutDamageMul -> %.2f"), OutgoingDamageMul);
	

	if (HealthComponent && BaseMaxHealth > 0.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("[ULT][Off] MaxHp -> %.1f"), BaseMaxHealth);
		HealthComponent->SetMaxHealth(BaseMaxHealth);
	}

	IncomingDamageScale = 1.0f;
	UE_LOG(LogTemp, Log, TEXT("[ULT][Off] IncomingDamageScale -> %.2f"), IncomingDamageScale);
}