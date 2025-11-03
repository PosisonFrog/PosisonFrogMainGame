#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/00_PlayerComponent/CSkillComponent.h"
#include "CSkill_SpinAttack.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UDamageType;
class UAnimMontage;
class USoundBase;
class UParticleSystem;
class UCameraShakeBase;

/**
 * 홀드형 회전 공격 스킬
 * - 키 누르는 동안 유지, 떼면 즉시 취소
 * - 틱마다 회전(Yaw) + 근접 범위 데미지
 * - Fury 활성(스냅샷) 시 DPS 배율 적용
 * - Fury 10칸 피니시: ‘망치 내려찍기’ 연출 + 광역 1타
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCSkill_SpinAttack : public UCSkillComponent
{
    GENERATED_BODY()

public:
    UCSkill_SpinAttack();

    void TryStartSpin();
    void StopSpin();

protected:
    virtual void BeginPlay() override;
    
protected:
    // 홀드 시작/종료
    virtual bool DoActivate() override;
    virtual bool DoCancel()  override;

    // Fury 연동
    virtual void OnFuryStarted (int32 TierIdx, float Duration, float TotalDamage, int32 InitialStacks) override;
    virtual void OnFuryEnded   (bool bCanceled, float TimeRemainingAtEnd) override;
    virtual void OnFuryFinisher(float FinisherDamage) override;

private:
    // 주기 실행(회전 + 근접 판정 + 피해)
    void SpinTick();

    // 근접 판정 대상 수집(반경 내 Pawn)
    void CollectTargetsInRadius(TArray<AActor*>& OutTargets, float Radius) const;

    // 대상에게 틱 피해 적용
    void ApplyDamageTo(AActor* Target, float DamageAmount, TSubclassOf<UDamageType> InDamageType) const;

    // 피니시: 망치 내려찍기
    void PlayFinisherMontageAndScheduleImpact(float FinisherDamage);
    void DoFinisherImpact(); // 실제 피해 및 이펙트

private:
    // ───────── 스핀 파라미터 ─────────
    UPROPERTY(EditDefaultsOnly, Category="Spin|Anim")
    UAnimMontage* CharSpinMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category="Spin|Anim")
    UAnimMontage* HammerSpinMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Spin|Anim")
    UNiagaraSystem* SpinVFX = nullptr;
    
    UPROPERTY(EditDefaultsOnly, Category="Spin|Timing", meta=(ClampMin="0.02", ClampMax="0.2"))
    float TickInterval = 0.05f;

    UPROPERTY(EditDefaultsOnly, Category="Spin|Damage", meta=(ClampMin="0"))
    float BaseDPS = 140.f;

    UPROPERTY(EditDefaultsOnly, Category="Spin|Damage", meta=(ClampMin="1.0"))
    float FuryDPSMultiplier = 1.2f; // Fury "버프"만 반영 (스택 증가 X)

    UPROPERTY(EditDefaultsOnly, Category="Spin|Range",  meta=(ClampMin="50"))
    float AttackRadius = 250.f;

    UPROPERTY(EditDefaultsOnly, Category="Spin|Range",  meta=(ClampMin="0"))
    float ZTolerance = 120.f;

    UPROPERTY(EditDefaultsOnly, Category="Spin|Control")
    bool bAutoRotateOwner = true;

    UPROPERTY(EditDefaultsOnly, Category="Spin|Control", meta=(ClampMin="0"))
    float SpinYawSpeedDegPerSec = 720.f;

    // 스택 금지: 기본값을 '일반 DamageType'으로 설정
    UPROPERTY(EditDefaultsOnly, Category="Spin|Damage")
    TSubclassOf<UDamageType> DamageTypeClass;

    // ───────── 피니시(망치 내려찍기) ─────────
    // Finisher (선택, 스택 금지)
    UPROPERTY(EditDefaultsOnly, Category="Finisher|Anim")
    UAnimMontage* CharFinisherMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category="Finisher|Anim")
    UAnimMontage* HammerFinisherMontage = nullptr;
    
    /** 내려찍기 낙하→충격 타이밍(초). 몽타주를 사용하지 않으면 이 시간 뒤에 피해 발생 */
    UPROPERTY(EditDefaultsOnly, Category="Finisher|Anim", meta=(ClampMin="0"))
    float FinisherImpactDelay = 0.35f;

    UPROPERTY(EditDefaultsOnly, Category="Finisher|Damage", meta=(ClampMin="0"))
    float FinisherDamageDefault = 250.f;

    UPROPERTY(EditDefaultsOnly, Category="Finisher|Range", meta=(ClampMin="50"))
    float FinisherRadius = 320.f;

    // 스택 금지 : 피니시 데미지도 일반 DamageType
    UPROPERTY(EditDefaultsOnly, Category="Finisher|Damage")
    TSubclassOf<UDamageType> FinisherDamageTypeClass;

    // 연출(선택 항목)
    UPROPERTY(EditDefaultsOnly, Category="Finisher|FX")
    UParticleSystem* FinisherImpactFX = nullptr;

    UPROPERTY(EditDefaultsOnly, Category="Finisher|FX")
    USoundBase* FinisherImpactSFX = nullptr;

    UPROPERTY(EditDefaultsOnly, Category="Finisher|FX")
    TSubclassOf<UCameraShakeBase> FinisherCameraShake;

private:
    UPROPERTY() UNiagaraComponent* ActiveSpinVFXComponent = nullptr;
    
    TWeakObjectPtr<ACharacter> OwnerChar;
    
    FTimerHandle TimerHandle_SpinTick;
    float        LastTickTime = 0.f;
    bool         bFuryActiveSnapshot = false;
    
    // 피니시 임시 저장
    float        PendingFinisherDamage = 0.f;

    int32 StacksAtActivation = 0;
};

