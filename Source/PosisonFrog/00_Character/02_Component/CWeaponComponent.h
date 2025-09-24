#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CWeaponComponent.generated.h"

class ACharacter;
class ACHammer;
class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeaponHit, AActor*, HitActor, float, Damage);

/**
 * 무기/콤보 컴포넌트
 * - DoAttack: 공격 입력 진입점
 * - ComboWindow(AnimNotifyState)로 콤보 입력 창을 제어
 * - 창 외 입력은 큐에 저장 → 창 Begin 시 자동 진행
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class POSISONFROG_API UCWeaponComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCWeaponComponent();

protected:
    virtual void BeginPlay() override;

public:
    /** 공격 입력 (키/버튼에서 호출) */
    UFUNCTION(BlueprintCallable, Category = "Attack")
    void DoAttack();

    /** (선택) 애님에서 호출하는 상태기 훅 */
    UFUNCTION(BlueprintCallable, Category = "Attack") void BeginAction();
    UFUNCTION(BlueprintCallable, Category = "Attack") void EndAction();

    /** AnimNotifyState_ComboWindow 에서 호출 */
    UFUNCTION(BlueprintCallable, Category = "Attack") void EnableComboInput();   // 창 시작
    UFUNCTION(BlueprintCallable, Category = "Attack") void DisableComboInput();  // 창 종료

    /** AnimNotifyState_PlayerAttack 등에서 호출 (히트창) */
    UFUNCTION(BlueprintCallable, Category = "Attack") void EnableAttackBoxCollider();
    UFUNCTION(BlueprintCallable, Category = "Attack") void DisableAttackBoxCollider();

    UPROPERTY(BlueprintAssignable)
    FOnWeaponHit OnWeaponHit;

    UFUNCTION()
    void HandleHammerHit(AActor* InstigatorActor, AActor* HitActor, float Damage, FHitResult Hit);
    
private:
    // ---- 내부 동작 ----
    void SpawnWeapon();
    void AttachWeaponToCharacter();

    void PlayComboAttack();
    void StepToNextCombo();      // 다음 콤보 스텝으로 전진(인덱스 증가 + Play)
    void ResetCombo();

    UFUNCTION() void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

private:
    // ---- 소유자/무기 ----
    UPROPERTY() ACharacter* OwnerCharacter = nullptr;
    UPROPERTY() ACHammer* Hammer = nullptr;

protected:
    // ---- 무기 에셋/부착 ----
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    TSubclassOf<ACHammer> HammerClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    FName AttachSocketName = TEXT("Hand_Hammer");

    // ---- 콤보 설정 ----
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    TArray<UAnimMontage*> ComboMontages;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.1"))
    float ComboResetTime = 1.5f;

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

    FTimerHandle ComboResetTimer;
};

