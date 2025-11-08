// CPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CPlayerController.generated.h"


class UInputMappingContext;
class UInputAction;
class UCInputConfig;
class UCPlayerWidget;
class ACPlayerCharacter;
class UCPauseSubsystem;
enum class EGamePauseState : uint8;

/**
 * C++ 중심 컨트롤러
 * - Enhanced Input Mapping Context를 C++에서 적용
 * - 일시정지/메뉴 토글과 입력 모드 전환(GameOnly / UIOnly) C++ 처리
 * - (선택) 캐릭터가 비어 있으면 InputConfig 자동 주입
 */
UCLASS()
class POSISONFROG_API ACPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ACPlayerController();

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ────────────── 입력 핸들러 ──────────────
    UFUNCTION() void HandlePausePressed();
    UFUNCTION() void HandleToggleMouse();

    UFUNCTION() void HandlePauseStateChanged(EGamePauseState NewState);

    // ────────────── 메뉴/입력 모드 ──────────────
    void ShowPauseMenu();
    void HidePauseMenu();
    void SetInputMode_GameOnly();
    void SetInputMode_UIOnly();

private:
    // ────────────── Enhanced Input ──────────────
    /** C++에서 적용할 기본 IMC (에디터 자산을 지정하세요) */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext = nullptr;

    /** 패드 전용 IMC (선택) */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> GamepadMappingContext = nullptr;
    
    /** IMC 적용 우선순위 (0: 기본) */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    int32 MappingPriority = 0;

    /** 패드 IMC 적용 우선순위 (기본 컨텍스트보다 우선순위를 높게 설정하는 것이 좋습니다) */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    int32 GamepadMappingPriority = 1;
    
    /** 일시정지 액션 (IMC에 매핑되어 있어야 함). 없으면 ESC 키 폴백 */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_Pause = nullptr;

    /** 마우스 토글 액션(선택). 없으면 기본 키 없음 */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_ToggleMouse = nullptr;

    /** (선택) 캐릭터에 주입할 입력 구성 자산 */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<const UCInputConfig> DefaultInputConfig = nullptr;

    // ────────────── 메뉴 위젯 ──────────────
    /** 일시정지 메뉴 위젯 클래스 (WBP 또는 C++ UUserWidget) */
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UUserWidget> PauseMenuClass;

    /** 생성된 메뉴 인스턴스 */
    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> PauseMenuInstance = nullptr;

    /** 현재 일시정지 여부 캐시 */
    UPROPERTY(VisibleInstanceOnly, Category = "State")
    bool bIsPausedMenuOpen = false;

    UPROPERTY(Transient)
    TObjectPtr<UCPauseSubsystem> CachedPauseSubsystem = nullptr;
};
