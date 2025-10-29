// CPlayerController.cpp

#include "00_Character/00_Player/CPlayerController.h"
#include "00_Character/00_Player/CPlayerCharacter.h"

// Enhanced Input
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "00_Character/02_Component/00_PlayerComponent/CEnhancedInputComponent.h" // 프로젝트용(선택)
#include "00_Character/02_Component/00_PlayerComponent/CInputConfig.h"           // 프로젝트용(선택)

// 위젯 (프로젝트 경로에 맞춰 통일)
#include "01_Widget/CPlayerWidget.h"

#include "99_Util/CLog.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

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

    // 입력 모드 / 마우스 커서
    FInputModeGameOnly Mode;
    SetInputMode(Mode);
    bShowMouseCursor = false;
    

    // Enhanced Input IMC를 C++에서 적용
    if (ULocalPlayer* LP = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            if (DefaultMappingContext)
            {
                Subsys->AddMappingContext(DefaultMappingContext, MappingPriority);
            }

            if (GamepadMappingContext)
            {
                Subsys->AddMappingContext(GamepadMappingContext, GamepadMappingPriority);
            }
        }
    }

    // 시작은 게임 전용 입력 모드
    SetInputMode_GameOnly();

    
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
    if (bIsPausedMenuOpen)
        HidePauseMenu();
    else
        ShowPauseMenu();
}

void ACPlayerController::HandleToggleMouse()
{
    bShowMouseCursor = !bShowMouseCursor;

    if (bShowMouseCursor)
        SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
    else
        SetInputMode_GameOnly();
}


// ─────────────────────────────────────────────────────────────
// 메뉴/입력 모드
// ─────────────────────────────────────────────────────────────
void ACPlayerController::ShowPauseMenu()
{
    if (bIsPausedMenuOpen) return;

    // 게임 일시정지
    SetPause(true);

    // 위젯 생성/표시
    if (PauseMenuClass && !PauseMenuInstance)
    {
        PauseMenuInstance = CreateWidget<UUserWidget>(this, PauseMenuClass);
    }

    if (PauseMenuInstance && !PauseMenuInstance->IsInViewport())
    {
        PauseMenuInstance->AddToViewport(1000);
    }

    bIsPausedMenuOpen = true;
    SetInputMode_UIOnly();
}

void ACPlayerController::HidePauseMenu()
{
    if (!bIsPausedMenuOpen) return;

    // 위젯 제거
    if (PauseMenuInstance && PauseMenuInstance->IsInViewport())
    {
        PauseMenuInstance->RemoveFromParent();
    }

    // 게임 재개
    SetPause(false);

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

void ACPlayerController::SetInputMode_UIOnly()
{
    FInputModeUIOnly Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
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