// Fill out your copyright notice in the Description page of Project Settings.


#include "CAnimNotify_CommandDoLaunch.h"

#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/02_Component/00_PlayerComponent/CPlayerEffectComponent.h"
#include "04_Skill/CSkill_CommandLaunchSlam.h"
#include "GameFramework/Character.h"

void UCAnimNotify_CommandDoLaunch::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp || !MeshComp->GetOwner())
        return;

    ACPlayerCharacter* PlayerChar = Cast<ACPlayerCharacter>(MeshComp->GetOwner());
    if (!PlayerChar)
        return;

    // 기존 커맨드 스킬 실행
    if (UCSkill_CommandLaunchSlam* CommandComp = PlayerChar->FindComponentByClass<UCSkill_CommandLaunchSlam>())
    {
        CommandComp->Anim_PerformLaunch();
    }
}