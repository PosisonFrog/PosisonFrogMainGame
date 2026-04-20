// Source/PosisonFrogMainGame/Private/02_MainMenu/01_Widget/OptionsMenuWidget.cpp

#include "02_MainMenu/01_Widget/OptionsMenuWidget.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "00_Character/00_Player/CPlayerController.h"


// Enhanced Input
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Components/ProgressBar.h"

void UOptionsMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // ── 이벤트 바인딩 ──
    if (Combo_Resolution)    Combo_Resolution->OnSelectionChanged.AddDynamic(this, &UOptionsMenuWidget::OnResolutionChanged);
    if (Btn_ResolutionLeft)  Btn_ResolutionLeft->OnClicked.AddDynamic(this, &UOptionsMenuWidget::OnResolutionLeftClicked);
    if (Btn_ResolutionRight) Btn_ResolutionRight->OnClicked.AddDynamic(this, &UOptionsMenuWidget::OnResolutionRightClicked);
    
    if (Combo_WindowMode)    Combo_WindowMode->OnSelectionChanged.AddDynamic(this, &UOptionsMenuWidget::OnWindowModeChanged);
    if (Btn_WindowModeLeft)  Btn_WindowModeLeft->OnClicked.AddDynamic(this, &UOptionsMenuWidget::OnWindowModeLeftClicked);
    if (Btn_WindowModeRight) Btn_WindowModeRight->OnClicked.AddDynamic(this, &UOptionsMenuWidget::OnWindowModeRightClicked);
    
    if (Check_VSync)         Check_VSync->OnCheckStateChanged.AddDynamic(this, &UOptionsMenuWidget::OnVSyncChanged);

    if (Slider_Master)       Slider_Master->OnValueChanged.AddDynamic(this, &UOptionsMenuWidget::OnMasterVolChanged);
    if (Slider_BGM)          Slider_BGM->OnValueChanged.AddDynamic(this, &UOptionsMenuWidget::OnBGMVolChanged);
    if (Slider_SFX)          Slider_SFX->OnValueChanged.AddDynamic(this, &UOptionsMenuWidget::OnSFXVolChanged);

    if (Btn_Apply)           Btn_Apply->OnClicked.AddDynamic(this, &UOptionsMenuWidget::OnApplyClicked);
    if (Btn_Default)         Btn_Default->OnClicked.AddDynamic(this, &UOptionsMenuWidget::OnDefaultClicked);
    if (Btn_Back)            Btn_Back->OnClicked.AddDynamic(this, &UOptionsMenuWidget::OnBackClicked);

    if (Slider_Brightness)   Slider_Brightness->OnValueChanged.AddDynamic(this, &UOptionsMenuWidget::OnBrightnessChanged);
    if (Slider_MouseSens)    Slider_MouseSens->OnValueChanged.AddDynamic(this, &UOptionsMenuWidget::OnMouseSensChanged);

    if (Btn_RebindAttack)    Btn_RebindAttack->OnClicked.AddDynamic(this, &UOptionsMenuWidget::OnRebindAttack);
    if (Btn_RebindDash)      Btn_RebindDash->OnClicked.AddDynamic(this, &UOptionsMenuWidget::OnRebindDash);

    // ── 해상도 목록 구성 + 설정 로드/적용 ──
    BuildResolutionList();
    LoadFromConfig();
    ApplyVolumes(true);
    SyncUIFromSettings();

    // 슬라이더 초기화(옵션)
    if (Slider_Brightness)
    {
        Slider_Brightness->SetValue(
            FMath::GetMappedRangeValueClamped(FVector2D(0.5f, 3.0f), FVector2D(0.f, 1.f), BrightnessGamma));
    }
    if (Slider_MouseSens)
    {
        Slider_MouseSens->SetValue(
            FMath::GetMappedRangeValueClamped(FVector2D(0.1f, 2.0f), FVector2D(0.f, 1.f), MouseSensitivity));
    }

    // 리바인드 준비 + 라벨 갱신
    EnsureRuntimeMapping();
    UpdateKeyLabels();
}

void UOptionsMenuWidget::FocusInitial()
{
    if (Combo_Resolution) { Combo_Resolution->SetKeyboardFocus(); return; }
    if (Btn_Back) { Btn_Back->SetKeyboardFocus(); return; }
}

