

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
struct FInputActionValue;

UCLASS(config = Game)
class POSISONFROG_API ACPlayerCharacter : public ACBaseCharacter
{
    GENERATED_BODY()

public:
    ACPlayerCharacter();

    /** Returns CameraBoom subobject */
    FORCEINLINE USpringArmComponent* GetCameraBoom() const { return SpringArm; }
    /** Returns FollowCamera subobject */
    FORCEINLINE UCameraComponent* GetFollowCamera() const { return PlayerCamera; }

protected:
    // 입력 바인딩
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // 생명주기
    virtual void BeginPlay() override;

public:
    // 입력 핸들러 (Enhanced Input에서 바인딩)
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void DashStart();
    void Attack();

    /** 입력 설정(프로젝트 전용 UCInputConfig) */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    UCInputConfig* InputConfig = nullptr;

    /** 이동 기본 속도(튜닝값) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
    float WalkingSpeed = 400.0f;

    // === UI ===
protected:
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UCPlayerWidget> PlayerWidgetClass;

    UPROPERTY() // GC 안전
        UCPlayerWidget* PlayerWidget = nullptr;

    UFUNCTION()
    void HandleHealthChanged(float CurrentHealth, float MaxHealth);

    void UpdateHpUI() const;

    // === 컴포넌트 ===
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    UCDashComponent* DashComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    UCWeaponComponent* WeaponComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    UCHealthComponent* HealthComponent = nullptr;

    // === 카메라 ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    USpringArmComponent* SpringArm = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    UCameraComponent* PlayerCamera = nullptr;
};

