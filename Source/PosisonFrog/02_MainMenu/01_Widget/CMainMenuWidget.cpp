#include "02_MainMenu/01_Widget/CMainMenuWidget.h"
#include "02_MainMenu/01_Widget/OptionsMenuWidget.h" // 옵션 위젯 사용 시

#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/Image.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

#ifndef WITH_NIAGARA
#define WITH_NIAGARA 0
#endif

#if WITH_NIAGARA
#include "NiagaraFunctionLibrary.h"
#endif

namespace
{
    FStreamableManager& GetMenuStreamableManager()
    {
        if (UAssetManager* AssetManager = UAssetManager::GetIfValid())
        {
            return AssetManager->GetStreamableManager();
        }
        
        static FStreamableManager StaticStreamableManager;
        return StaticStreamableManager;
    }
}

void UCMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 메인 레벨을 미리 로딩해 두어 컷신 재생 중 빠르게 진입할 수 있도록 한다.
    BeginPreloadStartLevel();
    
    // --- 중복 바인딩 방지 후 바인딩 ---
    if (MainMenu_StartButton)
    {
        MainMenu_StartButton->OnHovered.RemoveAll(this);
        MainMenu_StartButton->OnPressed.RemoveAll(this);
        MainMenu_StartButton->OnClicked.RemoveAll(this);

        MainMenu_StartButton->OnHovered.AddDynamic(this, &UCMainMenuWidget::OnStartButtonHovered);
        MainMenu_StartButton->OnUnhovered.AddDynamic(this, &UCMainMenuWidget::OnStartButtonUnhovered);
        MainMenu_StartButton->OnPressed.AddDynamic(this, &UCMainMenuWidget::OnAnyButtonPressed);
        MainMenu_StartButton->OnClicked.AddDynamic(this, &UCMainMenuWidget::OnStartClicked);
    }
    if (MainMenu_StartArrow)
        MainMenu_StartArrow->SetVisibility(ESlateVisibility::Hidden);

    if (MainMenu_SettingButton)
    {
        MainMenu_SettingButton->OnHovered.RemoveAll(this);
        MainMenu_SettingButton->OnPressed.RemoveAll(this);
        MainMenu_SettingButton->OnClicked.RemoveAll(this);

        MainMenu_SettingButton->OnHovered.AddDynamic(this, &UCMainMenuWidget::OnSettingButtonHovered);
        MainMenu_SettingButton->OnUnhovered.AddDynamic(this, &UCMainMenuWidget::OnSettingButtonUnhovered);
        MainMenu_SettingButton->OnPressed.AddDynamic(this, &UCMainMenuWidget::OnAnyButtonPressed);
        MainMenu_SettingButton->OnClicked.AddDynamic(this, &UCMainMenuWidget::OnSettingClicked);
    }
    if (MainMenu_SettingArrow)
        MainMenu_SettingArrow->SetVisibility(ESlateVisibility::Hidden);

    if (MainMenu_ExitButton)
    {
        MainMenu_ExitButton->OnHovered.RemoveAll(this);
        MainMenu_ExitButton->OnPressed.RemoveAll(this);
        MainMenu_ExitButton->OnClicked.RemoveAll(this);

        MainMenu_ExitButton->OnHovered.AddDynamic(this, &UCMainMenuWidget::OnExitButtonHovered);
        MainMenu_ExitButton->OnUnhovered.AddDynamic(this, &UCMainMenuWidget::OnExitButtonUnhovered);
        MainMenu_ExitButton->OnPressed.AddDynamic(this, &UCMainMenuWidget::OnAnyButtonPressed);
        MainMenu_ExitButton->OnClicked.AddDynamic(this, &UCMainMenuWidget::OnExitClicked);
    }
    if (MainMenu_ExitArrow)
        MainMenu_ExitArrow->SetVisibility(ESlateVisibility::Hidden);
}

void UCMainMenuWidget::NativeDestruct()
{
    // 입력 락 타이머 정리
    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().ClearTimer(InputUnlockTimer);
    }
    
    if (StartLevelStreamHandle.IsValid())
    {
        StartLevelStreamHandle->ReleaseHandle();
        StartLevelStreamHandle.Reset();
    }
    
    Super::NativeDestruct();
}

/* ─────────────────────────────
 * 버튼 핸들러
 * ───────────────────────────── */

