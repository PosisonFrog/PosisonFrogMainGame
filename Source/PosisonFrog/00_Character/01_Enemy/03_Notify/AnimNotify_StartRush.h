#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_StartRush.generated.h"

/**
 * Rush 패턴의 이동 시작을 알리는 AnimNotify
 * Rush_Telegraph State의 끝에 배치
 */
UCLASS()
class POSISONFROG_API UAnimNotify_StartRush : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};