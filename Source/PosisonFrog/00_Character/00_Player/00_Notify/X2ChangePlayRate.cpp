#include "X2ChangePlayRate.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h" // 몽타주 헤더 필요

void UX2ChangePlayRate::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp) return;
	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	if (!AnimInst) return;

	if (UAnimMontage* CurrentMontage = Cast<UAnimMontage>(Animation))
	{
		AnimInst->Montage_SetPlayRate(CurrentMontage, PlayRate);
	}
}

void UX2ChangePlayRate::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp) return;
	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	if (!AnimInst) return;

	if (UAnimMontage* CurrentMontage = Cast<UAnimMontage>(Animation))
	{
		AnimInst->Montage_SetPlayRate(CurrentMontage, 1.0f);
	}
}