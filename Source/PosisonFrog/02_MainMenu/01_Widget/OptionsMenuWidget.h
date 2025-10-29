// Source/PosisonFrogMainGame/Public/02_MainMenu/01_Widget/OptionsMenuWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OptionsMenuWidget.generated.h"

// ── 전방 선언 ──
class UButton;
class UComboBoxString;
class USlider;
class UCheckBox;
class UTextBlock;
class USoundMix;
class USoundClass;
class UInputMappingContext;
class UInputAction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOptionsClosed);

/**
 * 그래픽(해상도/창 모드/VSync/밝기), 오디오(마스터/BGM/SFX), 입력(마우스 감도/키 리바인드) 옵션 위젯
 */
UCLASS()
class POSISONFROG_API UOptionsMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    /** 메인 메뉴로 “닫힘” 알림 */
    UPROPERTY(BlueprintAssignable)
    FOnOptionsClosed OnClosed;

    /** 옵션 오픈 시 초기 포커스 */
    UFUNCTION()
    void FocusInitial();

protected:
    // ===================== UMG Bind =====================
    // 그래픽
    UPROPERTY(meta = (BindWidget), BlueprintReadOnly) UComboBoxString* Combo_Resolution = nullptr;
    UPROPERTY(meta = (BindWidget), BlueprintReadOnly) UComboBoxString* Combo_WindowMode = nullptr;
    UPROPERTY(meta = (BindWidget), BlueprintReadOnly) UCheckBox* Check_VSync = nullptr;

    // 오디오
    UPROPERTY(meta = (BindWidget), BlueprintReadOnly) USlider* Slider_Master = nullptr;
    UPROPERTY(meta = (BindWidget), BlueprintReadOnly) USlider* Slider_BGM = nullptr;
    UPROPERTY(meta = (BindWidget), BlueprintReadOnly) USlider* Slider_SFX = nullptr;

    // 공용 버튼
    UPROPERTY(meta = (BindWidget), BlueprintReadOnly) UButton* Btn_Apply = nullptr;
    UPROPERTY(meta = (BindWidget), BlueprintReadOnly) UButton* Btn_Default = nullptr;
    UPROPERTY(meta = (BindWidget), BlueprintReadOnly) UButton* Btn_Back = nullptr;

    // 추가: 밝기/마우스 감도
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly) USlider* Slider_Brightness = nullptr;  // 0~1 → 0.5~3.0 감마
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly) USlider* Slider_MouseSens = nullptr;  // 0~1 → 0.1~2.0 배

    // 추가: 키 리바인드(라벨/버튼)
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly) UTextBlock* Label_AttackKey = nullptr;
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly) UTextBlock* Label_DashKey = nullptr;
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly) UButton* Btn_RebindAttack = nullptr;
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly) UButton* Btn_RebindDash = nullptr;

    // ===================== 오디오 리소스 =====================
    UPROPERTY(EditDefaultsOnly, Category = "Audio") USoundMix* OptionsSoundMix = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Audio") USoundClass* MasterClass = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Audio") USoundClass* BGMClass = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Audio") USoundClass* SFXClass = nullptr;

    // ===================== 입력(Enhanced Input) =====================
    /** 에디터에서 지정할 기본 매핑 컨텍스트(복제하여 런타임 편집) */
    UPROPERTY(EditDefaultsOnly, Category = "Input|Rebind") UInputMappingContext* BaseMappingContext = nullptr;
    /** 선택: 게임패드 매핑 컨텍스트(복제하여 런타임 편집) */
    UPROPERTY(EditDefaultsOnly, Category = "Input|Rebind") UInputMappingContext* BaseGamepadMappingContext = nullptr;
    /** 런타임 복제본 (여기에서 Unmap/MapKey 수행) */
    UPROPERTY() UInputMappingContext* RuntimeMapping = nullptr;
    UPROPERTY() UInputMappingContext* RuntimeGamepadMapping = nullptr;
    
       /** 런타임 추가 시 사용할 우선순위 */
    UPROPERTY(EditDefaultsOnly, Category = "Input|Rebind") int32 BaseMappingPriority = 0;
    UPROPERTY(EditDefaultsOnly, Category = "Input|Rebind") int32 GamepadMappingPriority = 1;

    /** 리바인드 대상 액션(예시: 공격/대시) */
    UPROPERTY(EditDefaultsOnly, Category = "Input|Rebind") UInputAction* Action_Attack = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Input|Rebind") UInputAction* Action_Dash = nullptr;

    // ===================== 내부 상태 =====================
    TArray<FIntPoint> SupportedRes;
    float MasterVol = 1.f, BgmVol = 1.f, SfxVol = 1.f;
    float BrightnessGamma = 2.2f;      // 0.5~3.0 권장
    float MouseSensitivity = 1.0f;     // 0.1~2.0 권장
    bool  bWaitingForRebind = false;
    UInputAction* PendingAction = nullptr;
    bool bKeyboardContextAdded = false;
    bool bGamepadContextAdded = false;

    // ===================== 초기화/저장/적용 =====================
    void BuildResolutionList();
    void LoadFromConfig();
    void SaveToConfig();
    void ApplyVolumes(bool bPushMix = true);
    void SyncUIFromSettings();
    void ApplyGraphicsFromUI(bool bSave);
    void EnsureRuntimeMapping();
    void UpdateKeyLabels();

    // ===================== 콜백 =====================
    // 그래픽
    UFUNCTION() void OnResolutionChanged(FString Item, ESelectInfo::Type Type);
    UFUNCTION() void OnWindowModeChanged(FString Item, ESelectInfo::Type Type);
    UFUNCTION() void OnVSyncChanged(bool bChecked);

    // 오디오
    UFUNCTION() void OnMasterVolChanged(float V);
    UFUNCTION() void OnBGMVolChanged(float V);
    UFUNCTION() void OnSFXVolChanged(float V);

    // 추가: 밝기/감도
    UFUNCTION() void OnBrightnessChanged(float V);
    UFUNCTION() void OnMouseSensChanged(float V);

    // 버튼
    UFUNCTION() void OnApplyClicked();
    UFUNCTION() void OnDefaultClicked();
    UFUNCTION() void OnBackClicked();

    // 리바인드
    UFUNCTION() void OnRebindAttack();
    UFUNCTION() void OnRebindDash();
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    void ApplyRebind(UInputAction* Action, const FKey& NewKey);

    // ===================== 유틸 =====================
    FString ToResString(const FIntPoint& P) const;
    EWindowMode::Type ToWindowMode(const FString& Item) const;
    FString FromWindowMode(EWindowMode::Type M) const;
    FString KeyToText(const FKey& Key) const;
};

