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
class UCHitStopComponent;

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
    
    /** 공격 입력 (키/버튼에서 호출) */
    virtual void DoAttack() override;

    // --- 애님 노티파이 훅
    /** (선택) 애님에서 호출하는 상태기 훅 */
    UFUNCTION(BlueprintCallable, Category = "Attack") void BeginAction();
    UFUNCTION(BlueprintCallable, Category = "Attack") void EndAction();

    /** AnimNotifyState_ComboWindow 에서 호출 */
    UFUNCTION(BlueprintCallable, Category = "Attack") void EnableComboInput();   // 창 시작
    UFUNCTION(BlueprintCallable, Category = "Attack") void DisableComboInput();  // 창 종료

    // --- 해머 접근 ---
    ACHammer* GetHammer() const { return Cast<ACHammer>(GetCurrentWeapon()); }
    
private:
    virtual void SpawnWeapon() override;
    virtual void HandleWeaponHit(AActor* InstigatorActor, AActor* HitActor, float Damage, FHitResult Hit) override;

private:
    // --- 콤보 로직 ---
    void PlayComboAttack();
    void StepToNextCombo();      // 다음 콤보 스텝으로 전진(인덱스 증가 + Play)
    void ResetCombo();
    
    UFUNCTION() void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
 
protected:
    // ---- 콤보 설정 ----
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerMontages")
    TArray<UAnimMontage*> PlayerComboMontages;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HammerMontages")
    TArray<UAnimMontage*> HammerComboMontages;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.1"))
    float ComboResetTime = 0.8f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.1"))
    TArray <float> ComboAttackRatio  = {0.9f, 1.1f, 1.4f}; 

    // 히트 스톱
    UPROPERTY(EditAnywhere, Category = "Attack|HitStop", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float ThirdComboHitStopDuration = 0.3f;

    UPROPERTY(EditAnywhere, Category = "Attack|HitStop", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float ThirdComboHitStopTimeScale = 0.01f;

    UPROPERTY(EditAnywhere, Category = "Attack|HitStop")
    bool bEnableHitStop = true;
    
    // --- 궁극기 게이지 ---
    UPROPERTY(EditAnywhere, Category = "Ultimate|State")
    float AddUltGaugeMul = 0.02f;
    
    // ---- 런타임 상태 ----
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Attack")
    int32 CurrentCombo = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Attack")
    bool bIsAttacking = false;

    /** 다음 콤보로 넘어갈 수 있는 창(윈도우) 열림 여부 (AnimNotifyState가 관리) */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Attack")
    bool bCanNextCombo = false;

    /** 창이 닫혀 있을 때 들어온 추가 공격 입력 큐 */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Attack")
    bool bQueuedNextInput = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.1"))
    float AttackMoveSpeedMul = 0.4f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Knockback")
    bool bEnableHitKnockback = true;
   
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Knockback", meta = (ClampMin = "0.0"))
    float HitKnockbackStrength = 650.f;
   
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Knockback", meta = (ClampMin = "0.0"))
    float HitKnockbackUpStrength = 120.f;
    
private:
    UPROPERTY() UCHitStopComponent* HitStopComponent = nullptr;
    bool bHitStopTriggeredThisCombo = false; // 히트스톱 중복을 막기위한 플래그
    
    bool bHasNotifiedAttackEnd = false;
    
    FTimerHandle ComboResetTimer;
};

