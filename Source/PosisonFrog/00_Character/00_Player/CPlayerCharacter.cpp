#include "CPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "InputActionValue.h"

// 프로젝트 컴포넌트/유틸
#include "00_Character/02_Component/CDashComponent.h"
#include "00_Character/02_Component/CWeaponComponent.h"            // 파일명이 CWeaponComponent라면 헤더명을 맞춰주세요 -> 까먹어버렸지 몹니까
#include "00_Character/02_Component/CHealthComponent.h"
#include "00_Character/02_Component/CMovementBuffComponent.h"
#include "00_Character/02_Component/CEnhancedInputComponent.h"
#include "00_Character/02_Component/CGameplayTags.h"
#include "00_Character/00_Player/03_Camera/TransparentCameraComponent.h"

#include "01_Widget/CPlayerWidget.h"
#include "99_Util/CLog.h"

// ----------------------------------------------------------------------------
// 생성자
// ----------------------------------------------------------------------------
ACPlayerCharacter::ACPlayerCharacter()
{
    // 캡슐
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

    // 서브오브젝트
    DashComponent = CreateDefaultSubobject<UCDashComponent>(TEXT("DashComponent"));
    WeaponComponent = CreateDefaultSubobject<UCWeaponComponent>(TEXT("WeaponComponent"));
    HealthComponent = CreateDefaultSubobject<UCHealthComponent>(TEXT("HealthComponent"));
    MovementBuffComponent = CreateDefaultSubobject<UCMovementBuffComponent>(TEXT("MovementBuff"));

    check(DashComponent);
    check(WeaponComponent);
    check(HealthComponent);
    check(MovementBuffComponent);


    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 400.f;
    SpringArm->bUsePawnControlRotation = true;
    SpringArm->bDoCollisionTest = false;
    
    PlayerCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    PlayerCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    PlayerCamera->bUsePawnControlRotation = false;

    TransparentCameraComponent = CreateDefaultSubobject<UTransparentCameraComponent>(TEXT("TransparentCamera"));
    TransparentCameraComponent->SetupAttachment(RootComponent);
    
    
    // 이동(3인칭 기본값)
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->bOrientRotationToMovement = true;
        Move->RotationRate = FRotator(0.f, 500.f, 0.f);
        Move->MinAnalogWalkSpeed = 20.f;
        Move->BrakingDecelerationWalking = 2000.f;
        Move->BrakingDecelerationFalling = 1500.f;
        Move->MaxWalkSpeed = WalkingSpeed; // 에디터에서 덮어씀
    }
}

// ----------------------------------------------------------------------------
// BeginPlay
// ----------------------------------------------------------------------------
void ACPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();


    if (UCharacterMovementComponent* Move = GetCharacterMovement())
        Move->MaxWalkSpeed = WalkingSpeed;

    if (MovementBuffComponent)
        MovementBuffComponent->SetBaseMaxWalkSpeed(WalkingSpeed);

    // 체력 이벤트 → HP UI 갱신
    if (ensureMsgf(HealthComponent != nullptr, TEXT("HealthComponent missing")))
        HealthComponent->OnHealthChanged.AddDynamic(this, &ACPlayerCharacter::HandleHealthChanged);

    // UI 생성
    if (PlayerWidgetClass)
    {
        APlayerController* PC = Cast<APlayerController>(GetController());
        if (!PC) PC = GetWorld()->GetFirstPlayerController();

        PlayerWidget = CreateWidget<UCPlayerWidget>(PC, PlayerWidgetClass);
        if (PlayerWidget)
        {
            PlayerWidget->AddToViewport();
            UpdateHpUI();
            PlayerWidget->SetDashReady(); // 시작 상태
        }
        else
        {
            CLog::Log(TEXT("PlayerWidget create failed"));
        }
    }
}

void ACPlayerCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    
    checkf(DashComponent != nullptr, TEXT("DashComponent missing"));
    checkf(WeaponComponent != nullptr, TEXT("WeaponComponent missing"));
    checkf(HealthComponent != nullptr, TEXT("HealthComponent missing"));
    checkf(MovementBuffComponent != nullptr, TEXT("MovementBuffComponent missing"));
    // TransparentCameraComponent 설정 개선
    if (TransparentCameraComponent)
    {
        // SpringArm 먼저 설정 (CalibrateIdleView 호출하므로)
        if (SpringArm)
            TransparentCameraComponent->SetSpringArmComponent(SpringArm);
        
        if (PlayerCamera)
            TransparentCameraComponent->SetCameraComponent(PlayerCamera);
        
        CLog::Log(TEXT("TransparentCameraComponent initialized successfully"));
    }
    else
    {
        CLog::Log(TEXT("TransparentCameraComponent missing"));
    }
}

// ----------------------------------------------------------------------------
// 입력 바인딩(Enhanced Input + 태그)
// ----------------------------------------------------------------------------
void ACPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // 프로젝트 전용 강화 입력 컴포넌트 사용
    UCEnhancedInputComponent* EIC = Cast<UCEnhancedInputComponent>(PlayerInputComponent);
    checkf(EIC, TEXT("UCEnhancedInputComponent required"));
    checkf(InputConfig, TEXT("InputConfig(UCInputConfig) not set"));

    EIC->BindActionByTag(InputConfig, CGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ACPlayerCharacter::Move);
    EIC->BindActionByTag(InputConfig, CGameplayTags::InputTag_Look, ETriggerEvent::Triggered, this, &ACPlayerCharacter::Look);
    EIC->BindActionByTag(InputConfig, CGameplayTags::InputTag_Dash, ETriggerEvent::Started, this, &ACPlayerCharacter::DashStart);
    EIC->BindActionByTag(InputConfig, CGameplayTags::InputTag_Attack, ETriggerEvent::Started, this, &ACPlayerCharacter::Attack);
}

// ----------------------------------------------------------------------------
// 이동/시야
// ----------------------------------------------------------------------------
void ACPlayerCharacter::Move(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    if (!Controller) return;

    const FRotator ControlRot = Controller->GetControlRotation();
    const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);

    const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
    const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

    if (!FMath::IsNearlyZero(Axis.Y)) AddMovementInput(Forward, Axis.Y);
    if (!FMath::IsNearlyZero(Axis.X)) AddMovementInput(Right, Axis.X);
}

void ACPlayerCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>(); 
    if (!Controller) return;

    AddControllerYawInput(Axis.X);
    AddControllerPitchInput(Axis.Y);
}

// ----------------------------------------------------------------------------
// 공격/대시 (입력 및 버퍼/락)
// ----------------------------------------------------------------------------
void ACPlayerCharacter::DashStart()
{
    // 컨트롤러에서 바인딩되는 대시 입력 진입점
    const float Now = GetWorld()->GetTimeSeconds();

    // 공격 중이면 입력을 버퍼
    if (bDashLocked)
    {
        bDashBuffered = true;
        DashBufferExpire = Now + DashBufferWindow;
        return;
    }

    // 즉시 시도
    TryCommitDash();
}


bool ACPlayerCharacter::TryCommitDash()
{
    //잠금이 아니고 대쉬 컴포넌트가 유효하면 대쉬 진행 아니면 false 반환
    if (bDashOnCooldown || !IsValid(DashComponent))
        return false;
    
    DashComponent->StartDash(); // 내부 쿨타임/중복 체크는 컴포넌트 쪽에서

    // (2) 6초 쿨타임 무조건 시작
    bDashOnCooldown = true;
    DashCooldownRemaining = DashCooldown;

    GetWorldTimerManager().ClearTimer(TimerHandle_DashCooldown);
    GetWorldTimerManager().SetTimer(
        TimerHandle_DashCooldown,
        this, &ACPlayerCharacter::ResetDashCooldown,
        DashCooldown, false);

    GetWorldTimerManager().ClearTimer(TimerHandle_DashUITick);
    GetWorldTimerManager().SetTimer(
        TimerHandle_DashUITick,
        this, &ACPlayerCharacter::TickDashCooldownUI,
        0.05f, true);

    TickDashCooldownUI(); // 즉시 1회 갱신

    // (3) 대시 후 이속 버프 2초(+15%)
    if (MovementBuffComponent)
        MovementBuffComponent->AddSpeedBuff(DashSpeedMultiplier, DashSpeedBuffDuration);

    if (PlayerWidget)
        PlayerWidget->PlayDashFX(DashSpeedBuffDuration);
    
    return true;
}