/* ===================== 해상도 리스트 ===================== */

void UOptionsMenuWidget::BuildResolutionList()
{
    if (!Combo_Resolution) return;

    SupportedRes.Reset();
    TArray<FIntPoint> AllRes;
    if (UKismetSystemLibrary::GetSupportedFullscreenResolutions(AllRes))
    {
        AllRes.Sort([](const FIntPoint& A, const FIntPoint& B)
            {
                return (A.X < B.X) || (A.X == B.X && A.Y < B.Y);
            });

        // 너무 작은 해상도 제거(선택)
        for (const FIntPoint& R : AllRes)
        {
            if (R.X >= 1280) SupportedRes.Add(R);
        }
    }
    else
    {
        // 폴백
        SupportedRes = { {1280,720},{1600,900},{1920,1080},{2560,1440},{3840,2160} };
    }

    Combo_Resolution->ClearOptions();
    for (const FIntPoint& R : SupportedRes)
        Combo_Resolution->AddOption(ToResString(R));
}

/* ===================== 설정 로드/저장 ===================== */

void UOptionsMenuWidget::LoadFromConfig()
{
    const TCHAR* SectionAudio = TEXT("/Script/PosisonFrog.Audio");
    float V = 1.f;
    if (GConfig->GetFloat(SectionAudio, TEXT("Master"), V, GGameUserSettingsIni)) MasterVol = FMath::Clamp(V, 0.f, 1.f);
    if (GConfig->GetFloat(SectionAudio, TEXT("BGM"), V, GGameUserSettingsIni)) BgmVol = FMath::Clamp(V, 0.f, 1.f);
    if (GConfig->GetFloat(SectionAudio, TEXT("SFX"), V, GGameUserSettingsIni)) SfxVol = FMath::Clamp(V, 0.f, 1.f);

    if (Slider_Master) Slider_Master->SetValue(MasterVol);
    if (Slider_BGM)    Slider_BGM->SetValue(BgmVol);
    if (Slider_SFX)    Slider_SFX->SetValue(SfxVol);

    const TCHAR* SectionInput = TEXT("/Script/PosisonFrog.Input");
    if (GConfig->GetFloat(SectionInput, TEXT("MouseSensitivity"), V, GGameUserSettingsIni))
        MouseSensitivity = FMath::Clamp(V, 0.1f, 2.0f);

    // 밝기(감마)는 r.Gamma 또는 GameUserSettings Gamma를 함께 사용
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Gamma")))
    {
        BrightnessGamma = FMath::Clamp(CVar->GetFloat(), 0.5f, 3.0f);
    }
}

void UOptionsMenuWidget::SaveToConfig()
{
    const TCHAR* SectionAudio = TEXT("/Script/PosisonFrog.Audio");
    GConfig->SetFloat(SectionAudio, TEXT("Master"), MasterVol, GGameUserSettingsIni);
    GConfig->SetFloat(SectionAudio, TEXT("BGM"), BgmVol, GGameUserSettingsIni);
    GConfig->SetFloat(SectionAudio, TEXT("SFX"), SfxVol, GGameUserSettingsIni);

    const TCHAR* SectionInput = TEXT("/Script/PosisonFrog.Input");
    GConfig->SetFloat(SectionInput, TEXT("MouseSensitivity"), MouseSensitivity, GGameUserSettingsIni);

    GConfig->Flush(false, GGameUserSettingsIni);
}

/* ===================== 오디오 적용 ===================== */

