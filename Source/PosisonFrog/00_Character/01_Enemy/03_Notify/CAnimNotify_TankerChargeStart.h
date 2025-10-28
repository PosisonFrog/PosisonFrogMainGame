#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "CAnimNotify_TankerChargeStart.generated.h"

UCLASS()
class POSISONFROG_API UCAnimNotify_TankerChargeStart : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};
