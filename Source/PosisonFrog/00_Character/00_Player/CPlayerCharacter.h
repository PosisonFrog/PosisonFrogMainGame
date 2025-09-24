#pragma once

#include "CoreMinimal.h"
#include "00_Character/CBaseCharacter.h"
#include "CPlayerCharacter.generated.h"

class UCWeaponComponent;
class USpringArmComponent;
class UCameraComponent;
class UCDashComponent;
class UCHealthComponent;
class UCInputConfig;
class UCPlayerWidget;
class UCMovementBuffComponent;
struct FInputActionValue;

UCLASS(config = Game)
class POSISONFROG_API ACPlayerCharacter : public ACBaseCharacter
{
    GENERATED_BODY()

public:
    ACPlayerCharacter();

    FORCEINLINE USpringArmComponent* GetCameraBoom()   const { return SpringArm; }
    FORCEINLINE UCameraComponent* GetFollowCamera() const { return PlayerCamera; }

    /** 애님 노티: 공격 종료 직전(마지막 몇 프레임)에서 호출 */
    void OnAttackDashReady();

    /** (선택) 무기 컴포넌트가 공격 시작/종료 시 호출하면 더 견고 */
    void OnAttackStarted();
    void OnAttackEnded();


    
protected:
    // ACharacter
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    virtual void BeginPlay() override;
    virtual void PostInitializeComponents () override;

    /** 대시 요청 처리(즉시 실행 또는 버퍼링) */
    void RequestDash();
    
public:
    // ─ Input ─
    UFUNCTION() void Move(const FInputActionValue& Value);
    UFUNCTION() void Look(const FInputActionValue& Value);

    // ─ Skill ─
    UFUNCTION() void DashStart();    // ← 항상 쿨타임 부여
    
    UFUNCTION() void Attack();
    
protected:
    UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCInputConfig> InputConfig = nullptr;

    // ─ Movement Tunables ─
    UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0", ForceUnits = "cm/s"))
    float WalkingSpeed = 400.f;

    // ─ UI ─
    UPROPERTY(EditDefaultsOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UCPlayerWidget> PlayerWidgetClass;

    UPROPERTY(Transient, meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCPlayerWidget> PlayerWidget = nullptr;

    UFUNCTION() void HandleHealthChanged(float CurrentHealth, float MaxHealth);
    void UpdateHpUI() const;

    // ─ Dash Cooldown ─
    UPROPERTY(EditDefaultsOnly, Category = "Dash", meta = (ClampMin = "0"))
    float DashCooldown = 6.0f;               // 기본 6초

    UPROPERTY(VisibleInstanceOnly, Category = "Dash")
    bool bDashOnCooldown = false;

    UPROPERTY(VisibleInstanceOnly, Category = "Dash")
    float DashCooldownRemaining = 0.f;

    FTimerHandle TimerHandle_DashCooldown;   // 만료 타이머
    FTimerHandle TimerHandle_DashUITick;     // UI 20Hz

    UFUNCTION() void ResetDashCooldown();
    UFUNCTION() void TickDashCooldownUI();

    // ─ Dash 후 이속 버프 ─
    UPROPERTY(EditDefaultsOnly, Category = "Dash", meta = (ClampMin = "1.0"))
    float DashSpeedMultiplier = 1.3f;       // +30%

    UPROPERTY(EditDefaultsOnly, Category = "Dash", meta = (ClampMin = "0"))
    float DashSpeedBuffDuration = 2.0f;      // 2초


    // ───────── 대시 버퍼/락 파라미터 ─────────
    /** 공격 중 대시 입력을 버퍼해 유지할 수 있는 시간(초) */
    UPROPERTY(EditAnywhere, Category="Dash|Buffer", meta=(ClampMin="0.0"))
    float DashBufferWindow = 0.25f;

    /** 공격 중에는 대시를 실행하지 않도록 잠그는 플래그 */
    UPROPERTY(VisibleInstanceOnly, Category="Dash|Buffer")
    bool bDashLocked = false;

    /** 공격 중 입력된 대시가 버퍼에 저장되었는지 */
    UPROPERTY(VisibleInstanceOnly, Category="Dash|Buffer")
    bool bDashBuffered = false;

    /** 버퍼 만료 시각(월드 초) */
    UPROPERTY(VisibleInstanceOnly, Category="Dash|Buffer")
    float DashBufferExpire = 0.f;

    /** (정책 전환용) 대시 캔슬 허용 여부 – 본 요구사항에서는 false 유지 */
    UPROPERTY(EditAnywhere, Category="Dash|Policy")
    bool bAllowDashCancel = false;

    /** (정책 전환용) 캔슬 시 블렌드아웃 시간 */
    UPROPERTY(EditAnywhere, Category="Dash|Policy", meta=(ClampMin="0.0"))
    float DashCancelBlendOut = 0.05f;
    

    // ─ 궁극기 (버프) ─
    // 임의로 스택 +1은 100으로 설정
    UPROPERTY(EditDefaultsOnly, Category = "Ultimate")
    float UltimateCurrentPoints = 0.0f;
    
    UPROPERTY(EditDefaultsOnly, Category = "Ultimate")
    float UltimateMaxPoints = 100.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Ultimate")
    int32 UltimateStack = 0;

    UPROPERTY(EditDefaultsOnly, Category = "Ultimate")
    int32 UltimateMaxStacks = 3;
    
    UFUNCTION(BlueprintCallable)
    void AddUltimatePoint(AActor* HitActor, float Damage);

    void CalculateUltimatePoint(float AttackDamage);
    
    // ─ Components ─
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCDashComponent> DashComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCWeaponComponent> WeaponComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCHealthComponent> HealthComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCMovementBuffComponent> MovementBuffComponent = nullptr;

    // Camera
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USpringArmComponent> SpringArm = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> PlayerCamera = nullptr;
};