// OptionsMenuWidget.cpp - ApplyVolumes 함수 수정
void UOptionsMenuWidget::ApplyVolumes(bool bPushMix)
{
    if (!OptionsSoundMix || !MasterClass || !BGMClass || !SFXClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Options] Missing SoundMix or SoundClass!"));
        return;
    }

    UWorld* World = GetWorld();
    if (!World) return;

    // SoundMix 설정
    if (bPushMix)
    {
        // 기존 것 제거 후 새로 Push
        UGameplayStatics::PopSoundMixModifier(World, OptionsSoundMix);
        
        // Duration = -1.0f (무한) 또는 0.0f로 설정
        UGameplayStatics::PushSoundMixModifier(World, OptionsSoundMix);
    }

    // 볼륨 적용 (0.0001f로 완전 0 방지)
    float ClampedMaster = FMath::Max(MasterVol, 0.0001f);
    float ClampedBGM = FMath::Max(BgmVol, 0.0001f);
    float ClampedSFX = FMath::Max(SfxVol, 0.0001f);

    UGameplayStatics::SetSoundMixClassOverride(
        World,
        OptionsSoundMix,
        MasterClass,
        ClampedMaster,
        1.0f,  // Pitch
        0.0f,  // FadeInTime
        true   // bApplyToChildren
    );

    UGameplayStatics::SetSoundMixClassOverride(
        World,
        OptionsSoundMix,
        BGMClass,
        ClampedBGM,
        1.0f,
        0.0f,
        true
    );

    UGameplayStatics::SetSoundMixClassOverride(
        World,
        OptionsSoundMix,
        SFXClass,
        ClampedSFX,
        1.0f,
        0.0f,
        true
    );

    UE_LOG(LogTemp, Log, TEXT("[Options] Applied volumes - Master: %.2f, BGM: %.2f, SFX: %.2f"), 
           MasterVol, BgmVol, SfxVol);
}

/* ===================== UI ←→ 시스템 동기화 ===================== */

void UOptionsMenuWidget::SyncUIFromSettings()
{
    if (UGameUserSettings* GS = GEngine->GetGameUserSettings())
    {
        // 창 모드
        const EWindowMode::Type WM = GS->GetFullscreenMode();
        if (Combo_WindowMode)
        {
            Combo_WindowMode->ClearOptions();
            Combo_WindowMode->AddOption(TEXT("Fullscreen"));
            Combo_WindowMode->AddOption(TEXT("WindowedFullscreen"));
            Combo_WindowMode->AddOption(TEXT("Windowed"));
            Combo_WindowMode->SetSelectedOption(FromWindowMode(WM));
        }

        // 해상도
        const FIntPoint CurRes = GS->GetScreenResolution();
        const FString Sel = ToResString(CurRes);
        if (Combo_Resolution && Combo_Resolution->GetOptionCount() > 0)
        {
            // UComboBoxString에서 옵션 찾기
            bool bFound = false;
            for (int32 i = 0; i < Combo_Resolution->GetOptionCount(); ++i)
            {
                if (Combo_Resolution->GetOptionAtIndex(i) == Sel)
                {
                    Combo_Resolution->SetSelectedIndex(i);
                    bFound = true;
                    break;
                }
            }
            
            // 해당 해상도가 없으면 마지막 옵션 선택
            if (!bFound)
            {
                Combo_Resolution->SetSelectedIndex(Combo_Resolution->GetOptionCount() - 1);
            }
        }

        // VSync
        if (Check_VSync)
            Check_VSync->SetIsChecked(GS->IsVSyncEnabled());
    }
}

/* ===================== 그래픽 콜백/적용 ===================== */

void UOptionsMenuWidget::OnResolutionChanged(FString Item, ESelectInfo::Type Type)
{
    if (Type == ESelectInfo::Direct) return; // 코드로 설정된 경우 무시
    ApplyGraphicsFromUI(false);
}

void UOptionsMenuWidget::OnResolutionLeftClicked()
{
    if (!Combo_Resolution) return;
    
    int32 CurrentIndex = Combo_Resolution->GetSelectedIndex();
    int32 NewIndex = CurrentIndex - 1;
    
    if (NewIndex < 0)
    {
        NewIndex = Combo_Resolution->GetOptionCount() - 1; // 순환
    }
    
    Combo_Resolution->SetSelectedIndex(NewIndex);
    ApplyGraphicsFromUI(false);
}

void UOptionsMenuWidget::OnResolutionRightClicked()
{
    if (!Combo_Resolution) return;
    
    int32 CurrentIndex = Combo_Resolution->GetSelectedIndex();
    int32 NewIndex = CurrentIndex + 1;
    
    if (NewIndex >= Combo_Resolution->GetOptionCount())
    {
        NewIndex = 0; // 순환
    }
    
    Combo_Resolution->SetSelectedIndex(NewIndex);
    ApplyGraphicsFromUI(false);
}

void UOptionsMenuWidget::OnWindowModeChanged(FString Item, ESelectInfo::Type Type)
{
    if (Type == ESelectInfo::Direct) return;
    ApplyGraphicsFromUI(false);
}

