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

    // ─────────── Getter ───────────
    // ─ 카메라
    FORCEINLINE USpringArmComponent* GetCameraBoom() const { return SpringArm; }
    FORCEINLINE UCameraComponent* GetFollowCamera() const  { return PlayerCamera; }
    
    // ─ 궁극기
    float GetMaxUltimateGauge() const { return MaxUltGauge; }

    // ─────────── IBuffable ───────────
    // ─ 인터페이스 구현
    virtual float GetOutgoingDamageMultiplier() const override;
    virtual float GetIncomingDamageScale() const override;
    virtual bool IsBuffActive() const override;
    
protected:
    virtual void BeginPlay() override;
    virtual void PostInitializeComponents() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    
public:
    // ─────────── Input Handlers ───────────
    // ─ Enhanced Input 바인딩 대상
    UFUNCTION() void Move(const FInputActionValue& Value);
    UFUNCTION() void Look(const FInputActionValue& Value);
    UFUNCTION() void Attack();
    UFUNCTION() void DashStart();          // 입력 진입점(버퍼 or 즉시)
    UFUNCTION() void UseUltimate();
    UFUNCTION() void OnSpinPressed();      // 스핀 시작(홀드)
    UFUNCTION() void OnSpinReleased();     // 스핀 종료
    UFUNCTION() void OnCommandPressed();
    
    // 무기/애님에서 호출 (공격 시작/종료/대시소비 윈도우)
    UFUNCTION() void OnAttackStarted();
    UFUNCTION() void OnAttackEnded();
    UFUNCTION() void OnAttackDashReady();
    UFUNCTION() void HandleCommandMovementLockChanged(bool bLocked);

private:
    // ─────────── Dash ───────────
    // ─ Dash 실행 단일 경로 + 버퍼 소비
    UFUNCTION() bool TryCommitDash();                         // 쿨다운/UI/버프까지 한 곳에서 처리
    UFUNCTION() void ConsumeDashBufferIfValid(bool bFallback = false);

    // ─ Dash 쿨다운/UI 틱
    UFUNCTION() void ResetDashCooldown();
    UFUNCTION() void TickDashCooldownUI();

private:
    // ─────────── HP ───────────
    UFUNCTION() void HandleHealthChanged(float CurrentHealth, float MaxHealth);
    UFUNCTION() void HandleDeath();
    void UpdateHpUI() const;

    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
    
private:
    // ─────────── ULT ───────────
    void UpdateUltimateUI();
    UFUNCTION() void OnUltimateExpired(); // 궁극기 종료시 호출될 함수
    
    UFUNCTION() void TickUltimateUI(); // 궁극기 UI 수정
public:
    void AddUltimateGain(float Gain);
    
private:
    // ─────────── Input ───────────
    // ─ 입력 설정(태그 -> 액션, Enhanced Input용 DataAsset)
    UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<const UCInputConfig> InputConfig = nullptr;

    // ─────────── Move ───────────
    // ─ 이동 튜닝
    UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0", ForceUnits = "cm/s"))
    float WalkingSpeed = 400.f;

    // ─────────── UI ───────────
    UPROPERTY(EditDefaultsOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UCPlayerWidget> PlayerWidgetClass;

    UPROPERTY(Transient, meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCPlayerWidget> PlayerWidget = nullptr;

    // ─────────── Dash ───────────
    // ─ Dash 버퍼/락 정책
    UPROPERTY(EditAnywhere, Category = "Dash|Buffer", meta = (ClampMin = "0.0"))
    float DashBufferWindow = 0.20f;     // PDF: InputBuffer = 0.20s

    UPROPERTY(VisibleInstanceOnly, Category = "Dash|Buffer")
    bool bDashLocked = false;           // 공격 중 락

    UPROPERTY(VisibleInstanceOnly, Category = "Dash|Buffer")
    bool bDashBuffered = false;         // 버퍼 보유 여부

    UPROPERTY(VisibleInstanceOnly, Category = "Dash|Buffer")
    float DashBufferExpire = 0.f;       // 버퍼 만료 시각
    
    UPROPERTY(VisibleInstanceOnly, Category = "Movement|Lock")
    bool bCommandMovementLocked = false;

    UPROPERTY(VisibleInstanceOnly, Category = "Movement|Attack")
    bool bAttackMovementOverrideActive = false;
    
    void ApplyAttackMovementOverride(bool bEnable);
    
    // ─ Dash 쿨다운
    UPROPERTY(EditDefaultsOnly, Category = "Dash", meta = (ClampMin = "0"))
    float DashCooldown = 6.0f;

    UPROPERTY(VisibleInstanceOnly, Category = "Dash")
    bool bDashOnCooldown = false;

    UPROPERTY(VisibleInstanceOnly, Category = "Dash")
    float DashCooldownRemaining = 0.0f;

    FTimerHandle TimerHandle_DashCooldown;   // 쿨다운 만료
    FTimerHandle TimerHandle_DashUITick;     // UI 업데이트(20Hz 권장)

    // ─ Dash 후 이속 버프
    UPROPERTY(EditDefaultsOnly, Category = "Dash|Buff", meta = (ClampMin = "1.0"))
    float DashSpeedMultiplier = 1.30f;       // +30%

    UPROPERTY(EditDefaultsOnly, Category = "Dash|Buff", meta = (ClampMin = "0"))
    float DashSpeedBuffDuration = 2.0f;      // 2초

    // ─────────── HP ───────────
    UPROPERTY(EditDefaultsOnly, Category = "Death|Anim")
    UAnimMontage* DeathPlayerMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Death|Anim")
    UAnimMontage* DeathHammerMontage = nullptr;
    
    UPROPERTY(VisibleInstanceOnly, Category = "State")
    bool bIsDead = false;
    
    // ─────────── ULT ───────────
    UPROPERTY(EditDefaultsOnly, Category = "Ultimate|State")
    float MaxUltGauge = 100.0f;
    
    UPROPERTY(EditDefaultsOnly, Category = "Ultimate|State")
    float CurUltGauge = 0.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Ultimate|State")
    bool bUltActive = false;
    
    UPROPERTY(EditDefaultsOnly, Category = "Ultimate|State")
    float UltDuration = 5.0f; // 궁극기 전체 지속 시간 (초)
    
    float UltDrainTickInterval = 0.05f;

    FTimerHandle TimerHandle_UltDuration;
    FTimerHandle TimerHandle_UltUITick;

    // ─────────── 구성 컴포넌트 ───────────
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
    
    // ─────────── 카메라 ───────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USpringArmComponent> SpringArm = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> PlayerCamera = nullptr;

    //UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera", meta = (AllowPrivateAccess = "true"))
    //TObjectPtr<UTransparentCameraComponent> TransparentCameraComponent = nullptr;
};
