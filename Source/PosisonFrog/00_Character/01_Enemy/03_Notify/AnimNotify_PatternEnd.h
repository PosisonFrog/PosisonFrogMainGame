#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_PatternEnd.generated.h"

/**
 * 보스 패턴 종료를 알리는 AnimNotify
 * 모든 보스 공격 애니메이션 끝에 배치
 */
UCLASS()
class POSISONFROG_API UAnimNotify_PatternEnd : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	/** 패턴 성공 여부 (기본: true) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern")
	bool bSuccess = true;
};