void UOptionsMenuWidget::OnWindowModeLeftClicked()
{
    if (!Combo_WindowMode) return;
    
    int32 CurrentIndex = Combo_WindowMode->GetSelectedIndex();
    int32 NewIndex = CurrentIndex - 1;
    
    if (NewIndex < 0)
    {
        NewIndex = Combo_WindowMode->GetOptionCount() - 1;
    }
    
    Combo_WindowMode->SetSelectedIndex(NewIndex);
    ApplyGraphicsFromUI(false);
}

void UOptionsMenuWidget::OnWindowModeRightClicked()
{
    if (!Combo_WindowMode) return;
    
    int32 CurrentIndex = Combo_WindowMode->GetSelectedIndex();
    int32 NewIndex = CurrentIndex + 1;
    
    if (NewIndex >= Combo_WindowMode->GetOptionCount())
    {
        NewIndex = 0;
    }
    
    Combo_WindowMode->SetSelectedIndex(NewIndex);
    ApplyGraphicsFromUI(false);
}

void UOptionsMenuWidget::OnVSyncChanged(bool /*bChecked*/)
{
    ApplyGraphicsFromUI(false);
}

static bool ParseResString(const FString& In, int32& OutW, int32& OutH)
{
    FString L, R;
    if (!In.Split(TEXT("x"), &L, &R)) return false;
    OutW = FCString::Atoi(*L);
    OutH = FCString::Atoi(*R);
    return (OutW > 0 && OutH > 0);
}

void UOptionsMenuWidget::ApplyGraphicsFromUI(bool bSave)
{
    if (UGameUserSettings* GS = GEngine->GetGameUserSettings())
    {
        // 해상도
        if (Combo_Resolution)
        {
            int32 W = 0, H = 0;
            if (ParseResString(Combo_Resolution->GetSelectedOption(), W, H))
                GS->SetScreenResolution(FIntPoint(W, H));
        }

        // 창 모드
        if (Combo_WindowMode)
        {
            const FString WM = Combo_WindowMode->GetSelectedOption();
            GS->SetFullscreenMode(ToWindowMode(WM));
        }

        // VSync
        if (Check_VSync)
            GS->SetVSyncEnabled(Check_VSync->IsChecked());

        GS->ApplySettings(false);
        if (bSave) GS->SaveSettings();
    }
}

/* ===================== 오디오 콜백 ===================== */
// void UOptionsMenuWidget::OnMasterVolChanged(float V) { MasterVol = FMath::Clamp(V, 0.f, 1.f); ApplyVolumes(); }
void UOptionsMenuWidget::OnMasterVolChanged(float V)
{
    MasterVol = V;
    
    // UI 업데이트
    UpdateVolumeProgressBar(ProgressBar_MasterFill, Text_MasterVolume, V);
    UpdateSliderThumb(Slider_Master, V, MasterThumb_Low, MasterThumb_Mid, 
                      MasterThumb_High, MasterThreshold_LowToMid, MasterThreshold_MidToHigh);
    
    // 볼륨만 적용 (Push는 하지 않음)
    ApplyVolumes(false);  // ← false로 변경!
}

void UOptionsMenuWidget::OnBGMVolChanged(float V)
{
    BgmVol = V;
    ApplyVolumes(false);  // ← false로 변경!
}

void UOptionsMenuWidget::OnSFXVolChanged(float V)
{
    SfxVol = V;
    ApplyVolumes(false);  // ← false로 변경!
}
/* ===================== 밝기/마우스 감도 ===================== */

void UOptionsMenuWidget::OnBrightnessChanged(float V)
{
    BrightnessGamma = FMath::GetMappedRangeValueClamped(FVector2D(0.f, 1.f), FVector2D(0.5f, 3.0f), V);

#if ENGINE_MAJOR_VERSION >= 5
   /* if (UGameUserSettings* GS = GEngine->GetGameUserSettings())
    {
        // 일부 버전에서 SetGamma가 비활성일 수 있어 CVar도 함께 사용
        GS->SetGamma(BrightnessGamma);  <- 진짜 비활성화 되어있음 ㄷㄷ;;
    }*/
#endif
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Gamma")))
    {
        CVar->Set(BrightnessGamma, ECVF_SetByGameSetting);
    }
}

