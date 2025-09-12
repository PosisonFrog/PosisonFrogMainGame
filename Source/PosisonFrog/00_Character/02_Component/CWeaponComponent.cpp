// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/02_Component/CWeaponComponent.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/00_Player/02_Weapon/CHammer.h"
#include "GameFramework/Character.h"
#include "Global.h"

UCWeaponComponent::UCWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}


void UCWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACPlayerCharacter>(GetOwner());

	if (!IsValid(OwnerCharacter))
		return;

	if (!IsValid(HammerClass))
		return;

	SpawnWeapon();
}

void UCWeaponComponent::PlayComboAttack()
{
	if (!ComboMontages.IsValidIndex(CurrentCombo)) return;

	UAnimMontage* Montage = ComboMontages[CurrentCombo];
	OwnerCharacter->PlayAnimMontage(Montage);

	GetWorld()->GetTimerManager().ClearTimer(ComboResetTimer);
	GetWorld()->GetTimerManager().SetTimer(ComboResetTimer, this, &UCWeaponComponent::ResetCombo, ComboResetTime, false);
}

void UCWeaponComponent::ResetCombo()
{
	CurrentCombo = 0;
	bIsAttacking = false;
	bCanNextCombo = false;
}

void UCWeaponComponent::SpawnWeapon()
{
	if (!IsValid(OwnerCharacter) || !IsValid(HammerClass))
	{
		CLog::Log("WeaponComponent::SpawnWeapon - 필요한 참조가 유효하지 않음");
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		CLog::Log("WeaponComponent::SpawnWeapon - World가 유효하지 않음");
		return;
	}
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerCharacter;
	SpawnParams.Instigator = OwnerCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 해머 생성
	const FVector SpawnLocation = OwnerCharacter->GetActorLocation();
	const FRotator SpawnRotation = OwnerCharacter->GetActorRotation();

	Hammer = World->SpawnActor<ACHammer>(HammerClass, SpawnLocation, SpawnRotation, SpawnParams);
    
	if (!IsValid(Hammer))
	{
		CLog::Log("WeaponComponent::SpawnWeapon - 해머 생성 실패!");
		return;
	}

	Hammer->SetOwner(OwnerCharacter);

	AttachWeaponToCharacter();
    
	Hammer->DeactivateDamage();
}

void UCWeaponComponent::AttachWeaponToCharacter()
{
	if (!IsValid(Hammer) || !IsValid(OwnerCharacter))
		return;

	USkeletalMeshComponent* CharacterMesh = OwnerCharacter->GetMesh();
	if (!IsValid(CharacterMesh))
		return;
	
	if (!CharacterMesh->DoesSocketExist(AttachSocketName))
		AttachSocketName = NAME_None;
	
	bool bAttachSuccess = Hammer->AttachToComponent(
		CharacterMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		AttachSocketName
	);
}

void UCWeaponComponent::DoAttack()
{
	if (!OwnerCharacter)
		return;

	if (bIsAttacking)
	{
		if (bCanNextCombo && CurrentCombo < ComboMontages.Num() - 1)
		{
			++CurrentCombo;
			bCanNextCombo = false;
			PlayComboAttack();
		}

		return;
	}

	CurrentCombo = 0;
	bIsAttacking = true;
	PlayComboAttack();
}

void UCWeaponComponent::BeginAction()
{
	bIsAttacking = true;
}

void UCWeaponComponent::EndAction()
{
	ResetCombo();
}

void UCWeaponComponent::EnableComboInput()
{
	bCanNextCombo = true;
}

void UCWeaponComponent::DisableComboInput()
{
	bCanNextCombo = false;
}

void UCWeaponComponent::EnableAttackBoxCollider()
{
	if (Hammer)
	{
		Hammer->ActivateDamage();
	}
}

void UCWeaponComponent::DisableAttackBoxCollider()
{
	if (Hammer)
	{
		Hammer->DeactivateDamage();
	}
}
