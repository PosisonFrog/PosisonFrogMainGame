#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CDashComponent.generated.h"

class UCharacterMovementComponent;
class ACharacter;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCDashComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCDashComponent();

    // 대시 시작(이미 대시 중이면 무시)
    void StartDash();

    // 진행중 여부/설정 조회
    bool  IsDashing() const { return bIsDashing; }
    float GetDashDuration() const { return DashDuration; }

    // ─ 튜닝 파라미터 ─
    UPROPERTY(EditAnywhere, Category="Dash", meta=(ClampMin="0"))
    float DashDuration = 0.25f;               // 대시 지속시간

    UPROPERTY(EditAnywhere, Category="Dash", meta=(ClampMin="0"))
    float DashSpeed = 2400.f;                  // 대시 중 목표 속도(수평 cm/s)

    UPROPERTY(EditAnywhere, Category="Dash")
    bool bClearZVelocity = true;               // Z 속도 제거 여부

    UPROPERTY(EditAnywhere, Category="Dash|Control")
    bool bLockMoveInput = true;                // 대시 중 이동입력 무시

    // 물리 파라미터 임시 오버라이드(대시 손맛)
    UPROPERTY(EditAnywhere, Category="Dash|Physics", meta=(ClampMin="0"))
    float Override_GroundFriction = 0.f;

    UPROPERTY(EditAnywhere, Category="Dash|Physics", meta=(ClampMin="0"))
    float Override_BrakingFrictionFactor = 0.f;

    UPROPERTY(EditAnywhere, Category="Dash|Physics", meta=(ClampMin="0"))
    float Override_BrakingDecelWalking = 0.f;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    void BeginDash_Internal();
    void EndDash_Internal();
    void ApplyPhysicsOverrides();
    void RestorePhysicsOverrides();
    void StartCooldown();
    void MoveSpeedUp();

private:
    UPROPERTY() ACharacter* OwnerChar = nullptr;
    UPROPERTY() UCharacterMovementComponent* MoveComp = nullptr;

    // 상태
    bool  bIsDashing = false;
    bool  bIsOnCoolDown = false;
    float DashTimeAcc = 0.f;
    FVector DashDir2D = FVector::ForwardVector;

    //쿨다운
    float DashCooldown = 4.0f;
    float CooldownTimeRemaining = 0.0f;

    //이속 증가
    float MaxSpeedUp = 700.0f;
    float SpeedUpActiveTime = 2.0f;

    // 원복을 위한 스냅샷
    float Saved_GroundFriction = 0.f;
    float Saved_BrakingFrictionFactor = 0.f;
    float Saved_BrakingDecelWalking = 0.f;
    float Saved_DefaultMovementSpeed = 0.f;
    bool  Saved_bOrientRotationToMovement = false;

public:
    //마찰 구현
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash Physics")
    float DashEndFriction = 2.0f; // 적용할 마찰계수

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash Physics")
    float DashEndBrakingDecel = 50.0f; // 제동력

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash Physics")
    bool bApplyStopForceOnDashEnd = true; // 강제감속 여부

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash Physics")
    float StopForceMultiplier = 0.6f; // 속도 감소 비율 (0.3 = 70% 감소)
};