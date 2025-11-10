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

	/** 패턴 실행 */
	virtual void ExecutePattern(int32 PhaseIndex);

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

protected:
	/** 몽타주 재생 */
	void PlayMontage(UAnimMontage* Montage);

	/** 플레이어 타겟 가져오기 */
	AActor* GetPlayerTarget() const;

	/** AI 컨트롤러 가져오기 */
	AAIController* GetBossAI() const;

	/** 오너 보스 */
	UPROPERTY()
	TWeakObjectPtr<ACEnemyBossCharacter> OwnerBoss;

	/** 무기 컴포넌트 */
	UPROPERTY()
	TWeakObjectPtr<UCEnemyWeaponComponent> WeaponComponent;

	/** 패턴 ID */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern")
	FName PatternId;

	/** 현재 페이즈 인덱스 */
	int32 CurrentPhaseIndex;

	/** 패턴의 쿨다운 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern")
	float CooldownDuration = 5.0f;

	/** 이 패턴이 마지막으로 사용된 게임 시간 */
	float LastUsedTime = -1.0f;
	
};