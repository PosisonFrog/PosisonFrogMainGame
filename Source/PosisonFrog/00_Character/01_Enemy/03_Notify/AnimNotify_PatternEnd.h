#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_PatternEnd.generated.h"

/**
 * 패턴 종료를 알리는 범용 AnimNotify
 * 모든 패턴의 Recovery State 끝에 배치
 */
UCLASS()
class POSISONFROG_API UAnimNotify_PatternEnd : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;

	/** 패턴 성공 여부 (기본값: true) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern")
	bool bSuccess = true;
};
