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
    
protected:
    // ACharacter
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    virtual void BeginPlay() override;
    virtual void PostInitializeComponents () override;
    
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

