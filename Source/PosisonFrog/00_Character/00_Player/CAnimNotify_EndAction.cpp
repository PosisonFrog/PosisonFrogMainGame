// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/CAnimNotify_EndAction.h"

#include "00_Character/02_Component/CWeaponComponent.h"

void UCAnimNotify_EndAction::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp)
		return;

	if (AActor* Owner = MeshComp->GetOwner())
	{
		if (UCWeaponComponent* WeaponComponent = Owner->FindComponentByClass<UCWeaponComponent>())
		{
			WeaponComponent->EndAction();
		}
	}
}
