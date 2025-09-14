#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CAnimNotifyState_ComboWindow.generated.h"

/**
 * 콤보 입력 창(윈도우)을 여닫는 노티파이-상태
 * - Begin: UCWeaponComponent::EnableComboInput()
 * - End  : UCWeaponComponent::DisableComboInput()
 * - 규약: 각 콤보 애님 클립(또는 섹션)에서 "입력이 유효해야 하는 구간"에 이 상태를 배치
 */
UCLASS(meta = (DisplayName = "PF_ComboWindow"))
class POSISONFROG_API UCAnimNotifyState_ComboWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** 개발 편의 디버그 로그 토글 */
	UPROPERTY(EditAnywhere, Category = "PF|Debug")
	bool bDebugLog = false;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};

