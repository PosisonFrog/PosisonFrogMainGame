#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CPlayerKnockbackComponent.generated.h"

class UAnimMontage;
class ACharacter;
class APlayerController;

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

    // 넉백 시작 (외부에서 호출)
    UFUNCTION(BlueprintCallable, Category = "PF|Knockback")
    void StartKnockback(AActor* Attacker = nullptr); 

    // 강제 중단 (사망 시 등)
    UFUNCTION(BlueprintCallable, Category = "PF|Knockback")
    void CancelKnockback();

    // 상태 쿼리
    UFUNCTION(BlueprintPure, Category = "PF|Knockback")
    bool IsKnockedBack() const { return bIsKnockedBack; }

    UFUNCTION(BlueprintPure, Category = "PF|Knockback")
    bool IsStunned() const { return bIsStunned; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // 시퀀스 단계들
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

private:
    // ─────────── 애니메이션 ───────────
    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Animation")
    TObjectPtr<UAnimMontage> AirMontage = nullptr;        // 공중 넉백 애님

    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Animation")
    TObjectPtr<UAnimMontage> DownMontage = nullptr;       // 땅에 쓰러진 기절 애님

    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Animation")
    TObjectPtr<UAnimMontage> GetUpMontage = nullptr;      // 일어서는 애님

    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Animation")
    bool bLoopAirAnimation = true;                        // 공중 애님 루프 여부
    
    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Animation", meta = (ClampMin = "0.1", ClampMax = "2.0"))
    float AirAnimationPlayRate = 1.0f;                    // 공중 애님 재생 속도

    // ─────────── 타이밍 ───────────
    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Timing", meta = (ClampMin = "0.1"))
    float AirDuration = 0.7f;                             // 공중 애님 지속 시간

    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Timing", meta = (ClampMin = "0"))
    float StunDuration = 1.0f;                            // 기절 지속 시간

    UPROPERTY(EditDefaultsOnly, Category = "Knockback|Settings")
    bool bBlockInputDuringKnockback = true;               // 넉백 중 입력 차단

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

    // 타이머
    FTimerHandle TH_TransitionToDown;
    FTimerHandle TH_StunEnd;
    FTimerHandle TH_GetUpComplete;
};