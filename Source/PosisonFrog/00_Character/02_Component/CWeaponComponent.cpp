// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/02_Component/CWeaponComponent.h"

#include "GameFramework/Character.h"

UCWeaponComponent::UCWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}


void UCWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacter>(GetOwner());
}


// Called every frame
void UCWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	
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
