#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CPlayerKnockbackComponent.generated.h"

class UAnimMontage;
class ACharacter;
class APlayerController;
class ACHammer;
class UCPlayerWeaponComponent;

/**
 * 플레이어 넉백 전담 컴포넌트
 * - 3단계 넉백 시퀀스 관리: 공중 → 기절 → 일어서기
 * - 시간 기반 전환 (땅 감지 없음)
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCPlayerKnockbackComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCPlayerKnockbackComponent();

    UFUNCTION(BlueprintCallable, Category = "PF|Knockback")
    void StartKnockback(AActor* Attacker = nullptr); 

    UFUNCTION(BlueprintCallable, Category = "PF|Knockback")
    void CancelKnockback();

    UFUNCTION(BlueprintPure, Category = "PF|Knockback")
    bool IsKnockedBack() const { return bIsKnockedBack; }

    UFUNCTION(BlueprintPure, Category = "PF|Knockback")
    bool IsStunned() const { return bIsStunned; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void PlayAirAnimation();
    void TransitionToDown();
    void OnStunEnd();
    void OnGetUpComplete();

    // 유틸리티
    void BlockInput();
    void UnblockInput();
    void StopMovement();
    void PlayMontage(UAnimMontage* Montage);
    void FaceAttacker(AActor* Attacker);
    void ClearTimers();
    
    ACHammer* GetHammer() const;

private:
    // ─────────── 애니메이션 ───────────
    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Animation")
    TObjectPtr<UAnimMontage> CharacterAirMontage = nullptr;       

    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Animation")
    TObjectPtr<UAnimMontage> CharacterDownMontage = nullptr;       

    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Animation")
    TObjectPtr<UAnimMontage> CharacterGetUpMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Animation")
    TObjectPtr<UAnimMontage> HammerAirMontage = nullptr;       

    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Animation")
    TObjectPtr<UAnimMontage> HammerDownMontage = nullptr;       

    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Animation")
    TObjectPtr<UAnimMontage> HammerGetUpMontage = nullptr;


    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Animation")
    bool bLoopAirAnimation = true;                        
    
    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Animation", meta = (ClampMin = "0.1", ClampMax = "2.0"))
    float AirAnimationPlayRate = 1.0f;                  

    // ─────────── 타이밍 ───────────
    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Timing", meta = (ClampMin = "0.1"))
    float AirDuration = 0.7f;                           

    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Timing", meta = (ClampMin = "0"))
    float StunDuration = 1.0f;                            

    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Settings")
    bool bBlockInputDuringKnockback = true;              

    // ─────────── 디버그 ───────────
    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Debug")
    bool bDebugLog = false;

    // ─────────── 런타임 상태 ───────────
    UPROPERTY(Transient)
    bool bIsKnockedBack = false;

    UPROPERTY(Transient)
    bool bIsStunned = false;

    // 캐시된 참조
    TWeakObjectPtr<ACharacter> OwnerCharacter;
    TWeakObjectPtr<APlayerController> CachedPC;
    TWeakObjectPtr<UCPlayerWeaponComponent> CachedWeaponComponent;

    // 타이머
    FTimerHandle TH_TransitionToDown;
    FTimerHandle TH_StunEnd;
    FTimerHandle TH_GetUpComplete;
};