void ACPlayerCharacter::Attack()
{
    if (IsValid(WeaponComponent))
        WeaponComponent->DoAttack();
    else
        CLog::Log(TEXT("WeaponComponent missing"));
}

// 무기/애님에서 공격 시작 시점에 호출(있으면 더 견고)
void ACPlayerCharacter::OnAttackStarted()
{
    bDashLocked = true;
}

// 무기/애님에서 공격 완전 종료 시 호출(정보용 – 실동작은 DashReady에서 처리)
void ACPlayerCharacter::OnAttackEnded()
{
    bDashLocked = false;
    ConsumeDashBufferIfValid(true);   // 노티 미스 대비 2차 소비 시도

}

// 애님 노티(마지막 몇 프레임): 여기서 버퍼를 “같은 프레임에” 소모 → 즉발 체감
void ACPlayerCharacter::OnAttackDashReady()
{
    bDashLocked = false;
    ConsumeDashBufferIfValid(false);  // 같은 프레임 즉발
}

void ACPlayerCharacter::ConsumeDashBufferIfValid(bool bFallback)
{
    const float Now = GetWorld()->GetTimeSeconds();

    if (bDashBuffered && Now <= DashBufferExpire)
    {
        bDashBuffered = false;

        // 이동락/루트모션 잔여 방지
        if (UCharacterMovementComponent* Move = GetCharacterMovement())
        {
            if (Move->MovementMode == MOVE_None)
                Move->SetMovementMode(MOVE_Walking);
        }

        TryCommitDash();
        return;
    }

    // 프레임 드랍 등으로 노티 Begin을 놓친 경우 End에서 짧은 유예 허용
    if (bFallback && bDashBuffered)
    {
        if (Now <= DashBufferExpire + 0.04f) // ≈ 2~3프레임
        {
            bDashBuffered = false;
            TryCommitDash();
        }
        else
        {
            bDashBuffered = false; // 만료
        }
    }
}

// ----------------------------------------------------------------------------
// HP UI 연동
// ----------------------------------------------------------------------------
void ACPlayerCharacter::HandleHealthChanged(float CurrentHealth, float MaxHealth)
{
    if (PlayerWidget)
        PlayerWidget->UpdateHpBar(CurrentHealth, MaxHealth);
}

void ACPlayerCharacter::UpdateHpUI() const
{
    if (PlayerWidget && HealthComponent)
        PlayerWidget->UpdateHpBar(HealthComponent->GetHealth(), HealthComponent->GetMaxHealth());
}

// ----------------------------------------------------------------------------
// Dash 쿨다운 UI
// ----------------------------------------------------------------------------
void ACPlayerCharacter::ResetDashCooldown()
{
    bDashOnCooldown = false;
    DashCooldownRemaining = 0.f;

    GetWorldTimerManager().ClearTimer(TimerHandle_DashUITick);

    if (PlayerWidget)
        PlayerWidget->SetDashReady();
}

void ACPlayerCharacter::TickDashCooldownUI()
{
    DashCooldownRemaining = GetWorldTimerManager().GetTimerRemaining(TimerHandle_DashCooldown);
    DashCooldownRemaining = FMath::Max(0.f, DashCooldownRemaining);

    if (PlayerWidget)
        PlayerWidget->UpdateDashCooldown(DashCooldownRemaining, DashCooldown);

    if (DashCooldownRemaining <= KINDA_SMALL_NUMBER && bDashOnCooldown)
        ResetDashCooldown();
}



