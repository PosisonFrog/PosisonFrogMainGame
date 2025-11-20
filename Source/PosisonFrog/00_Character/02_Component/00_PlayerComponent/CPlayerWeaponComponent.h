#pragma once

#include "CoreMinimal.h"
#include "00_Character/00_Player/02_Weapon/CHammer.h"
#include "00_Character/02_Component/CBaseWeaponComponent.h"
#include "Components/ActorComponent.h"
#include "CPlayerWeaponComponent.generated.h"

class UCUltimateBuffComponent;
class ACharacter;
class ACHammer;
class UAnimMontage;

// 플레이어 콤보 공격 히트 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayerComboHit, AActor*, HitActor, int32, ComboIndex, float, Damage);


/**
 * 무기/콤보 컴포넌트
 * - DoAttack: 공격 입력 진입점
 * - ComboWindow(AnimNotifyState)로 콤보 입력 창을 제어
 * - 창 외 입력은 큐에 저장 → 창 Begin 시 자동 진행
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class POSISONFROG_API UCPlayerWeaponComponent : public UCBaseWeaponComponent
{
    GENERATED_BODY()

public:
    UCPlayerWeaponComponent();
    
    // ───────── 공격 인터페이스 ─────────
    // 공격 입력 (키/버튼에서 호출)
    virtual void DoAttack() override;

    // ───────── 애니메이션 노티파이 훅 ─────────
    // (선택) 애님에서 호출하는 상태기 훅
    UFUNCTION(BlueprintCallable, Category = "Attack") void BeginAction();
    UFUNCTION(BlueprintCallable, Category = "Attack") void EndAction();
    // AnimNotifyState_ComboWindow 에서 호출
    UFUNCTION(BlueprintCallable, Category = "Attack") void EnableComboInput();
    UFUNCTION(BlueprintCallable, Category = "Attack") void DisableComboInput();

    // ───────── 해머 접근 ─────────
    // 매번 Cast하지 않도록 캐싱된 포인터 반환
    ACHammer* GetHammer() const { return CurrentHammer; }

    // ───────── 이벤트 ─────────
    // 플레이어 콤보 공격이 적에게 적중했을 때 브로드캐스트
    UPROPERTY(BlueprintAssignable, Category = "Attack|Events")
    FOnPlayerComboHit OnPlayerComboHit;
    
private:
    virtual void SpawnWeapon() override;
    virtual void HandleWeaponHit(AActor* InstigatorActor, AActor* HitActor, float Damage, FHitResult Hit) override;
    
    // ───────── 콤보 로직 ─────────
    void PlayComboAttack();
    void StepToNextCombo();      // 다음 콤보 스텝으로 전진(인덱스 증가 + Play)
    void ResetCombo();
    
    UFUNCTION() void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    // 타격 이펙트 관련 헬퍼 함수들
    void SpawnHitEffect(AActor* HitActor, const FHitResult& HitInfo);
    bool CheckUltimateActive() const;
    FTransform CalculateEffectTransform(const FHitResult& HitInfo, const FVector& LocationOffset, const FRotator& RotationOffset) const;

    // ───────── 히트스톱 헬퍼 함수 ─────────
    // 콤보별 히트스톱 파라미터 가져오기
    bool GetComboHitStopParams(int32 ComboIndex, float& OutPlayerDuration, float& OutPlayerTimeScale, float& OutEnemyDuration, float& OutEnemyTimeScale) const;

    // 히트스톱 적용 (애니메이션 정지 + 서브시스템 호출)
    void ApplyComboHitStop(AActor* HitActor, float PlayerDuration, float PlayerTimeScale, float EnemyDuration, float EnemyTimeScale);
    
    // 애니메이션 일시정지 및 재개 예약
    void PauseAndScheduleResumeAnimation(UAnimInstance* AnimInst, float ResumeDelay);
    
protected:
    // ───────── 콤보 설정 ─────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerMontages")
    TArray<UAnimMontage*> PlayerComboMontages;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HammerMontages")
    TArray<UAnimMontage*> HammerComboMontages;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.1"))
    float ComboResetTime = 0.8f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.1"))
    TArray<float> ComboAttackRatio = {0.9f, 1.1f, 1.4f}; 

    // ───────── 히트 스톱 설정 ─────────
    // 1타 히트스톱
    UPROPERTY(EditAnywhere, Category = "Attack|HitStop|Combo1")
    bool bEnableFirstComboHitStop = true;

    UPROPERTY(EditAnywhere, Category = "Attack|HitStop|Combo1|Player", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float FirstComboPlayerHitStopDuration = 0.08f;

    UPROPERTY(EditAnywhere, Category = "Attack|HitStop|Combo1|Player", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float FirstComboPlayerHitStopTimeScale = 0.2f;

    UPROPERTY(EditAnywhere, Category = "Attack|HitStop|Combo1|Enemy", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float FirstComboEnemyHitStopDuration = 0.12f;

    UPROPERTY(EditAnywhere, Category = "Attack|HitStop|Combo1|Enemy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float FirstComboEnemyHitStopTimeScale = 0.1f;

    // 2타 히트스톱
    UPROPERTY(EditAnywhere, Category = "Attack|HitStop|Combo2")
    bool bEnableSecondComboHitStop = true;

    UPROPERTY(EditAnywhere, Category = "Attack|HitStop|Combo2|Player", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float SecondComboPlayerHitStopDuration = 0.1f;

    UPROPERTY(EditAnywhere, Category = "Attack|HitStop|Combo2|Player", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SecondComboPlayerHitStopTimeScale = 0.15f;

    UPROPERTY(EditAnywhere, Category = "Attack|HitStop|Combo2|Enemy", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float SecondComboEnemyHitStopDuration = 0.15f;

    UPROPERTY(EditAnywhere, Category = "Attack|HitStop|Combo2|Enemy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SecondComboEnemyHitStopTimeScale = 0.05f;

    // 3타 히트스톱
    UPROPERTY(EditAnywhere, Category = "Attack|HitStop|Combo3")
    bool bEnableThirdComboHitStop = true;

    UPROPERTY(EditAnywhere, Category = "Attack|HitStop|Combo3|Player", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float ThirdComboPlayerHitStopDuration = 0.15f;

    UPROPERTY(EditAnywhere, Category = "Attack|HitStop|Combo3|Player", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ThirdComboPlayerHitStopTimeScale = 0.1f;

    UPROPERTY(EditAnywhere, Category = "Attack|HitStop|Combo3|Enemy", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float ThirdComboEnemyHitStopDuration = 0.3f;

    UPROPERTY(EditAnywhere, Category = "Attack|HitStop|Combo3|Enemy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ThirdComboEnemyHitStopTimeScale = 0.01f;
    
    // ───────── 궁극기 게이지 ─────────
    UPROPERTY(EditAnywhere, Category = "Ultimate|State")
    float AddUltGaugeMul = 0.02f;
    
    // ───────── 런타임 상태 ─────────
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Attack")
    int32 CurrentCombo = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Attack")
    bool bIsAttacking = false;

    // 다음 콤보로 넘어갈 수 있는 창(윈도우) 열림 여부 (AnimNotifyState가 관리)
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Attack")
    bool bCanNextCombo = false;

    // 창이 닫혀 있을 때 들어온 추가 공격 입력 큐
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Attack")
    bool bQueuedNextInput = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.1"))
    float AttackMoveSpeedMul = 0.4f;

    // ───────── 넉백 설정 ─────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Knockback")
    bool bEnableHitKnockback = true;

    // 각 콤보별 수평 넉백 강도 (1타, 2타, 3타)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Knockback", meta = (ClampMin = "0.0"))
    TArray<float> HitKnockbackStrengths = { 500.0f, 650.0f, 800.0f };

    // 각 콤보별 수직 넉백 강도 (1타, 2타, 3타)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Knockback", meta = (ClampMin = "0.0"))
    TArray<float> HitKnockbackUpStrengths = { 100.0f, 120.0f, 150.0f };
    
private:
    // 매번 Cast 연산을 피하기 위한 캐싱된 해머 포인터
    UPROPERTY() ACHammer* CurrentHammer = nullptr;
    
    bool bHitStopTriggeredThisCombo = false; // 히트스톱 중복을 막기위한 플래그
    bool bHasNotifiedAttackEnd = false;
    
    FTimerHandle ComboResetTimer;
};
