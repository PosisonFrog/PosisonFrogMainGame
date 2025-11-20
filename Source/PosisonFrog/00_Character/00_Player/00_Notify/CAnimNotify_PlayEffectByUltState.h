// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AnimNotify_PlayNiagaraEffect.h"
#include "CAnimNotify_PlayEffectByUltState.generated.h"

/**
 * 
 */
UCLASS()
class POSISONFROG_API UCAnimNotify_PlayEffectByUltState : public UAnimNotify_PlayNiagaraEffect
{
	GENERATED_BODY()

public:
	UCAnimNotify_PlayEffectByUltState();

	// ─────────── Notify 실행 ───────────
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	// ─────────── 에디터 설정 ───────────
	// 궁극기 활성화 시 재생할 이펙트 (nullptr이면 일반 이펙트만 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara System", meta = (DisplayName = "Ultimate Effect Template"))
	TObjectPtr<UNiagaraSystem> UltimateEffect = nullptr;

private:
	// 플레이어가 궁극기 상태인지 확인
	bool IsPlayerUltimateActive(USkeletalMeshComponent* MeshComp) const;
};
