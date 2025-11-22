#pragma once
#include "CoreMinimal.h"
#include "00_Character/01_Enemy/CEnemyCharacterBase.h"
#include "CRangedSkirmisher.generated.h"

class UAnimMontage;
class UNiagaraSystem;
class UCapsuleComponent;
class UCharacterMovementComponent;
class CEnemyBullet;
struct FTimerHandle;

UCLASS()
class POSISONFROG_API ACRangedSkirmisher : public ACEnemyCharacterBase
{
    GENERATED_BODY()
public:
    ACRangedSkirmisher();

protected:
    // AActor
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    
    virtual void OnDead() override;
    virtual void OnResetForRespawn_Implementation() override;
    // FSM 확장
    virtual void DoChase() override;     // 거리 밴드 유지 + 전술 이동
    virtual void DoAttack() override;    // 사격 조건/쿨다운/회피
    virtual void ExitState(EEnemyState OldState) override;


    // ───── 사격 ─────
    void StartBurst();
    void FireBurstShot();
    void FireOnce();
    FVector ComputeLeadAimDir(const FVector& From, const FVector& TargetPos, const FVector& TargetVel, float ProjSpeed) const;
    FVector GetMuzzleLocation(FRotator& OutMuzzleRot) const;

    // ───── 회피(이베이드) ─────
    void TryEvadeRandom();              // 확률적 회피
    void DoEvade(bool bPreferLeft);     // 실제 실행
    UFUNCTION() void OnAnyDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
                                  AController* InstigatedBy, AActor* DamageCauser);

    // ───── 유틸 ─────
    bool HasClearShot() const;

protected:
    // ───── 전투/사격 설정 ─────

    UPROPERTY(EditAnywhere, Category="PF|Ranged|Combat", meta=(ClampMin="0.0"))
    float BurstShotInterval = 0.25f;     // 버스트 내 연속 발사 간격
    
    UPROPERTY(EditAnywhere, Category="PF|Ranged|Combat", meta=(ClampMin="0.0"))
    float BurstCooldown = 3.0f;          // 버스트 종료 후 쿨다운
    
    UPROPERTY(EditAnywhere, Category="PF|Ranged|Combat", meta=(ClampMin="1", ClampMax="6"))
    int32 ShotsPerBurst = 2;           // 연사 딜레이

    UPROPERTY(EditAnywhere, Category="PF|Ranged|Combat")
    float DesiredRangeMin = 597.f;

    UPROPERTY(EditAnywhere, Category="PF|Ranged|Combat")
    float DesiredRangeMax = 897.f;

    UPROPERTY(EditAnywhere, Category="PF|Ranged|Combat")
    float ProjectileSpeed = 1100.f;      // 리드샷 계산 및 탄환 초기속도

    UPROPERTY(EditAnywhere, Category="PF|Ranged|Combat")
    TSubclassOf<AActor> ProjectileClass; // CEnemyBullet

    UPROPERTY(EditAnywhere, Category="PF|Ranged|Combat")
    FName MuzzleSocket = TEXT("Muzzle");  // 스켈레탈 소켓명

    UPROPERTY(EditAnywhere, Category="PF|Ranged|Combat")
    FVector MuzzleOffset = FVector(30.f, 0.f, 80.f); // 소켓 없을 때 사용

    UPROPERTY(EditAnywhere, Category="PF|Ranged|Combat")
    float BulletDamage = 12.f;           // 한 발 데미지(탄환에 전달)

    UPROPERTY(EditAnywhere, Category="PF|Ranged|Combat")
    float SpreadDegrees = 1.5f;          // 소량의 탄퍼짐

    // ───── 이동/전술 ─────
    UPROPERTY(EditAnywhere, Category="PF|Ranged|Move")
    float RunSpeed = 600.f;

    // ───── 회피(이베이드) ─────
    UPROPERTY(EditAnywhere, Category="PF|Ranged|Evade")
    bool bCanEvade = true;

    UPROPERTY(EditAnywhere, Category="PF|Ranged|Evade")
    float EvadeCooldown = 3.0f;

    UPROPERTY(EditAnywhere, Category="PF|Ranged|Evade")
    float EvadeImpulse = 900.f; // LaunchCharacter 측면 임펄스

    UPROPERTY(EditAnywhere, Category="PF|Ranged|Evade")
    float EvadeChanceOnThink = 0.04f;    // 주기적(DoAttack 틱) 확률

    UPROPERTY(EditAnywhere, Category="PF|Ranged|Evade")
    float EvadeTriggerDistance = 900.f;  // 가까울수록 회피 적극

    /*추가된 부분 -> 최대 회피 거리(TriggerDistance)에서 적용될 확률 배율 (0~1 범위) */
    UPROPERTY(EditAnywhere, Category="PF|Ranged|Evade", meta=(ClampMin="0.0", ClampMax="1.0"))
    float EvadeChanceAtMaxDistanceScale = 0.2f;
    
    float LastEvadeTime = -1000.f;

    // ───── 연출 ─────
    UPROPERTY(EditAnywhere, Category="PF|Ranged|VFX")
    UNiagaraSystem* MuzzleFX = nullptr;
    

    UPROPERTY(EditAnywhere, Category="PF|Ranged|Anim")
    UAnimMontage* FireMontage = nullptr;

    UPROPERTY(EditAnywhere, Category="PF|Animation")
    UAnimMontage* DeadMontage = nullptr;
    
    // 내부
    FTimerHandle BurstTimerHandle;
    int32 ShotsFiredInBurst = 0;
    float LastBurstTime = -1000.f;
    FTransform InitialMeshRelativeTransform = FTransform::Identity;
    
    // 디버그
    UPROPERTY(EditAnywhere, Category="PF|Debug")
    bool bDebugLog = false;
};