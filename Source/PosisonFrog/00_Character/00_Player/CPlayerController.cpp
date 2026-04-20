// CPlayerController.cpp

#include "00_Character/00_Player/CPlayerController.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "05_System/CPauseSubsystem.h"

// Enhanced Input
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "00_Character/02_Component/00_PlayerComponent/CEnhancedInputComponent.h" // 프로젝트용(선택)
#include "00_Character/02_Component/00_PlayerComponent/CInputConfig.h"           // 프로젝트용(선택)

// 위젯 (프로젝트 경로에 맞춰 통일)
#include "00_Character/CMainGameModeBase.h"
#include "01_Widget/CPauseMenuWidget.h"
#include "01_Widget/CPlayerWidget.h"

#include "99_Util/CLog.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

// ------------------------------------------------------------------
// 생성자
// ------------------------------------------------------------------
ACPlayerController::ACPlayerController()
{
    bShowMouseCursor = false;
    bEnableClickEvents = false;
    bEnableMouseOverEvents = false;
}


// ------------------------------------------------------------------
// BeginPlay
// ------------------------------------------------------------------
void ACPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // 시작은 게임 전용 입력 모드
    SetInputMode_GameOnly();

    if (GConfig)
    {
        float SavedSensitivity = 1.0f;
        const TCHAR* Section = TEXT("/Script/PosisonFrog.Input");
        
        if (GConfig->GetFloat(Section, TEXT("MouseSensitivity"), SavedSensitivity, GGameUserSettingsIni))
        {
            SetMouseSensitivity(SavedSensitivity);
        }
    }
    
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        CachedPauseSubsystem = GameInstance->GetSubsystem<UCPauseSubsystem>();
        if (CachedPauseSubsystem)
        {
            CachedPauseSubsystem->OnPauseStateChanged.AddDynamic(this, &ACPlayerController::HandlePauseStateChanged);
            HandlePauseStateChanged(CachedPauseSubsystem->GetPauseState());
        }
    }
    
    // 기본 입력 컨텍스트 적용
    ActivateGameInputMappings();
    
    /*// UI 생성은 로컬 컨트롤러 + 게임플레이 맵에서만
    if (IsLocalController() && ShouldCreatePlayerWidget())
    {
        CreatePlayerWidget();
    }

    if (UCHealOrbPoolSubsystem* Pool = GetGameInstance()->GetSubsystem<UCHealOrbPoolSubsystem>())
    {
        Pool->OnCountersChanged.RemoveAll(this);
        Pool->OnCountersChanged.AddDynamic(this, &ACPlayerController::OnHealOrbCountersChanged);

        if (OrbHUDWidget)
        {
            OrbHUDWidget->UpdateCounters(Pool->GetActiveCount(), Pool->GetTotalPicked());
        }
    }*/
}

void ACPlayerController::SetMouseSensitivity(float InSensitivity)
{
    MouseSensitivity = FMath::Clamp(InSensitivity, 0.1f, 2.0f);
}

void ACPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    // (선택) 캐릭터에 InputConfig 자동 주입
    if (ACPlayerCharacter* PC = Cast<ACPlayerCharacter>(InPawn))
    {
        if (DefaultInputConfig && PC->GetClass()->FindPropertyByName(TEXT("InputConfig")))
        {
            // 리플렉션을 피하고 싶으면 캐릭터에 Setter 함수를 노출해 직접 주입하세요.
            // 여기서는 안전한 캐스팅을 가정하지 않으니, 프로젝트에 맞게 보완하셔도 됩니다.
        }
    }
}

void ACPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (CachedPauseSubsystem)
    {
        CachedPauseSubsystem->OnPauseStateChanged.RemoveDynamic(this, &ACPlayerController::HandlePauseStateChanged);
        CachedPauseSubsystem = nullptr;
    }
    
    Super::EndPlay(EndPlayReason);
}