void UCMainMenuWidget::OnStartClicked()
{
    if (bInputLocked) return;
    LockInput();

    PlayUISound(SFX_Click);
    if (Anim_Click) PlayAnimation(Anim_Click, 0.08f, 1, EUMGSequencePlayMode::Forward, 1.0f);

    // (선택) 페이드아웃 연출
    if (Anim_FadeOut) PlayAnimation(Anim_FadeOut);

    bTravelRequested = true;
    bCutsceneFinished = !bWaitForCutsceneBeforeTravel;
    
    BeginPreloadStartLevel();
    
    // Blueprint에서 컷신을 재생할 수 있도록 이벤트를 호출한다.
    BP_OnStartGameRequested();
    
    // 컷신이 없거나 이미 종료되었다면 즉시 전환을 시도한다.
    TryTravelToStartLevel();
}

void UCMainMenuWidget::OnSettingClicked()
{
    
    if (bInputLocked) return;
    LockInput();

    PlayUISound(SFX_Click);
    if (Anim_Click) PlayAnimation(Anim_Click, 0.08f, 1, EUMGSequencePlayMode::Forward, 1.0f);

    if (!OptionsMenu && OptionsMenuClass)
    {
        OptionsMenu = CreateWidget<UOptionsMenuWidget>(GetOwningPlayer(), OptionsMenuClass);
        if (OptionsMenu)
        {
            OptionsMenu->OnClosed.AddDynamic(this, &UCMainMenuWidget::OnOptionsClosed);
        }
    }

    if (OptionsMenu)
    {
        OptionsMenu->AddToViewport(100);   // 메인 위젯 위
        SetMainPanelVisible(false);        // 메인 메뉴 패널 숨김(클릭 차단)
        OptionsMenu->FocusInitial();
    }
}

void UCMainMenuWidget::OnExitClicked()
{
    if (bInputLocked) return;
    LockInput();

    PlayUISound(SFX_Click);
    if (Anim_Click) PlayAnimation(Anim_Click, 0.08f, 1, EUMGSequencePlayMode::Forward, 1.0f);

    UKismetSystemLibrary::QuitGame(GetWorld(), GetOwningPlayer(), EQuitPreference::Quit, false);
}

/* ─────────────────────────────
 * 공통 연출 / SFX / VFX
 * ───────────────────────────── */
void UCMainMenuWidget::OnAnyButtonPressed()
{
    // 필요 시 프레스 전용 짧은 연출만 (클릭 연출은 OnClicked에서)
    // 마우스 위치 VFX 등
    if (APlayerController* PC = GetOwningPlayer())
    {
        float MX = 0, MY = 0;
        if (PC->GetMousePosition(MX, MY))
        {
            SpawnClickVFX(FVector2D(MX, MY));
        }
    }
}

// Start 버튼 호버
void UCMainMenuWidget::OnStartButtonHovered()
{
    if (MainMenu_StartArrow)
        MainMenu_StartArrow->SetVisibility(ESlateVisibility::Visible);
}

void UCMainMenuWidget::OnStartButtonUnhovered()
{
    if (MainMenu_StartArrow)
        MainMenu_StartArrow->SetVisibility(ESlateVisibility::Hidden);
}

// Setting 버튼 호버
void UCMainMenuWidget::OnSettingButtonHovered()
{
    if (MainMenu_SettingArrow)
        MainMenu_SettingArrow->SetVisibility(ESlateVisibility::Visible);
}

void UCMainMenuWidget::OnSettingButtonUnhovered()
{
    if (MainMenu_SettingArrow)
        MainMenu_SettingArrow->SetVisibility(ESlateVisibility::Hidden);
}

// Exit 버튼 호버
void UCMainMenuWidget::OnExitButtonHovered()
{
    if (MainMenu_ExitArrow)
        MainMenu_ExitArrow->SetVisibility(ESlateVisibility::Visible);
}

void UCMainMenuWidget::OnExitButtonUnhovered()
{
    if (MainMenu_ExitArrow)
        MainMenu_ExitArrow->SetVisibility(ESlateVisibility::Hidden);
}

void UCMainMenuWidget::PlayUISound(USoundBase* SFX)
{
    if (!SFX) return;
    UGameplayStatics::PlaySound2D(this, SFX);
}

void UCMainMenuWidget::SpawnClickVFX(FVector2D ScreenPos)
{
#if WITH_NIAGARA
    if (!VFX_Click) return;

    // 화면 좌표 → 월드 디컨버전(2D UI에서 클릭 파티클을 살짝 카메라 앞에 뿌리는 방식)
    if (APlayerController* PC = GetOwningPlayer())
    {
        FVector World, Dir;
        if (PC->DeprojectScreenPositionToWorld(ScreenPos.X, ScreenPos.Y, World, Dir))
        {
            const FVector SpawnAt = World + Dir * 200.f; // 카메라 앞 200cm
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), VFX_Click, SpawnAt, FRotator::ZeroRotator, FVector(1.f));
        }
    }
