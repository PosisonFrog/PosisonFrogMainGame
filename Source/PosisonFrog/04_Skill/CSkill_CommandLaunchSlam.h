// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CSkill_CommandLaunchSlam.generated.h"


class ACHammer;
class UDamageType;
class UParticleSystem;
class USoundBase;
class UCameraShakeBase;
class UCharacterMovementComponent;
class UAnimMontage;
class UAnimInstance;
class ACEnemyCharacterBase;

// 자기 자신의 중복 입력을 막귀 위해서 Launching을 추가
UENUM(BlueprintType)
enum class ECommandAirState : uint8
{
    Inactive,
    Launching,
    AirborneWaiting,
    Descending
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAirCommandLockChanged, bool, bLocked);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCSkill_CommandLaunchSlam : public UActorComponent
{
    GENERATED_BODY()

public:
    UCSkill_CommandLaunchSlam();

    // ─ 입력에서 호출 ─
    UFUNCTION(BlueprintCallable, Category="CommandSkill")
    bool TryStartCommand();            // Ctrl+LMB 첫 입력 (지상)
    UFUNCTION(BlueprintCallable, Category="CommandSkill")
    bool TryConfirmSlam();             // 공중 1초 내 재입력

    // ─ 애님 노티에서 호출 ─
    UFUNCTION(BlueprintCallable, Category="CommandSkill|Anim")
    void Anim_PerformLaunch();         // Launch 노티 지점에서 실제 띄우기
    UFUNCTION(BlueprintCallable, Category="CommandSkill|Anim")
    void Anim_SlamImpact();            // Slam 몽타주 임팩트 프레임

    UFUNCTION(BlueprintPure,   Category="CommandSkill")
    bool IsAirCommandActive() const { return State != ECommandAirState::Inactive; }
    UFUNCTION(BlueprintPure,   Category="CommandSkill")
    bool ShouldBlockOtherActions() const { return bBlockOtherActionsWhileAir && IsAirCommandActive(); }

    UPROPERTY(BlueprintAssignable, Category="CommandSkill")
    FOnAirCommandLockChanged OnAirCommandLockChanged;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick, FActorComponentTickFunction*) override;

private:
    // 내부 로직
    void EnterAirborneWaiting();
    void ForceDescend(bool bAsSlam);
    void OnAirWindowExpired();
    void DoShockwaveImpact();
    void CollectCharactersInRadius(TArray<ACharacter*>& OutChars, float Radius, bool bIncludeLaunchedIgnoringZ = false) const;
    bool IsOnGroundNow() const;
    bool IsLaunchableEnemy(ACharacter* C) const;
    void ForceDropEnemiesInRange() const;
    void CleanupLaunchedEnemies();
    bool IsTrackedLaunchedEnemy(const ACharacter* C) const;

    void StartSlamConfirmDelay();
    void ClearSlamWaiting();
    
    // 애님 유틸
    UAnimInstance* GetPlayerAnimInstance() const;
    UAnimInstance* GetHammerAnimInstance() const;
    // 플레이어 전용
    void PlayCharMontageSafe(UAnimMontage* Montage, FName Section = NAME_None, float PlayRate = 1.f);
    // 해머 전용
    void PlayHammerMontageSafe(UAnimMontage* Montage, FName Section = NAME_None, float PlayRate = 1.0f);
    
private:
    // ─ Launch/Range ─
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Launch")
    float PlayerLaunchZ = 900.f;
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Launch")
    float EnemyLaunchZ  = 800.f;
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Launch", meta=(ClampMin="0"))
    float EnemyForceDropSpeed = 3200.f;
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Launch", meta=(ClampMin="0"))
    float LaunchDamage = 180.f;

    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Range", meta=(ClampMin="100"))
    float LaunchRadius = 450.f;
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Range", meta=(ClampMin="50"))
    float ZTolerance   = 150.f;

    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Timing", meta=(ClampMin="0.1", ClampMax="3.0"))
    float AirCommandWindow = 1.0f;

    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Timing", meta=(ClampMin="0.0", ClampMax="3.0"))
    float SlamConfirmDelay = 0.7f;
    
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Slam", meta=(ClampMin="200"))
    float SlamDownSpeed   = 2200.f;
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Slam", meta=(ClampMin="200"))
    float AutoDescendSpeed= 1600.f;
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Shockwave", meta=(ClampMin="100"))
    float ShockwaveRadius = 520.f;
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Shockwave", meta=(ClampMin="0"))
    float ShockwaveDamage = 180.f;
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|DamageType")
    TSubclassOf<UDamageType> LaunchDamageType;
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|DamageType")
    TSubclassOf<UDamageType> ShockwaveDamageType;

    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Lock")
    bool bBlockOtherActionsWhileAir = true;

    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Detect", meta=(ClampMin="10", ClampMax="150"))
    float GroundTouchThreshold = 60.f;

    // FX
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|FX")
    UParticleSystem*           ShockwaveFX = nullptr;
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|FX")
    USoundBase*                ShockwaveSFX = nullptr;
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|FX")
    TSubclassOf<UCameraShakeBase> ShockwaveCameraShake;

    // ─ Launch 대상 필터(태그/폴백) ─
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Filter|Tags")
    TArray<FName> LaunchAllowTags;
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Filter|Tags")
    TArray<FName> LaunchDenyTags;
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Filter|ClassFallback")
    TArray<TSoftClassPtr<ACharacter>> LaunchAllowClassFallbacks;
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Filter|ClassFallback")
    TArray<TSoftClassPtr<ACharacter>> LaunchDenyClassFallbacks;

    // ─ 애님 연동 ─
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Anim")
    bool bLaunchByNotify = true;         // true면 Launch는 노티에서 실행
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Anim")
    bool bImpactByNotify = true;         // true면 임팩트는 노티에서 실행(착지 감지 보조는 유지)

    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Anim")
    UAnimMontage* LaunchCharMontage = nullptr;
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Anim")
    UAnimMontage* LaunchHammerMontage = nullptr;
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Anim")
    FName LaunchSection = NAME_None;

    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Anim")
    UAnimMontage* SlamCharMontage = nullptr;
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Anim")
    UAnimMontage* SlamHammerMontage = nullptr;
    UPROPERTY(EditDefaultsOnly, Category="CommandSkill|Anim")
    FName SlamSection = NAME_None;

private:
    // 런타임
    ECommandAirState State = ECommandAirState::Inactive;
    bool  bPendingSlam = false;
    bool  bImpactDone  = false;

    bool  bAwaitingSlamConfirm = false;
    float EarliestSlamConfirmTime = 0.f;
    
    TWeakObjectPtr<ACharacter> OwnerChar;
    TWeakObjectPtr<ACHammer> Hammer;
    TWeakObjectPtr<UCharacterMovementComponent> MoveComp;
    FTimerHandle TimerHandle_AirWindow;
    TArray<TWeakObjectPtr<ACharacter>> LaunchedEnemies;
};