// ------------------------------------------------------------------
// SetupInputComponent
// ------------------------------------------------------------------
void ACPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // Enhanced Input 컴포넌트
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
    {
        if (IA_Pause)
        {
            EIC->BindAction(IA_Pause, ETriggerEvent::Started, this, &ACPlayerController::HandlePausePressed);
        }
        if (IA_ToggleMouse)
        {
            EIC->BindAction(IA_ToggleMouse, ETriggerEvent::Started, this, &ACPlayerController::HandleToggleMouse);
        }
    }
    else
    {
        // 폴백: ESC로 일시정지 (Enhanced Input 미사용 시)
        InputComponent->BindAction("Pause", IE_Pressed, this, &ACPlayerController::HandlePausePressed);
    }
}

// ─────────────────────────────────────────────────────────────
// 입력 핸들러
// ─────────────────────────────────────────────────────────────
void ACPlayerController::HandlePausePressed()
{
    if (CachedPauseSubsystem)
    {
        CachedPauseSubsystem->RequestTogglePause(this);
        return;
    }
    
    if (UGameplayStatics::IsGamePaused(this))
    {
        SetPause(false);
        HidePauseMenu();
    }
    else
    {
        SetPause(true);
        ShowPauseMenu();
    }
}

void ACPlayerController::HandleToggleMouse()
{
    bShowMouseCursor = !bShowMouseCursor;

    if (bShowMouseCursor)
    {
        if (bIsPausedMenuOpen && PauseMenuInstance)
        {
            SetInputMode_UIOnly(PauseMenuInstance);
        }
        else
        {
            SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
        }
    }
    else
        SetInputMode_GameOnly();
}

void ACPlayerController::HandlePauseStateChanged(EGamePauseState NewState)
{
    if (NewState == EGamePauseState::Paused)
    {
        ShowPauseMenu();
    }
    else
    {
        HidePauseMenu();
    }
}


// ─────────────────────────────────────────────────────────────
// 메뉴/입력 모드
// ─────────────────────────────────────────────────────────────
void ACPlayerController::ShowPauseMenu()
{
    if (bIsPausedMenuOpen) return;

    DeactivateGameInputMappings();
    ActivatePauseMappings();
    
    // 게임 일시정지
    // SetPause(true);
 
    // 위젯 생성/표시
    if (PauseMenuClass && !PauseMenuInstance)
    {
        PauseMenuInstance = CreateWidget<UPauseMenuWidget>(this, PauseMenuClass);
        if (PauseMenuInstance)
        {
            PauseMenuInstance->OnResumeRequested.AddDynamic(this, &ACPlayerController::HandlePauseMenuResumeRequested);
            PauseMenuInstance->OnRestartRequested.AddDynamic(this, &ACPlayerController::HandlePauseMenuRestartRequested);
            PauseMenuInstance->OnReturnToTitleRequested.AddDynamic(this, &ACPlayerController::HandlePauseMenuReturnToTitleRequested);
            PauseMenuInstance->OnExitRequested.AddDynamic(this, &ACPlayerController::HandlePauseMenuExitRequested);

            PauseMenuInstance->AddToViewport(1000);
        }
    }
 
    if (PauseMenuInstance)
    {
        PauseMenuInstance->SetVisibility(ESlateVisibility::Visible);
        PauseMenuInstance->ResetMenuState();
        PauseMenuInstance->FocusInitial();
    }
 
    ApplyPauseAudio();
    
    bIsPausedMenuOpen = true;
    SetInputMode_UIOnly(PauseMenuInstance);
}

void ACPlayerController::HidePauseMenu()
{
    if (!bIsPausedMenuOpen) return;

    // 위젯 제거
    /*if (PauseMenuInstance && PauseMenuInstance->IsInViewport())
    {
        PauseMenuInstance->RemoveFromParent();
    }*/

    if (PauseMenuInstance && PauseMenuInstance->IsInViewport())
    {
        PauseMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
    }

    //SetPause(false);
    DeactivatePauseMappings();
    ActivateGameInputMappings();
    RestorePauseAudio();
    
    bIsPausedMenuOpen = false;
    SetInputMode_GameOnly();
}

