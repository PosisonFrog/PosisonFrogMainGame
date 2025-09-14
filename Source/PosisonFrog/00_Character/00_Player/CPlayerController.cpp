// CPlayerController.cpp

#include "00_Character/00_Player/CPlayerController.h"

#include "00_Character/00_Player/CPlayerCharacter.h"

// Enhanced Input
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

// 우리 프로젝트 컴포넌트/유틸
#include "00_Character/02_Component/CEnhancedInputComponent.h"
#include "00_Character/02_Component/CGameplayTags.h"
#include "00_Character/02_Component/CHealthComponent.h"

// 위젯 (프로젝트 경로에 맞춰 통일)
#include "01_Widget/CPlayerWidget.h"

#include "99_Util/CLog.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

// ------------------------------------------------------------------
// 생성자
// ------------------------------------------------------------------
ACPlayerController::ACPlayerController()
{
    // 여기선 아무 것도 하지 않아도 됩니다.
    // 커스텀 입력 컴포넌트 생성은 CreateInputComponent()에서 수행합니다.
}

// ------------------------------------------------------------------
// 커스텀 입력 컴포넌트 생성
// ------------------------------------------------------------------
void ACPlayerController::CreateInputComponent()
{
    // UCEnhancedInputComponent를 직접 생성하여 컨트롤러에 장착
    CEnhancedInputComponent = NewObject<UCEnhancedInputComponent>(this, UCEnhancedInputComponent::StaticClass(), TEXT("PCInputComponent"));
    InputComponent = CEnhancedInputComponent;
    InputComponent->RegisterComponent();

    // 컨트롤러의 입력 스택에 Push
    PushInputComponent(InputComponent);
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

    // Enhanced Input MappingContext 등록
    if (ULocalPlayer* LP = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsys =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
        {
            if (bClearPreviousMappings)
                Subsys->ClearAllMappings();

            if (DefaultMappingContext)
            {
                // 우선순위 0
                Subsys->AddMappingContext(DefaultMappingContext, 0);
            }
        }
    }

    // UI 생성은 로컬 컨트롤러 + 게임플레이 맵에서만
    if (IsLocalController() && ShouldCreatePlayerWidget())
    {
        CreatePlayerWidget();
    }
}

// ------------------------------------------------------------------
// SetupInputComponent
// ------------------------------------------------------------------
void ACPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (!CEnhancedInputComponent)
    {
        CLog::Log(TEXT("[PC] UCEnhancedInputComponent가 없습니다(생성 실패)."));
        return;
    }
    if (!InputConfig)
    {
        CLog::Log(TEXT("[PC] InputConfig(UCInputConfig)가 설정되지 않았습니다."));
        return;
    }

    SetupInputBindings();
}

// ------------------------------------------------------------------
// Pawn 소유 시작/해제
// ------------------------------------------------------------------
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
