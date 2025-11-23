#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CAnimNotifyState_BossAttack.generated.h"

/**
 * 보스 공격 판정 구간을 제어하는 노티파이 스테이트
 */
UCLASS()
class POSISONFROG_API UCAnimNotifyState_BossAttack : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// 구간 시작 시 (Hit 리스트 초기화)
	virtual void NotifyBegin(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	// 구간 지속 중 매 프레임 (충돌 감지 시도)
	virtual void NotifyTick(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
};