void UOptionsMenuWidget::OnMouseSensChanged(float V)
{
    MouseSensitivity = FMath::GetMappedRangeValueClamped(FVector2D(0.f, 1.f), FVector2D(0.1f, 2.0f), V);

    UpdateMouseSensUI(V);
    UpdateSliderThumb(Slider_MouseSens, V,
                         MouseThumb_Low, MouseThumb_Mid, MouseThumb_High,
                         MouseThreshold_LowToMid, MouseThreshold_MidToHigh);
    
    if (ACPlayerController* PC = Cast<ACPlayerController>(GetOwningPlayer()))
    {
		PC->SetMouseSensitivity(MouseSensitivity); 
    }
    
    if (GConfig)
    {
        const TCHAR* Section = TEXT("/Script/PosisonFrog.Input");
        GConfig->SetFloat(Section, TEXT("MouseSensitivity"), MouseSensitivity, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }
}

/* ===================== 버튼 ===================== */

void UOptionsMenuWidget::OnApplyClicked()
{
    ApplyGraphicsFromUI(true);
    SaveToConfig();
}

void UOptionsMenuWidget::OnDefaultClicked()
{
    // 그래픽 기본값(예시)
    if (Combo_Resolution)  Combo_Resolution->SetSelectedOption(TEXT("1920x1080"));
    if (Combo_WindowMode)  Combo_WindowMode->SetSelectedOption(TEXT("WindowedFullscreen"));
    if (Check_VSync)       Check_VSync->SetIsChecked(true);
    ApplyGraphicsFromUI(false);

    // 오디오 기본값
    MasterVol = BgmVol = SfxVol = 1.f;
    if (Slider_Master) Slider_Master->SetValue(MasterVol);
    if (Slider_BGM)    Slider_BGM->SetValue(BgmVol);
    if (Slider_SFX)    Slider_SFX->SetValue(SfxVol);
    ApplyVolumes();

    // 밝기/감도 기본값
    BrightnessGamma = 2.2f;
    MouseSensitivity = 1.0f;
    if (Slider_Brightness)
        Slider_Brightness->SetValue(FMath::GetMappedRangeValueClamped(FVector2D(0.5f, 3.0f), FVector2D(0.f, 1.f), BrightnessGamma));
    if (Slider_MouseSens)
        Slider_MouseSens->SetValue(FMath::GetMappedRangeValueClamped(FVector2D(0.1f, 2.0f), FVector2D(0.f, 1.f), MouseSensitivity));

    // 리바인드 초기화는 필요 시 구현(기본 키로 되돌리기)
}

void UOptionsMenuWidget::OnBackClicked()
{
    OnClosed.Broadcast();
}

/* ===================== 리바인드 (Enhanced Input) ===================== */

void UOptionsMenuWidget::EnsureRuntimeMapping()
{
    if (!RuntimeMapping && BaseMappingContext)
    {
        RuntimeMapping = DuplicateObject<UInputMappingContext>(BaseMappingContext, this);
    }

    RuntimeMapping = DuplicateObject<UInputMappingContext>(BaseMappingContext, this);
    if (!RuntimeGamepadMapping && BaseGamepadMappingContext)
    {
        RuntimeGamepadMapping = DuplicateObject<UInputMappingContext>(BaseGamepadMappingContext, this);
    }
    
    if (const ULocalPlayer* LP = GetOwningLocalPlayer())
    {
        if (auto* Subsys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
        {
            if (RuntimeMapping && !bKeyboardContextAdded)
            {
                Subsys->AddMappingContext(RuntimeMapping, BaseMappingPriority);
                bKeyboardContextAdded = true;
            }
           
            if (RuntimeGamepadMapping && !bGamepadContextAdded)
            {
                Subsys->AddMappingContext(RuntimeGamepadMapping, GamepadMappingPriority);
                bGamepadContextAdded = true;
            }
        }
    }

    if (!RuntimeMapping && !RuntimeGamepadMapping)
    {
        return;
    }

    // 저장된 키 불러오기
    auto ApplySaved = [&](UInputAction* Action, const TCHAR* KeyName)
        {
            if (!Action) return;
            FString S;
            if (GConfig && GConfig->GetString(TEXT("/Script/PosisonFrog.Input"), KeyName, S, GGameUserSettingsIni) && !S.IsEmpty())
            {
                const FKey K(*S);
                if (K.IsValid()) ApplyRebind(Action, K);
            }
        };
    ApplySaved(Action_Attack, TEXT("Key_Attack"));
    ApplySaved(Action_Dash, TEXT("Key_Dash"));
}

void UOptionsMenuWidget::UpdateKeyLabels()
{
    if (!RuntimeMapping && !RuntimeGamepadMapping) return;

    auto FirstKeyFor = [&](UInputAction* Action)->FKey
    {
        auto FindInContext = [&](UInputMappingContext* Context)->FKey
        {
            if (!Context) return EKeys::Invalid;
            const TArray<FEnhancedActionKeyMapping>& Maps = Context->GetMappings();
            for (const auto& M : Maps)
            {
                if (M.Action == Action)
                {
                    return M.Key;
                }
            }
            return EKeys::Invalid;
        };
        
        FKey Key = FindInContext(RuntimeMapping);
        if (!Key.IsValid())
        {
            Key = FindInContext(RuntimeGamepadMapping);
        }
        return Key;
    };

    if (Label_AttackKey) Label_AttackKey->SetText(FText::FromString(KeyToText(FirstKeyFor(Action_Attack))));
    if (Label_DashKey)   Label_DashKey->SetText(FText::FromString(KeyToText(FirstKeyFor(Action_Dash))));
}

void UOptionsMenuWidget::OnRebindAttack()
{
    EnsureRuntimeMapping();
    PendingAction = Action_Attack;
    bWaitingForRebind = (PendingAction != nullptr);
    SetKeyboardFocus();
}

void UOptionsMenuWidget::OnRebindDash()
{
    EnsureRuntimeMapping();
    PendingAction = Action_Dash;
    bWaitingForRebind = (PendingAction != nullptr);
    SetKeyboardFocus();
}

FReply UOptionsMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (bWaitingForRebind && PendingAction)
    {
        const FKey NewKey = InKeyEvent.GetKey();
        if (NewKey.IsValid() && NewKey != EKeys::Escape)
        {
            ApplyRebind(PendingAction, NewKey);

            // 저장
            if (GConfig)
            {
                const TCHAR* Section = TEXT("/Script/PosisonFrog.Input");
                const TCHAR* Name = (PendingAction == Action_Attack) ? TEXT("Key_Attack") : TEXT("Key_Dash");
                GConfig->SetString(Section, Name, *NewKey.GetFName().ToString(), GGameUserSettingsIni);
                GConfig->Flush(false, GGameUserSettingsIni);
            }
        }
        bWaitingForRebind = false;
        PendingAction = nullptr;
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UOptionsMenuWidget::ApplyRebind(UInputAction* Action, const FKey& NewKey)
{
    if (!Action) return;
    
    UInputMappingContext* TargetContext = nullptr;
    if (NewKey.IsGamepadKey())
    {
        TargetContext = RuntimeGamepadMapping ? RuntimeGamepadMapping : RuntimeMapping;
    }
    else
    {
        TargetContext = RuntimeMapping ? RuntimeMapping : RuntimeGamepadMapping;
    }
   
    if (!TargetContext)
    {
        return;
    }
  
    auto RemoveExisting = [&](UInputMappingContext* Context)
    {
        if (!Context) return;
        TArray<FKey> ToRemove;
        const TArray<FEnhancedActionKeyMapping>& Maps = Context->GetMappings();
        for (const auto& M : Maps)
        {
            if (M.Action == Action)
            {
                ToRemove.Add(M.Key);
            }
        }
        for (const FKey& Key : ToRemove)
        {
            Context->UnmapKey(Action, Key);
        }
    };
    RemoveExisting(TargetContext);
 
    // 새 키 매핑
    TargetContext->MapKey(Action, NewKey);
    UpdateKeyLabels();
}
/* ===================== 유틸 ===================== */

FString UOptionsMenuWidget::ToResString(const FIntPoint& P) const
{
    return FString::Printf(TEXT("%dx%d"), P.X, P.Y);
}

EWindowMode::Type UOptionsMenuWidget::ToWindowMode(const FString& Item) const
{
    if (Item.Equals(TEXT("Fullscreen")))         return EWindowMode::Fullscreen;
    if (Item.Equals(TEXT("WindowedFullscreen"))) return EWindowMode::WindowedFullscreen;
    return EWindowMode::Windowed;
}

FString UOptionsMenuWidget::FromWindowMode(EWindowMode::Type M) const
{
    switch (M)
    {
    case EWindowMode::Fullscreen:         return TEXT("Fullscreen");
    case EWindowMode::WindowedFullscreen: return TEXT("WindowedFullscreen");
    default:                              return TEXT("Windowed");
    }
}

FString UOptionsMenuWidget::KeyToText(const FKey& Key) const
{
    return Key.IsValid() ? Key.GetDisplayName().ToString() : TEXT("-");
}

void UOptionsMenuWidget::UpdateVolumeProgressBar(UProgressBar* ProgressBar, UTextBlock* TextBlock, float Value)
{
    // Progress Bar 업데이트
    if (ProgressBar)
    {
        ProgressBar->SetPercent(Value);
    }
    
    // 퍼센트 텍스트 업데이트
    if (TextBlock)
    {
        int32 Percent = FMath::RoundToInt(Value * 100.0f);
        TextBlock->SetText(FText::Format(FText::FromString("{0}%"), Percent));
    }
}

void UOptionsMenuWidget::UpdateMouseSensUI(float NormalizedValue)
{
    // Progress Bar 업데이트
    if (ProgressBar_MouseSensFill)
    {
        ProgressBar_MouseSensFill->SetPercent(NormalizedValue);
    }
    
    // 퍼센트 텍스트 업데이트
    if (Text_MouseSens)
    {
        int32 Percent = FMath::RoundToInt(NormalizedValue * 100.0f);
        Text_MouseSens->SetText(FText::Format(FText::FromString("{0}%"), Percent));
    }
}

void UOptionsMenuWidget::UpdateSliderThumb(USlider* Slider, float Percent, UTexture2D* LowImg, UTexture2D* MidImg,
    UTexture2D* HighImg, float LowToMid, float MidToHigh)
{
    if (!Slider) return;

    UTexture2D* SelectedImage = nullptr;

    if (Percent <= LowToMid)
        SelectedImage = LowImg;
    else if (Percent <= MidToHigh)
        SelectedImage = MidImg;
    else
        SelectedImage = HighImg;

    if (!SelectedImage) return;

    FSliderStyle SliderStyle = Slider->GetWidgetStyle();

    FSlateBrush NewThumbBrush;
    NewThumbBrush.SetResourceObject(SelectedImage);
    NewThumbBrush.ImageSize = FVector2d(115.0f, 92.0f);
    NewThumbBrush.DrawAs = ESlateBrushDrawType::Image;

    SliderStyle.NormalThumbImage = NewThumbBrush;
    SliderStyle.HoveredThumbImage = NewThumbBrush;
    SliderStyle.DisabledThumbImage = NewThumbBrush;

    Slider->SetWidgetStyle(SliderStyle);
}

/*Build.cs

PublicDependencyModuleNames.AddRange(new string[] {
    "Core","CoreUObject","Engine","InputCore","UMG","EnhancedInput"
});
PrivateDependencyModuleNames.AddRange(new string[] { "Slate","SlateCore" });*/

/*UMG 위젯 디자이너에서 아래 이름으로 컨트롤 배치(없으면 Optional로 생략 가능)

Combo_Resolution, Combo_WindowMode, Check_VSync

Slider_Master, Slider_BGM, Slider_SFX

Btn_Apply, Btn_Default, Btn_Back

(선택) Slider_Brightness, Slider_MouseSens

(선택) Label_AttackKey, Label_DashKey, Btn_RebindAttack, Btn_RebindDash

사운드 리소스(에디터에서 지정)

OptionsSoundMix, MasterClass, BGMClass, SFXClass

Enhanced Input 리소스(에디터에서 지정)

BaseMappingContext(프로젝트의 기본 IMC 에셋)

Action_Attack, Action_Dash(UInputAction 에셋
-메인 메뉴와 연결
UCMainMenuWidget의 OptionsMenuClass에 이 위젯 클래스를 할당

OnClosed 델리게이트로 메인 메뉴 복귀 처리*/
