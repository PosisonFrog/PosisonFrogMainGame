// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AnimNotify_PlayNiagaraEffect.h"
#include "CAnimNotify_PlayEffectByUltState.generated.h"

// ───────── 이펙트 설정 구조체 ───────────
USTRUCT(BlueprintType)
struct FEffectSettings
{
	GENERATED_BODY()

	// 나이아가라 이펙트 템플릿
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UNiagaraSystem> EffectTemplate = nullptr;

	// 위치 오프셋
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FVector LocationOffset = FVector::ZeroVector;

	// 회전 오프셋
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FRotator RotationOffset = FRotator::ZeroRotator;

	// 스케일
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FVector ScaleOffset = FVector(1.0f, 1.0f, 1.0f);
};

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
	// 일반 상태 이펙트 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara System", meta = (DisplayName = "Normal Effect Template"))
	FEffectSettings NormalEffect;
	
	// 궁극기 활성화 시 재생할 이펙트 (nullptr이면 일반 이펙트만 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara System", meta = (DisplayName = "Ultimate Effect Template"))
	FEffectSettings UltimateEffect;

private:
	// 플레이어가 궁극기 상태인지 확인
	bool IsPlayerUltimateActive(USkeletalMeshComponent* MeshComp) const;

	struct FParentSettings
	{
		TObjectPtr<UNiagaraSystem> Template;
		FVector LocationOffset;
		FRotator RotationOffset;
		FVector ScaleOffset;
	};

	// 부모 클래스 설정 백업
	FParentSettings BackupParentSettings() const;

	void RestoreParentSettings(const FParentSettings& Settings);
	void ApplyEffectSettings(const FEffectSettings& Settings);
};
