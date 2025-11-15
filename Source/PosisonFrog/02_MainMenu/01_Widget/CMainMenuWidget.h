#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CMainMenuWidget.generated.h"

struct FStreamableHandle;
// ── 전방 선언 ──
class UImage;
class UWidget;
class UButton;
class UWidgetAnimation;
class USoundBase;
class UNiagaraSystem;
class UOptionsMenuWidget;
class UWorld;

/**
 * 메인 메뉴 UMG 위젯 (Start / Settings / Exit)
 */
UCLASS()
class POSISONFROG_API UCMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // ─────────────────────────────
    // UI 바인딩 (디자이너에 놓인 위젯들)
    // ─────────────────────────────
protected:
    // 메인 루트 패널(선택) : 옵션창 오픈 시 숨김 처리
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
    TObjectPtr<UWidget> MainRootPanel;

    UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "MainMenu")
    TObjectPtr<UButton> MainMenu_StartButton;

    UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "MainMenu")
    TObjectPtr<UImage> MainMenu_StartArrow;

    UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "MainMenu")
    TObjectPtr<UButton> MainMenu_SettingButton;

    UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "MainMenu")
    TObjectPtr<UImage> MainMenu_SettingArrow;

    UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "MainMenu")
    TObjectPtr<UButton> MainMenu_ExitButton;

    UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "MainMenu")
    TObjectPtr<UImage> MainMenu_ExitArrow;

    // ── 버튼 핸들러(Clicked 권장) ──
    UFUNCTION() void OnStartClicked();
    UFUNCTION() void OnSettingClicked();
    UFUNCTION() void OnExitClicked();

    UFUNCTION() void OnAnyButtonPressed();

    UFUNCTION() void OnStartButtonHovered();
    UFUNCTION() void OnStartButtonUnhovered();

    UFUNCTION() void OnSettingButtonHovered();
    UFUNCTION() void OnSettingButtonUnhovered();

    UFUNCTION() void OnExitButtonHovered();
    UFUNCTION() void OnExitButtonUnhovered();

    // ─────────────────────────────
    // 애니메이션 (이름 일치 필요)
    // ─────────────────────────────
protected:
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    UWidgetAnimation* Anim_Focus = nullptr;

    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    UWidgetAnimation* Anim_Hover = nullptr;

    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    UWidgetAnimation* Anim_Click = nullptr;

    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    UWidgetAnimation* Anim_FadeOut = nullptr;

    // ─────────────────────────────
    // SFX / VFX
    // ─────────────────────────────
protected:
    UPROPERTY(EditDefaultsOnly, Category = "UI|SFX")
    USoundBase* SFX_Hover = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "UI|SFX")
    USoundBase* SFX_Click = nullptr;

    UFUNCTION() void PlayUISound(USoundBase* SFX);

    UPROPERTY(EditDefaultsOnly, Category = "UI|VFX")
    UNiagaraSystem* VFX_Click = nullptr;

    UFUNCTION() void SpawnClickVFX(FVector2D ScreenPos);

    // ─────────────────────────────
    // 네비게이션 / 옵션 위젯 (선택)
    // ─────────────────────────────
protected:
    // 시작 레벨 이름을 BP에서 변경 가능
    UPROPERTY(EditDefaultsOnly, Category = "Navigation")
    FName StartLevelName = TEXT("PlayLevel");

    // (선택) 시작 레벨을 SoftObjectPtr로 지정하면 백그라운드 프리로드를 사용할 수 있습니다.
    UPROPERTY(EditDefaultsOnly, Category = "Navigation")
    TSoftObjectPtr<UWorld> StartLevelAsset;
    
    // 컷신 종료까지 레벨 진입을 지연할지 여부 (BP에서 설정)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation", meta = (AllowPrivateAccess = "true"))
    bool bWaitForCutsceneBeforeTravel = false;
    
    //옵션 메뉴(있다면 연결)
    UPROPERTY(EditDefaultsOnly, Category = "Navigation")
    TSubclassOf<UOptionsMenuWidget> OptionsMenuClass;

    UPROPERTY() UOptionsMenuWidget* OptionsMenu = nullptr;

    UFUNCTION() void OnOptionsClosed();
    void SetMainPanelVisible(bool bVisible);

    // Start 버튼 클릭 이후 레벨 로드를 제어
    void BeginPreloadStartLevel();
    void OnStartLevelPreloadCompleted();
    void TryTravelToStartLevel();
    
    // 컷신 종료를 알리기 위한 Blueprint 호출 지점
    UFUNCTION(BlueprintCallable, Category = "Navigation")
    void NotifyCutsceneFinished();
    
    // (선택) Start 버튼 클릭 시 BP에서 컷신을 재생할 수 있도록 하는 이벤트
    UFUNCTION(BlueprintImplementableEvent, Category = "Navigation")
    void BP_OnStartGameRequested();
    
    TSharedPtr<FStreamableHandle> StartLevelStreamHandle;
    bool bLevelPreloaded = false;
    bool bCutsceneFinished = true;
    bool bTravelRequested = false;
    
    // ─────────────────────────────
    // 더블클릭/스팸 방지
    // ─────────────────────────────
protected:
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    float InputLockDuration = 0.35f;

    bool bInputLocked = false;
    FTimerHandle InputUnlockTimer;

    void LockInput();
    void UnlockInput();
};
