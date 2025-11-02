#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_StopRush.generated.h"

/**
 * Rush 패턴의 이동 종료를 알리는 AnimNotify
 * Rush_Execute State의 끝에 배치
 */
UCLASS()
class POSISONFROG_API UAnimNotify_StopRush : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};