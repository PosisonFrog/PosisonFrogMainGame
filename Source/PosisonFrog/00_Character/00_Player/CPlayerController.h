// CPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "01_Item/CHealOrbPoolSubsystem.h"
#include "00_Character/00_Player/01_Widget/COrbHUDWidget.h"
#include "CPlayerController.generated.h"

struct FInputActionValue;
class UInputMappingContext;
class UCInputConfig;
class UCPlayerWidget;
class ACPlayerCharacter;
class UCHealthComponent;
class UCEnhancedInputComponent;

/**
 * 플레이어 컨트롤러
 * - Enhanced Input 매핑 컨텍스트 적용
 * - 입력 바인딩(컨트롤러 → 캐릭터 위임)
 * - HP UI(플레이어 위젯) 생성/갱신
 */
UCLASS()
class POSISONFROG_API ACPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ACPlayerController();

protected:
    /** 커스텀 입력 컴포넌트(UCEnhancedInputComponent) 생성 지점 */
    virtual void CreateInputComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupInputComponent() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

    // ---- 입력 바인딩 내부 구성 ----
    void SetupInputBindings();

    // ---- UI ----
    bool ShouldCreatePlayerWidget() const;
    void CreatePlayerWidget();

    UFUNCTION()
    void OnHealOrbCountersChanged(int32 ActiveOrbs, int32 TotalPicked);

    // ---- 입력 핸들러 (컨트롤러에서 받아 캐릭터 함수로 위임) ----
    UFUNCTION() void HandleMove(const FInputActionValue& Value);
    UFUNCTION() void HandleLook(const FInputActionValue& Value);
    UFUNCTION() void HandleDashStart();
    UFUNCTION() void HandleAttack();

    // ---- HP 갱신 ----
    UFUNCTION() void HandleHealthChanged(float CurrentHealth, float MaxHealth);
    void UpdateHpUI() const;

protected:
    // ===== UI =====
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UCPlayerWidget> PlayerWidgetClass;

    UPROPERTY() // GC 안전
    UCPlayerWidget* PlayerWidget = nullptr;

    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UCOrbHUDWidget> OrbHUDWidgetClass;

    UPROPERTY()
    UCOrbHUDWidget* OrbHUDWidget = nullptr;

    // ===== 입력(매핑/설정) =====
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    UCInputConfig* InputConfig = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    UInputMappingContext* DefaultMappingContext = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    bool bClearPreviousMappings = true;

    /** 실제로 소유하는 입력 컴포넌트(컨트롤러 소유) */
    UPROPERTY(Transient)
    UCEnhancedInputComponent* CEnhancedInputComponent = nullptr;

    // ===== 소유 캐릭터 & 헬스 =====
    UPROPERTY()
    TObjectPtr<ACPlayerCharacter> OwnerCharacter = nullptr;

    UPROPERTY()
    TObjectPtr<UCHealthComponent> HealthComponent = nullptr;
};

