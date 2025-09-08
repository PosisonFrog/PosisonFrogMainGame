// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/CAnimNotifyState_Combo.h"

#include "00_Character/02_Component/CWeaponComponent.h"
#include "GameFramework/Character.h"

void UCAnimNotifyState_Combo::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(MeshComp->GetOwner()))
	{
		if (UCWeaponComponent* WeaponComponent = OwnerCharacter->FindComponentByClass<UCWeaponComponent>())
		{
			WeaponComponent->EnableComboInput();
		}
	}
}

void UCAnimNotifyState_Combo::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(MeshComp->GetOwner()))
	{
		if (UCWeaponComponent* WeaponComponent = OwnerCharacter->FindComponentByClass<UCWeaponComponent>())
		{
			WeaponComponent->DisableComboInput();
		}
	}
}