void ACPlayerController::SetInputMode_GameOnly()
{
    FInputModeGameOnly Mode;
    SetInputMode(Mode);
    bShowMouseCursor = false;
    bEnableClickEvents = false;
    bEnableMouseOverEvents = false;
}

void ACPlayerController::SetInputMode_UIOnly(UUserWidget* InWidgetToFocus)
{
    FInputModeUIOnly Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    if (InWidgetToFocus)
    {
        Mode.SetWidgetToFocus(InWidgetToFocus->TakeWidget());
    }
    SetInputMode(Mode);
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
}

UEnhancedInputLocalPlayerSubsystem* ACPlayerController::GetEnhancedInputSubsystem() const
{
    if (const ULocalPlayer* LP = GetLocalPlayer())
    {
        return LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    }
    
    return nullptr;
}

void ACPlayerController::ActivateGameInputMappings()
{
    if (UEnhancedInputLocalPlayerSubsystem* Subsys = GetEnhancedInputSubsystem())
    {
        if (PauseMappingContext && bPauseMappingActive)
        {
            Subsys->RemoveMappingContext(PauseMappingContext);
            bPauseMappingActive = false;
        }
            
        if (DefaultMappingContext && !bKeyboardMappingActive)
        {
            Subsys->AddMappingContext(DefaultMappingContext, MappingPriority);
            bKeyboardMappingActive = true;
        }
            
        if (GamepadMappingContext && !bGamepadMappingActive)
        {
            Subsys->AddMappingContext(GamepadMappingContext, GamepadMappingPriority);
            bGamepadMappingActive = true;
        }
    }
}

void ACPlayerController::DeactivateGameInputMappings()
{
    if (UEnhancedInputLocalPlayerSubsystem* Subsys = GetEnhancedInputSubsystem())
    {
        if (DefaultMappingContext && bKeyboardMappingActive)
        {
            Subsys->RemoveMappingContext(DefaultMappingContext);
            bKeyboardMappingActive = false;
        }
            
        if (GamepadMappingContext && bGamepadMappingActive)
        {
            Subsys->RemoveMappingContext(GamepadMappingContext);
            bGamepadMappingActive = false;
        }
    }
}

void ACPlayerController::ActivatePauseMappings()
{
    if (UEnhancedInputLocalPlayerSubsystem* Subsys = GetEnhancedInputSubsystem())
    {
        if (PauseMappingContext && !bPauseMappingActive)
        {
            Subsys->AddMappingContext(PauseMappingContext, PauseMappingPriority);
            bPauseMappingActive = true;
        }
    }
}

void ACPlayerController::DeactivatePauseMappings()
{
    if (UEnhancedInputLocalPlayerSubsystem* Subsys = GetEnhancedInputSubsystem())
    {
        if (PauseMappingContext && bPauseMappingActive)
        {
            Subsys->RemoveMappingContext(PauseMappingContext);
            bPauseMappingActive = false;
        }
    }
}

void ACPlayerController::ApplyPauseAudio()
{
    if (!GetWorld())
    {
        return;
    }
    
    GetWorldTimerManager().ClearTimer(PauseSoundMixTimerHandle);
    
    if (PauseSoundMix && PauseBGMClass)
    {
        const float TargetVolume = FMath::Clamp(PauseBGMVolumeMultiplier, 0.0f, 1.0f);
        UGameplayStatics::SetSoundMixClassOverride(GetWorld(), PauseSoundMix, PauseBGMClass, TargetVolume, 1.0f, PauseAudioFadeTime, true);
        UGameplayStatics::PushSoundMixModifier(GetWorld(), PauseSoundMix);
        bPauseSoundMixActive = true;
    }
    
    if (PauseEnterSound)
    {
        UGameplayStatics::PlaySound2D(this, PauseEnterSound);
    }
}

