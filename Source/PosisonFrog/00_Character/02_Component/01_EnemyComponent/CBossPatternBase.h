// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CBossPatternBase.generated.h"

class ACEnemyBossCharacter;
class UCEnemyWeaponComponent;
class AAIController;
class UAnimMontage;


/**
 * 보스 패턴 베이스 클래스
 * 모든 보스 패턴은 이 클래스를 상속받아 구현됩니다.
 */
UCLASS(Abstract, Blueprintable)
class POSISONFROG_API UCBossPatternBase : public UObject
{
	GENERATED_BODY()

public:
	UCBossPatternBase();

	/** 패턴 초기화 (패턴 실행 전 한번 호출) */
	virtual void Initialize(ACEnemyBossCharacter* InOwnerBoss, UCEnemyWeaponComponent* InWeaponComponent);
	
	virtual void ExecutePattern(int32 PhaseIndex, const struct FBossPatternDefinition& PatternData);

	/** 패턴 종료 처리 */
	virtual void OnPatternEnd();

	/** 패턴 클린업 (타이머 정리 등) */
	virtual void Cleanup();

	/** 패턴 ID 반환 */
	virtual FName GetPatternId() const { return PatternId; }

	/** 페이즈별 설정 업데이트 */
	virtual void UpdatePhaseSettings(int32 PhaseIndex);

	/** 패턴의 쿨다운이 아직 남았는지 확인합니다. */
	virtual bool IsOnCooldown() const;

	/** 패턴의 쿨다운 타이머를 시작합니다. (현재 시간을 기록) */
	virtual void StartCooldown();

	/** 런타임 쿨다운 값을 반환합니다. */
	float GetRuntimeCooldown() const { return RuntimeCooldown; }

protected:
	
	void FinishPattern(bool bApplyCooldown = true);
	float PlayMontage(UAnimMontage* Montage);

	/** 플레이어 타겟 가져오기 */
	AActor* GetPlayerTarget() const;

	/** AI 컨트롤러 가져오기 */
	AAIController* GetBossAI() const;

	UPROPERTY()
	TWeakObjectPtr<ACEnemyBossCharacter> OwnerBoss;

	UPROPERTY()
	TWeakObjectPtr<UCEnemyWeaponComponent> WeaponComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Pattern")
	FName PatternId;
	
	int32 CurrentPhaseIndex;
	float RuntimeCooldown = 5.0f;
	float LastUsedTime = -9999.f;
};