#include "CPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

#include "00_Character/02_Component/CEnhancedInputComponent.h"
#include "00_Character/02_Component/CGameplayTags.h"
#include "00_Character/02_Component/CWeaponComponent.h"
#include "00_Character/02_Component/CDashComponent.h"
#include "00_Character/02_Component/CHealthComponent.h"
#include "00_Character/02_Component/CMovementBuffComponent.h"

#include "01_Widget/CPlayerWidget.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Global.h" // CLog 등

ACPlayerCharacter::ACPlayerCharacter()
{
    // 캡슐
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

    // ─ Components ─
    DashComponent = CreateDefaultSubobject<UCDashComponent>(TEXT("DashComponent"));
    check(DashComponent);

    WeaponComponent = CreateDefaultSubobject<UCWeaponComponent>(TEXT("WeaponComponent"));
    check(WeaponComponent);

    HealthComponent = CreateDefaultSubobject<UCHealthComponent>(TEXT("HealthComponent"));
    check(HealthComponent);

    MovementBuffComponent = CreateDefaultSubobject<UCMovementBuffComponent>(TEXT("MovementBuff"));
    check(MovementBuffComponent);

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    check(SpringArm);
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 400.f;
    SpringArm->bUsePawnControlRotation = true;

    PlayerCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    check(PlayerCamera);
    PlayerCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    PlayerCamera->bUsePawnControlRotation = false;

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

void ACPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->MaxWalkSpeed = WalkingSpeed;
    }

    // 버프 기준 속도 동기화
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

    if (WeaponComponent)
    {
        WeaponComponent->OnWeaponHit.AddDynamic(this, &ACPlayerCharacter::AddUltimatePoint);
    }
}

void ACPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UCEnhancedInputComponent* EIC = Cast<UCEnhancedInputComponent>(PlayerInputComponent);
    checkf(EIC, TEXT("UCEnhancedInputComponent required"));

    EIC->BindActionByTag(InputConfig, CGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ACPlayerCharacter::Move);
    EIC->BindActionByTag(InputConfig, CGameplayTags::InputTag_Look, ETriggerEvent::Triggered, this, &ACPlayerCharacter::Look);
    EIC->BindActionByTag(InputConfig, CGameplayTags::InputTag_Dash, ETriggerEvent::Started, this, &ACPlayerCharacter::DashStart);
    EIC->BindActionByTag(InputConfig, CGameplayTags::InputTag_Attack, ETriggerEvent::Started, this, &ACPlayerCharacter::Attack);
}

// ─ Input Handlers ─
void ACPlayerCharacter::Move(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    if (!Controller) return;

    const FRotator ControlRot = Controller->GetControlRotation();
    const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);

    const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
    const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

    AddMovementInput(Forward, Axis.Y);
    AddMovementInput(Right, Axis.X);
}

void ACPlayerCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>(); 
    if (!Controller) return;

    AddControllerYawInput(Axis.X);
    AddControllerPitchInput(Axis.Y);
}

void ACPlayerCharacter::DashStart()
{
    if (!IsValid(DashComponent))
    {
        CLog::Log(TEXT("DashComponent missing"));
        return;
    }

    // 쿨타임 중이면 무시
    if (bDashOnCooldown)
    {
        CLog::Print(TEXT("Dash on cooldown"), -1, 0.6f, FColor::Cyan);
        return;
    }

    // (1) 대시 실행
    DashComponent->StartDash();

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
}

void ACPlayerCharacter::Attack()
{
    if (IsValid(WeaponComponent))
        WeaponComponent->DoAttack();
    else
        CLog::Log(TEXT("WeaponComponent missing"));
}

// ─ Health / UI ─
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

// ─ Dash Cooldown Helpers ─
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

void ACPlayerCharacter::AddUltimatePoint(AActor* HitActor, float Damage)
{
    CalculateUltimatePoint(Damage);

    
    if (PlayerWidget)
        PlayerWidget->SetUltimatePoints(UltimateCurrentPoints, UltimateMaxPoints, UltimateStack);
}

void ACPlayerCharacter::CalculateUltimatePoint(float AttackDamage)
{
    if (UltimateStack >= UltimateMaxStacks && UltimateCurrentPoints >= UltimateMaxPoints)
        return;
    
    float Total = UltimateCurrentPoints + AttackDamage * 2;
    
    int32 NewStack = FMath::FloorToInt(Total / UltimateMaxPoints);
    float Remainder = FMath::Fmod(Total, UltimateMaxPoints);

    UltimateStack += NewStack;

    // 최대 스택 초과 방지
    if (UltimateStack >= UltimateMaxStacks)
    {
        UltimateStack = UltimateMaxStacks;
        UltimateCurrentPoints = UltimateMaxPoints;
    }
    else
    {
        UltimateCurrentPoints = Remainder;
    }
}

void ACPlayerCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    
    checkf(DashComponent != nullptr, TEXT("DashComponent missing"));
    checkf(WeaponComponent != nullptr, TEXT("WeaponComponent missing"));
    checkf(HealthComponent != nullptr, TEXT("HealthComponent missing"));
    checkf(MovementBuffComponent != nullptr, TEXT("MovementBuffComponent missing"));
}
