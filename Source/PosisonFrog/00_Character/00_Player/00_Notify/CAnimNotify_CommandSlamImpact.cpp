// Fill out your copyright notice in the Description page of Project Settings.


#include "CAnimNotify_CommandSlamImpact.h"
#include "04_Skill/CSkill_CommandLaunchSlam.h"
#include "GameFramework/Character.h"

void UCAnimNotify_CommandSlamImpact::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase*)
{
	if (!MeshComp) return;
	if (ACharacter* Ch = Cast<ACharacter>(MeshComp->GetOwner()))
		if (UCSkill_CommandLaunchSlam* Skill = Ch->FindComponentByClass<UCSkill_CommandLaunchSlam>())
			Skill->Anim_SlamImpact();
}