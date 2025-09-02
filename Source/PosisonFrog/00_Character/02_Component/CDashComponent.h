// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CDashComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDashDelegate);

class UAnimMontage;
class UCurveFloat;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCDashComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // 생성자
    UCDashComponent();
    
protected:
    virtual void BeginPlay() override;

public:
    // 오버라이드된 UActorComponent 함수
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    
    // 대시 기능 함수들
    UFUNCTION(BlueprintCallable, Category = "Dash")
    void StartDash();
    
    UFUNCTION()
    void StopDash();
    
    UFUNCTION(BlueprintPure, Category = "Dash")
    bool CanDash() const;
    
    UFUNCTION(BlueprintPure, Category = "Dash")
    bool IsDashing() const { return bIsDashing; }
    
    // 상태 설정 함수들
    UFUNCTION()
    void SetInputDirection(const FVector& InputDirection) { CurrentInputDirection = InputDirection; }
    
    void SetOwnerSprinting(bool bInIsSprinting) { bIsOwnerSprinting = bInIsSprinting; }

    // 대시 이벤트 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Dash")
    FOnDashDelegate OnDashStarted;

    UPROPERTY(BlueprintAssignable, Category = "Dash")
    FOnDashDelegate OnDashEnded;





    // ----- 멤버 변수 -----
    
private:
    
    // 컴포넌트 참조
    UPROPERTY()
    class ACharacter* OwnerCharacter;

    UPROPERTY()
    class UCharacterMovementComponent* MovementComponent;

    // 타이머 핸들
    FTimerHandle DashTimerHandle;
    FTimerHandle CooldownTimerHandle;
    
    // ----- 대시 기본 설정 -----
    
    // 대시 속도 승수
    UPROPERTY(EditAnywhere, Category = "Dash", meta = (AllowPrivateAccess = "true"))
    float DashSpeedMultiplier = 12.0f;

    // 대시 지속 시간
    UPROPERTY(EditAnywhere, Category = "Dash", meta = (AllowPrivateAccess = "true"))
    float DashDuration = 0.2f;

    // 대시 쿨다운 시간
    UPROPERTY(EditAnywhere, Category = "Dash", meta = (AllowPrivateAccess = "true"))
    float DashCooldown = 1.5f;

    // 대시 중 무적 여부
    UPROPERTY(EditAnywhere, Category = "Dash", meta = (AllowPrivateAccess = "true"))
    bool bInvincibleWhileDashing = false;

    // 공중에서 대시 가능 여부
    UPROPERTY(EditAnywhere, Category = "Dash", meta = (AllowPrivateAccess = "true"))
    bool bCanAirDash = true;

    // 최대 대시 횟수
    UPROPERTY(EditAnywhere, Category = "Dash", meta = (AllowPrivateAccess = "true"))
    int32 MaxDashCount = 1;
    
    // 대시 최대 거리
    UPROPERTY(EditAnywhere, Category = "Dash", meta = (AllowPrivateAccess = "true"))
    float DashDistance = 1400.0f;

    // 대시 속도
    UPROPERTY(EditAnywhere, Category = "Dash", meta = (AllowPrivateAccess = "true"))
    float DashSpeed = 4000.0f;
    
    // ----- 공중 대시 관련 설정 -----
    
    // 공중 대시 속도 조절 계수 (1.0보다 작게 설정하면 공중에서 더 느리게 대시)
    UPROPERTY(EditAnywhere, Category = "Dash", meta = (AllowPrivateAccess = "true"))
    float AirDashSpeedModifier = 1.4f;

    // 공중 대시 지속 시간 조절 계수 (1.0보다 작게 설정하면 공중 대시 시간이 짧아짐)
    UPROPERTY(EditAnywhere, Category = "Dash", meta = (AllowPrivateAccess = "true"))
    float AirDashDurationModifier = 0.2f;
    
    // 공중 대시 거리 조정 계수
    UPROPERTY(EditAnywhere, Category = "Dash", meta = (AllowPrivateAccess = "true"))
    float AirDashDistanceModifier = 2.0f;
    
    // ----- 애니메이션 관련 -----
    
    // 대시 애니메이션 몽타주
    UPROPERTY(EditAnywhere, Category = "Dash|Animation", meta = (AllowPrivateAccess = "true"))
    UAnimMontage* DashMontage;

    // 지상 대시 애니메이션 몽타주
    UPROPERTY(EditAnywhere, Category = "Dash|Animation", meta = (AllowPrivateAccess = "true"))
    UAnimMontage* GroundDashMontage;

    // 공중 대시 애니메이션 몽타주
    UPROPERTY(EditAnywhere, Category = "Dash|Animation", meta = (AllowPrivateAccess = "true"))
    UAnimMontage* AirDashMontage;

    // 대시 속도 커브
    UPROPERTY(EditAnywhere, Category = "Dash|Movement", meta = (AllowPrivateAccess = "true"))
    UCurveFloat* DashSpeedCurve;
    
    // ----- 런타임 상태 변수 -----
    
    // 내부 상태 변수
    bool bIsDashing = false;
    bool bInCooldown = false;
    int32 CurrentDashCount = 0;
    
    // 대시 시작 시 공중 상태 저장
    bool bWasInAir = false;
    
    // 대시 진행 시간 관련
    float DashTimeRemaining = 0.0f;
    float CooldownTimeRemaining = 0.0f;
    float DashStartTime;
    float DashElapsedTime;
    
    // 원래 값 저장
    float OriginalMaxSpeed = 0.0f;
    float OriginalGravityScale = 1.0f;
    float InitialDashSpeed;
    
    // 대시 관련 상태
    FVector DashTargetLocation;
    FVector CurrentInputDirection;
    bool bIsOwnerSprinting = false;
};