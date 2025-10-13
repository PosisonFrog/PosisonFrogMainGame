#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/CBaseDashComponent.h"
#include "Components/ActorComponent.h"
#include "CPlayerDashComponent.generated.h"

class ACharacter;
class UCharacterMovementComponent;
class UCurveFloat;

/** 대시 시작/종료 이벤트(이펙트/사운드 트리거 용) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDashEvent);

/**
 * 순수 "이동/물리"만 담당하는 대시 컴포넌트
 * - 쿨다운/버프/UI는 캐릭터(ACPlayerCharacter)가 관리합니다.
 * - Tick 고정 속도 모드 / LaunchCharacter 모드 지원
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class POSISONFROG_API UCPlayerDashComponent : public UCBaseDashComponent
{
    GENERATED_BODY()

public:
    UCPlayerDashComponent();

    /** 대시 시작(외부에서 쿨다운 등 선 체크 후 호출) */
    UFUNCTION(BlueprintCallable, Category = "Dash")
    void StartDash();

    /** 대시 강제 종료(예외 상황용) */
    UFUNCTION(BlueprintCallable, Category = "Dash")
    void CancelDash();

    UFUNCTION(BlueprintPure, Category = "Dash")
    bool IsDashing() const { return bIsDashing; }

    /** 카메라 Yaw 기준 방향 사용(기본: 캐릭터 Forward) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Direction")
    bool bUseCameraYaw = false;

    /** 외부가 명시적으로 방향을 주입하고 싶을 때 */
    UFUNCTION(BlueprintCallable, Category = "Dash|Direction")
    void SetExplicitDirection(const FVector& WorldDir) { ExplicitDir = WorldDir; bUseExplicitDir = true; }

    /** LaunchCharacter 기반 즉시 추진 모드(루트모션/네트 예측 친화) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Mode")
    bool bUseLaunchMode = false;

    /** Tick 고정 속도 모드 파라미터 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash", meta = (EditCondition = "!bUseLaunchMode"))
    float DashSpeed = 1600.f;

    /** LaunchCharacter 모드 파라미터 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Launch", meta = (EditCondition = "bUseLaunchMode"))
    float LaunchStrength = 1400.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Launch", meta = (EditCondition = "bUseLaunchMode"))
    float LaunchUpward = 50.f;

    /** 공통: 대시 지속 시간 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash", meta = (ClampMin = "0"))
    float DashDuration = 0.25f;

    /** 시작 시 Z 속도 제거 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
    bool bClearZVelocity = true;

    /** (선택) 시간 정규화(0~1)에 대한 속도 배율 곡선 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
    TObjectPtr<UCurveFloat> SpeedCurve = nullptr;

    /** 대시 중 물리 오버라이드(직진 손맛) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Physics", meta = (ClampMin = "0"))
    float Override_GroundFriction = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Physics", meta = (ClampMin = "0"))
    float Override_BrakingFrictionFactor = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Physics", meta = (ClampMin = "0"))
    float Override_BrakingDecelWalking = 100.f;

    /** 대시 종료 직후의 빠른 감속감 파라미터 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Physics", meta = (ClampMin = "0"))
    float End_GroundFriction = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Physics", meta = (ClampMin = "0"))
    float End_BrakingDecelWalking = 4096.f;

    /** 종료 시 수평속도 감쇠 적용 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Physics")
    bool bApplyStopForceOnEnd = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Physics", meta = (ClampMin = "0", ClampMax = "1"))
    float StopForceMultiplier = 0.5f;

    /** 이벤트 */
    UPROPERTY(BlueprintAssignable) FOnDashEvent OnDashStarted;
    UPROPERTY(BlueprintAssignable) FOnDashEvent OnDashEnded;

    // 애니메이션
    UPROPERTY(EditAnywhere, Category = "Dash|Anim")
    UAnimMontage* DashPlayerMontage;

    UPROPERTY(EditAnywhere, Category = "Dash|Anim")
    UAnimMontage* DashHammerMontage;
    
protected:
    // UObject
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // UActorComponent
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    void BeginDash_Internal();
    void EndDash_Internal();

    void ApplyPhysicsOverrides();
    void RestorePhysicsOverrides_Immediate();
    void RestorePhysicsOverrides_Delayed();

    FVector ResolveDashDirection() const;

private:
    // 캐시
    TWeakObjectPtr<ACharacter> OwnerChar;
    TWeakObjectPtr<UCharacterMovementComponent> MoveComp;
    
    // 상태
    bool   bIsDashing = false;
    float  DashTimeAcc = 0.f;
    FVector DashDir2D = FVector::ForwardVector;

    // 외부 지정 방향
    bool    bUseExplicitDir = false;
    FVector ExplicitDir = FVector::ZeroVector;

    // 저장해둘 물리값
    float Saved_GroundFriction = 0.f;
    float Saved_BrakingFrictionFactor = 0.f;
    float Saved_BrakingDecelWalking = 0.f;
    bool  Saved_bOrientRotationToMovement = false;

    // 지연 복구 타이머
    FTimerHandle TimerHandle_DelayedRestore;
};
