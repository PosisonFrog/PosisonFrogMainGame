// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/00_Notify/CAnimNotify_PlayEffectByUltState.h"

#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/00_Player/02_Weapon/CHammer.h"

UCAnimNotify_PlayEffectByUltState::UCAnimNotify_PlayEffectByUltState()
{
}

void UCAnimNotify_PlayEffectByUltState::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp)
	{
		return;
	}

	// 궁극기 상태이고 궁극기 이펙트가 설정되어 있으면 임시로 교체
	UNiagaraSystem* OriginalTemplate = Template;
    
	if (IsPlayerUltimateActive(MeshComp) && UltimateEffect)
	{
		// 부모 클래스의 Template을 궁극기 이펙트로 임시 교체
		Template = UltimateEffect;
	}

	// 실제 이펙트 스폰 부모 -> 클래스의 Notify 호출
	// 이렇게 하면 소켓, 오프셋, 스케일 등 모든 설정이 그대로 적용됨
	Super::Notify(MeshComp, Animation, EventReference);

	// 다음 호출을 위해 원래 Template 복원
	Template = OriginalTemplate;
}

bool UCAnimNotify_PlayEffectByUltState::IsPlayerUltimateActive(USkeletalMeshComponent* MeshComp) const
{
	if (!MeshComp)
	{
		return false;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return false;
	}

	// 플레이어 캐릭터 확인
	if (ACPlayerCharacter* Player = Cast<ACPlayerCharacter>(Owner))
	{
		// IBuffable 인터페이스를 통해 궁극기 상태 확인
		return Player->IsBuffActive();
	}

	// Hammer 메시인 경우 플레이어를 찾아서 확인
	if (ACHammer* Hammer = Cast<ACHammer>(Owner))
	{
		// Hammer의 Owner는 보통 Player
		if (ACPlayerCharacter* Player = Cast<ACPlayerCharacter>(Hammer->GetOwner()))
		{
			return Player->IsBuffActive();
		}
	}

	return false;
}
