// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/00_Notify/CAnimNotifyState_PlayerAttack.h"

#include "00_Character/02_Component/CWeaponComponent.h"
#include "GameFramework/Character.h"

void UCAnimNotifyState_PlayerAttack::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (ACharacter* OwnerCharacter = Cast<ACharacter>(MeshComp->GetOwner()))
	{
		if (UCWeaponComponent* WeaponComponent = OwnerCharacter->FindComponentByClass<UCWeaponComponent>())
		{
			WeaponComponent->EnableAttackBoxCollider();
		}
	}
}

void UCAnimNotifyState_PlayerAttack::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(MeshComp->GetOwner()))
	{
		if (UCWeaponComponent* WeaponComponent = OwnerCharacter->FindComponentByClass<UCWeaponComponent>())
		{
			WeaponComponent->DisableAttackBoxCollider();
		}
	}
}
