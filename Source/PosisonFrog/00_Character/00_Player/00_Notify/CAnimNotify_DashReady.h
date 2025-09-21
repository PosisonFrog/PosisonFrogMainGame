#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "CAnimNotify_DashReady.generated.h"

/** 공격 모션 종료 직전(마지막 몇 프레임)에 배치하여
 *  대시 버퍼를 즉시 소모하게 하는 Notify
 */
UCLASS()
class POSISONFROG_API UCAnimNotify_DashReady : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};
