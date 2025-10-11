#pragma once

#include "CoreMinimal.h"
#include "00_Character/CBaseCharacter.h"
#include "00_Character/02_Component/00_PlayerComponent/Buffable.h"
#include "CPlayerCharacter.generated.h"

class UCSkill_CommandLaunchSlam;
class UCSkill_SpinAttack;
class UCFuryGaugeComponent;
class UCUltimateBuffComponent;
class UCameraComponent;
class USpringArmComponent;
class UTransparentCameraComponent;

class UCPlayerDashComponent;
class UCPlayerWeaponComponent;
class UCPlayerHealthComponent;
class UCPlayerMovementBuffComponent;

class UCInputConfig;
class UCPlayerWidget;
struct FInputActionValue;


/**
 * 플레이어 캐릭터:
 * - 입력 버퍼/락으로 Attack1/2 → Dash 캔슬 지원
 * - Dash 실행은 단일 경로(TryCommitDash)로만 처리 → 쿨다운/UI/버프 일관성 보장
 * - 쿨다운/UI/버프는 캐릭터가 관리, 이동/물리는 UCDashComponent가 담당
 */
UCLASS(config = Game)
class POSISONFROG_API ACPlayerCharacter : public ACBaseCharacter, public IBuffable
{
    GENERATED_BODY()

public:
    ACPlayerCharacter();

    // 카메라 Getter
    FORCEINLINE USpringArmComponent* GetCameraBoom()   const { return SpringArm; }
    FORCEINLINE UCameraComponent* GetFollowCamera() const { return PlayerCamera; }

protected:
    // ACharacter
    virtual void BeginPlay() override;
    virtual void PostInitializeComponents() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public: // 입력 핸들러 (Enhanced Input 바인딩 대상)
    UFUNCTION() void Move(const FInputActionValue& Value);
    UFUNCTION() void Look(const FInputActionValue& Value);
    UFUNCTION() void DashStart();   // 입력 진입점(버퍼 or 즉시)
    UFUNCTION() void Attack();
    UFUNCTION() void OnSpinPressed();      // 스핀 시작(홀드)
    UFUNCTION() void OnSpinReleased();     // 스핀 종료
    UFUNCTION() void OnCommandPressed();
    
    // 무기/애님에서 호출 (공격 시작/종료/대시소비 윈도우)
    UFUNCTION() void OnAttackStarted();
    UFUNCTION() void OnAttackEnded();
    UFUNCTION() void OnAttackDashReady();

private: // Dash 실행 단일 경로 + 버퍼 소비
    UFUNCTION() bool TryCommitDash();                         // 쿨다운/UI/버프까지 한 곳에서 처리
    UFUNCTION() void ConsumeDashBufferIfValid(bool bFallback = false);

private: // HP/UI 연동
    UFUNCTION() void HandleHealthChanged(float CurrentHealth, float MaxHealth);
    void UpdateHpUI() const;

private: // Dash 쿨다운/UI 틱
    UFUNCTION() void ResetDashCooldown();
    UFUNCTION() void TickDashCooldownUI();

private: // 궁극기 UI/버프 적용
    void UpdateUltimateUI();
    UFUNCTION() void UseUltimate();

public:
    // IBuffable 인터페이스 구현
    virtual float GetOutgoingDamageMultiplier() const override;
    virtual float GetIncomingDamageScale() const override;
    virtual bool IsBuffActive() const override;
    
    void AddUltimateGain(float Gain);
    float GetMaxUltimateGauge() const { return MaxUltGauge; }
    
private:
    // ─ 입력 설정(태그→액션, Enhanced Input용 DataAsset) ─
    UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<const UCInputConfig> InputConfig = nullptr;

    // ─ 이동 튜닝 ─
    UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0", ForceUnits = "cm/s"))
    float WalkingSpeed = 400.f;

    // ─ UI ─
    UPROPERTY(EditDefaultsOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UCPlayerWidget> PlayerWidgetClass;

    UPROPERTY(Transient, meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCPlayerWidget> PlayerWidget = nullptr;

    // ─ Dash 버퍼/락 정책 ─
    UPROPERTY(EditAnywhere, Category = "Dash|Buffer", meta = (ClampMin = "0.0"))
    float DashBufferWindow = 0.20f;     // PDF: InputBuffer = 0.20s

    UPROPERTY(VisibleInstanceOnly, Category = "Dash|Buffer")
    bool bDashLocked = false;           // 공격 중 락

    UPROPERTY(VisibleInstanceOnly, Category = "Dash|Buffer")
    bool bDashBuffered = false;         // 버퍼 보유 여부

    UPROPERTY(VisibleInstanceOnly, Category = "Dash|Buffer")
    float DashBufferExpire = 0.f;       // 버퍼 만료 시각

    // ─ Dash 쿨다운 ─
    UPROPERTY(EditDefaultsOnly, Category = "Dash", meta = (ClampMin = "0"))
    float DashCooldown = 6.0f;

    UPROPERTY(VisibleInstanceOnly, Category = "Dash")
    bool bDashOnCooldown = false;

    UPROPERTY(VisibleInstanceOnly, Category = "Dash")
    float DashCooldownRemaining = 0.f;

    FTimerHandle TimerHandle_DashCooldown;   // 쿨다운 만료
    FTimerHandle TimerHandle_DashUITick;     // UI 업데이트(20Hz 권장)

    // ─ Dash 후 이속 버프 ─
    UPROPERTY(EditDefaultsOnly, Category = "Dash|Buff", meta = (ClampMin = "1.0"))
    float DashSpeedMultiplier = 1.30f;       // +30%

    UPROPERTY(EditDefaultsOnly, Category = "Dash|Buff", meta = (ClampMin = "0"))
    float DashSpeedBuffDuration = 2.0f;      // 2초

    // ─ Ultimate  ─
    UPROPERTY(EditDefaultsOnly, Category = "Ultimate|State")
    float MaxUltGauge = 100.0f;
    
    UPROPERTY(EditDefaultsOnly, Category = "Ultimate|State")
    float CurUltGauge = 0.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Ultimate|State")
    bool bUltActive = false;

    // 게이지 소모 관련
    UPROPERTY(EditAnywhere, Category = "Ultimate|State")
    float UltDrainPerSec = 20.0f; // 5초면 소모 100 / 5 = 20

    UPROPERTY(EditAnywhere, Category = "Ultimate|State")
    float UltDrainTickInterval = 0.05f;

    FTimerHandle TimerHandle_UltDrain;

    UFUNCTION()
    void OnUltimateDrainTimer();
    
    void StartUltimateDrain();
    void StopUltimateDrain();
    
    // ─ 구성 컴포넌트 ─
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCPlayerDashComponent> DashComponent = nullptr;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCPlayerWeaponComponent> WeaponComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCPlayerHealthComponent> HealthComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCPlayerMovementBuffComponent> MovementBuffComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCUltimateBuffComponent> UltimateBuffComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fury", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCFuryGaugeComponent> FuryGaugeComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Skill", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCSkill_SpinAttack> SpinAttackComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Skill", meta=(AllowPrivateAccess = "true"))
    TObjectPtr<UCSkill_CommandLaunchSlam> CommandLaunchSlamComponent = nullptr;
    
    // ─ 카메라 ─
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USpringArmComponent> SpringArm = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> PlayerCamera = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UTransparentCameraComponent> TransparentCameraComponent = nullptr;
};
