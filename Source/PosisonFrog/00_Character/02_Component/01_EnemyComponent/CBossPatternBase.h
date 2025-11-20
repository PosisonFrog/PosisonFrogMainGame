#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CBossPatternBase.generated.h"

class ACEnemyBossCharacter;
class UCEnemyWeaponComponent;
class UCEnemyBossPhaseComponent;
class AAIController;
class UAnimMontage;

/**
 * 보스 패턴 베이스 클래스
 * 모든 보스 패턴은 이 클래스를 상속받아 구현됩니다.
 * ActorComponent로 변경되어 Tick 및 BeginPlay/EndPlay를 지원합니다.
 */
UCLASS(Abstract, Blueprintable, ClassGroup=(Boss), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCBossPatternBase : public UActorComponent
{
	GENERATED_BODY()

public:
	UCBossPatternBase();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	/** 패턴 실행 */
	virtual bool ExecutePattern(int32 PhaseIndex, const struct FBossPatternDefinition& PatternData);

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

	/** 패턴의 쿨다운 타이머를 시작합니다. */
	virtual void StartCooldown();

	/** 런타임 쿨다운 값을 반환합니다. */
	float GetRuntimeCooldown() const { return RuntimeCooldown; }

protected:
	/** 패턴 종료를 PhaseComponent에 직접 알림 */
	void FinishPattern(bool bApplyCooldown = true);
	
	/** 몽타주 재생 헬퍼 */
	float PlayMontage(UAnimMontage* Montage);

	/** 플레이어 타겟 가져오기 */
	AActor* GetPlayerTarget() const;

	/** AI 컨트롤러 가져오기 */
	AAIController* GetBossAI() const;

	/** DataAsset에서 현재 패턴 데이터 가져오기 */
	const FBossPatternDefinition* GetMyPatternData() const;

	/** Owner Boss 참조 */
	UPROPERTY()
	TWeakObjectPtr<ACEnemyBossCharacter> OwnerBoss;

	/** Weapon Component 참조 */
	UPROPERTY()
	TWeakObjectPtr<UCEnemyWeaponComponent> WeaponComponent;

	/** Phase Component 참조 - DataAsset 직접 접근용 */
	UPROPERTY()
	TWeakObjectPtr<UCEnemyBossPhaseComponent> PhaseComponent;

	/** 패턴 식별자 */
	UPROPERTY(EditDefaultsOnly, Category = "Pattern")
	FName PatternId;
	
	/** 현재 페이즈 인덱스 */
	int32 CurrentPhaseIndex;
	
	/** 런타임 쿨다운 값 (DataAsset에서 로드) */
	float RuntimeCooldown = 5.0f;
	
	/** 마지막 사용 시간 (더 이상 사용 안 함 - PhaseComponent 맵 사용) */
	float LastUsedTime = -9999.f;
};