void ACPlayerController::RestorePauseAudio()
{
    if (!GetWorld())
    {
        return;
    }
    
    if (bPauseSoundMixActive && PauseSoundMix && PauseBGMClass)
    {
        GetWorldTimerManager().ClearTimer(PauseSoundMixTimerHandle);
        UGameplayStatics::SetSoundMixClassOverride(GetWorld(), PauseSoundMix, PauseBGMClass, 1.0f, 1.0f, PauseAudioFadeTime, true);
            
        if (PauseAudioFadeTime <= 0.0f)
        {
            ClearPauseAudioOverride();
        }
        else
        {
            GetWorldTimerManager().SetTimer(PauseSoundMixTimerHandle, this, &ACPlayerController::ClearPauseAudioOverride, PauseAudioFadeTime, false);
        }
    }
    
    if (PauseResumeSound)
    {
        UGameplayStatics::PlaySound2D(this, PauseResumeSound);
    }
}

void ACPlayerController::ClearPauseAudioOverride()
{
    if (!GetWorld())
    {
        return;
    }
    
    if (bPauseSoundMixActive && PauseSoundMix)
    {
        UGameplayStatics::PopSoundMixModifier(GetWorld(), PauseSoundMix);

        if (PauseBGMClass)
        {
            UGameplayStatics::ClearSoundMixClassOverride(GetWorld(), PauseSoundMix, PauseBGMClass);
        }
            
        bPauseSoundMixActive = false;
    }
}

void ACPlayerController::HandlePauseMenuResumeRequested()
{
    if (CachedPauseSubsystem)
    {
        CachedPauseSubsystem->RequestResume(this);
    }
    else
    {
        SetPause(false);
        HidePauseMenu();
    }
}

void ACPlayerController::HandlePauseMenuRestartRequested()
{
    if (CachedPauseSubsystem)
    {
        CachedPauseSubsystem->RequestResume(this);
    }
    else
    {
        SetPause(false);
        HidePauseMenu();
    }
    
    if (ACMainGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACMainGameModeBase>() : nullptr)
    {
        GameMode->RestartFromLastCheckpoint(this);
    }
}

void ACPlayerController::HandlePauseMenuReturnToTitleRequested()
{
    if (CachedPauseSubsystem)
    {
        CachedPauseSubsystem->RequestResume(this);
    }
    else
    {
        SetPause(false);
        HidePauseMenu();
    }
    
    if (ACMainGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACMainGameModeBase>() : nullptr)
    {
        GameMode->ReturnToTitleScreen();
    }
}

void ACPlayerController::HandlePauseMenuExitRequested()
{
    UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, true);
}

