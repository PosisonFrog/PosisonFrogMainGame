#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "X2ChangePlayRate.generated.h"

/**
 * 몽타주 특정 구간의 재생 속도를 변경하는 노티파이 스테이트
 */
UCLASS()
class UX2ChangePlayRate : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// 에디터에서 설정할 목표 재생 속도 (예: 2.0 = 2배속)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	float PlayRate = 2.0f;

	// 노티파이 구간 시작 시 호출
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	// 노티파이 구간 종료 시 호출
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};