#endif
}

/* ─────────────────────────────
 * 옵션 닫힘 / 패널 토글
 * ───────────────────────────── */

void UCMainMenuWidget::OnOptionsClosed()
{
    if (OptionsMenu)
    {
        OptionsMenu->RemoveFromParent();
        // 포인터 유지(다시 열 때 재사용) 또는 nullptr로 파기 중 택1
        // OptionsMenu = nullptr;
    }
    SetMainPanelVisible(true);
}

void UCMainMenuWidget::SetMainPanelVisible(bool bVisible)
{
    if (MainRootPanel)
    {
        MainRootPanel->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible
            : ESlateVisibility::Collapsed);
    }
}

void UCMainMenuWidget::BeginPreloadStartLevel()
{
    if (StartLevelAsset.IsNull())
    {
        bLevelPreloaded = true;
        return;
    }
    
    if (StartLevelAsset.IsValid())
    {
        bLevelPreloaded = true;
        return;
    }
    
    if (StartLevelStreamHandle.IsValid())
    {
        if (StartLevelStreamHandle->HasLoadCompleted())
        {
            bLevelPreloaded = true;
            TryTravelToStartLevel();
        }
        return;
    }
    
    StartLevelStreamHandle = GetMenuStreamableManager().RequestAsyncLoad(
        StartLevelAsset.ToSoftObjectPath(),
        FStreamableDelegate::CreateUObject(this, &UCMainMenuWidget::OnStartLevelPreloadCompleted),
        FStreamableManager::AsyncLoadHighPriority);
    
    if (!StartLevelStreamHandle.IsValid())
    {
        // 로드 요청이 실패한 경우 즉시 전환할 수 있도록 true 처리
        bLevelPreloaded = true;
    }
}

void UCMainMenuWidget::OnStartLevelPreloadCompleted()
{
    bLevelPreloaded = true;
    TryTravelToStartLevel();
}

void UCMainMenuWidget::NotifyCutsceneFinished()
{
    bCutsceneFinished = true;
    TryTravelToStartLevel();
}

void UCMainMenuWidget::TryTravelToStartLevel()
{
    if (!bTravelRequested || !bCutsceneFinished || !bLevelPreloaded)
    {
        return;
    }
    
    bTravelRequested = false;
    
    if (StartLevelStreamHandle.IsValid())
    {
        StartLevelStreamHandle->ReleaseHandle();
        StartLevelStreamHandle.Reset();
    }
    
    if (StartLevelAsset.IsValid())
    {
        const FString LevelPackageName = StartLevelAsset.GetLongPackageName();
        if (!LevelPackageName.IsEmpty())
        {
            UGameplayStatics::OpenLevel(this, FName(*LevelPackageName), true);
            return;
        }
    }
    
    if (!StartLevelName.IsNone())
    {
        UGameplayStatics::OpenLevel(this, StartLevelName, true);
    }
}

/* ─────────────────────────────
 * 더블클릭/스팸 방지
 * ───────────────────────────── */

void UCMainMenuWidget::LockInput()
{
    if (bInputLocked) return;
    bInputLocked = true;

    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().SetTimer(InputUnlockTimer, this, &UCMainMenuWidget::UnlockInput, InputLockDuration, false);
    }
}

void UCMainMenuWidget::UnlockInput()
{
    bInputLocked = false;
}

/*UMG 디자이너에서 버튼을 배치하고 이름을 다음과 같이 맞춰야 합니다.

MainMenu_StartButton, MainMenu_SettingButton, MainMenu_ExitButton

(선택) 루트 패널을 MainRootPanel로 BindWidget하면 옵션창 열 때 깔끔하게 숨깁니다.

(선택) 애니메이션 이름: Anim_Hover, Anim_Click, Anim_FadeOut

Start 레벨명은 StartLevelName(EditDefaultsOnly)에서 변경 가능합니다.

옵션 메뉴를 쓰려면 **OptionsMenuClass**에 이전에 드린 UOptionsMenuWidget BP/C++ 클래스를 할당해 주세요.

중복 클릭 방지는 InputLockDuration(기본 0.35초)로 조절합니다.*/