// ------------------------------------------------------------------
// Pawn 소유 시작/해제
// ------------------------------------------------------------------
/*
void ACPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    OwnerCharacter = Cast<ACPlayerCharacter>(InPawn);
    if (!OwnerCharacter)
    {
        CLog::Log(TEXT("[PC] 캐릭터 소유 실패"));
        return;
    }
    CLog::Log(TEXT("[PC] 캐릭터 소유 완료"));

    // HealthComponent 바인딩
    HealthComponent = OwnerCharacter->FindComponentByClass<UCHealthComponent>();
    if (HealthComponent)
    {
        HealthComponent->OnHealthChanged.AddDynamic(this, &ACPlayerController::HandleHealthChanged);

        // 위젯이 이미 만들어져 있으면 초기 갱신
        if (PlayerWidget)
        {
            UpdateHpUI();
        }
    }
}


void ACPlayerController::OnUnPossess()
{
    if (HealthComponent)
    {
        HealthComponent->OnHealthChanged.RemoveDynamic(this, &ACPlayerController::HandleHealthChanged);
        HealthComponent = nullptr;
    }
    OwnerCharacter = nullptr;

    Super::OnUnPossess();
}

// ------------------------------------------------------------------
// 입력 바인딩
// ------------------------------------------------------------------
void ACPlayerController::SetupInputBindings()
{
    check(InputConfig);
    check(CEnhancedInputComponent);

    // 컨트롤러에서 입력을 받아 캐릭터의 액션 함수로 위임
    CEnhancedInputComponent->BindActionByTag(
        InputConfig, CGameplayTags::InputTag_Move,
        ETriggerEvent::Triggered, this, &ACPlayerController::HandleMove);

    CEnhancedInputComponent->BindActionByTag(
        InputConfig, CGameplayTags::InputTag_Look,
        ETriggerEvent::Triggered, this, &ACPlayerController::HandleLook);

    CEnhancedInputComponent->BindActionByTag(
        InputConfig, CGameplayTags::InputTag_Dash,
        ETriggerEvent::Started, this, &ACPlayerController::HandleDashStart);

    CEnhancedInputComponent->BindActionByTag(
        InputConfig, CGameplayTags::InputTag_Attack,
        ETriggerEvent::Started, this, &ACPlayerController::HandleAttack);
}

// ------------------------------------------------------------------
// UI 생성 조건/생성
// ------------------------------------------------------------------
bool ACPlayerController::ShouldCreatePlayerWidget() const
{
    if (!PlayerWidgetClass)
        return false;

    const UWorld* World = GetWorld();
    if (!World)
        return false;

    const FString LevelName = World->GetMapName();

    // 메인메뉴/세팅 레벨에선 생성하지 않음 (PIE 접두사(UEDPIE_)가 붙어도 Contains로 판정)
    if (LevelName.Contains(TEXT("MainMenu")) ||
        LevelName.Contains(TEXT("Setting")))
    {
        return false;
    }

    return true;
}

void ACPlayerController::CreatePlayerWidget()
{
    if (PlayerWidgetClass && !PlayerWidget)
    {
        PlayerWidget = CreateWidget<UCPlayerWidget>(this, PlayerWidgetClass);
        if (PlayerWidget)
        {
            PlayerWidget->AddToViewport();
        }
    }

    if (OrbHUDWidgetClass && !OrbHUDWidget)
    {
        OrbHUDWidget = CreateWidget<UCOrbHUDWidget>(this, OrbHUDWidgetClass);
        if (OrbHUDWidget)
        {
            OrbHUDWidget->AddToViewport();
        }
    }
}

void ACPlayerController::OnHealOrbCountersChanged(int32 ActiveOrbs, int32 TotalPicked)
{
    if (OrbHUDWidget)
    {
        OrbHUDWidget->UpdateCounters(ActiveOrbs, TotalPicked);
    }
}

// ------------------------------------------------------------------
// 입력 핸들러 → 캐릭터 위임
// ------------------------------------------------------------------
void ACPlayerController::HandleMove(const FInputActionValue& Value)
{
    if (OwnerCharacter)
    {
        OwnerCharacter->Move(Value);
    }
}

void ACPlayerController::HandleLook(const FInputActionValue& Value)
{
    if (OwnerCharacter)
    {
        OwnerCharacter->Look(Value);
    }
}

void ACPlayerController::HandleDashStart()
{
    if (OwnerCharacter)
    {
        OwnerCharacter->DashStart();
    }
}

void ACPlayerController::HandleAttack()
{
    if (OwnerCharacter)
    {
        OwnerCharacter->Attack();
    }
}

// ------------------------------------------------------------------
// HP 위젯 갱신
// ------------------------------------------------------------------
void ACPlayerController::HandleHealthChanged(float CurrentHealth, float MaxHealth)
{
    if (PlayerWidget)
    {
        PlayerWidget->UpdateHpBar(CurrentHealth, MaxHealth);
    }
}

void ACPlayerController::UpdateHpUI() const
{
    if (PlayerWidget && HealthComponent)
    {
        PlayerWidget->UpdateHpBar(HealthComponent->GetHealth(), HealthComponent->GetMaxHealth());
    }
}
*/
