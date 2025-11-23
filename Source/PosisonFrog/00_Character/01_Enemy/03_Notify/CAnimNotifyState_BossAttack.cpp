#include "CAnimNotifyState_BossAttack.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/00_BossPattern/CBossPattern_BasicAttack.h" // 경로 확인 필수

void UCAnimNotifyState_BossAttack::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		if (ACEnemyBossCharacter* Boss = Cast<ACEnemyBossCharacter>(MeshComp->GetOwner()))
		{
			// 패턴 컴포넌트 가져오기
			if (UCBossPattern_BasicAttack* AttackComp = Boss->FindComponentByClass<UCBossPattern_BasicAttack>())
			{
				// 공격 시작 알림 (피격 목록 초기화)
				AttackComp->StartAttackCollision();
			}
		}
	}
}

void UCAnimNotifyState_BossAttack::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		if (ACEnemyBossCharacter* Boss = Cast<ACEnemyBossCharacter>(MeshComp->GetOwner()))
		{
			if (UCBossPattern_BasicAttack* AttackComp = Boss->FindComponentByClass<UCBossPattern_BasicAttack>())
			{
				// 매 프레임 충돌 검사 수행
				AttackComp->CheckAttackCollision();
			}
		}
	}
}