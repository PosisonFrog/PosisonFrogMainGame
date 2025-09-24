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
#include "00_Character/00_Player/03_Camera/TransparentCameraComponent.h"

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
    
    SpringArm->bEnableCameraLag = true;       
    SpringArm->CameraLagSpeed = 7.0f;
    SpringArm->bEnableCameraRotationLag = false;

    SpringArm->bDoCollisionTest = false;
    
    PlayerCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    check(PlayerCamera);
    PlayerCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    PlayerCamera->bUsePawnControlRotation = false;

    TransparentCameraComponent = CreateDefaultSubobject<UTransparentCameraComponent>(TEXT("TransparentCamera"));
    check(TransparentCameraComponent);
    
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

    if (TransparentCameraComponent && PlayerCamera)
    {
        TransparentCameraComponent->SetCameraComponent(PlayerCamera);
    }
    
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
    // 컨트롤러에서 바인딩되는 대시 입력 진입점
    RequestDash();
}

// ───────── 대시 버퍼/락 핵심 로직 ─────────

void ACPlayerCharacter::RequestDash()
{
    const float Now = GetWorld()->GetTimeSeconds();

    // 공격 중이면 실행하지 않고 버퍼에 저장
    if (bDashLocked)
    {
        bDashBuffered    = true;
        DashBufferExpire = Now + DashBufferWindow;
        return;
    }

    // 쿨타임 중이면 무시
    if (bDashOnCooldown)
    {
        CLog::Print(TEXT("Dash on cooldown"), -1, 0.6f, FColor::Cyan);
        return;
    }
    
    // 잠금이 아니면 즉시 대시
    if (DashComponent)
    {
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
    }
}

// 애님 노티(마지막 몇 프레임): 여기서 버퍼를 “같은 프레임에” 소모 → 즉발 체감
void ACPlayerCharacter::OnAttackDashReady()
{
    const float Now = GetWorld()->GetTimeSeconds();

    // 공격 종료 직전이므로 락 해제
    bDashLocked = false;

    // 버퍼가 살아 있으면 그 프레임에 즉시 대시
    if (bDashBuffered && Now <= DashBufferExpire)
    {
        bDashBuffered = false;

        // 혹시 공격 중 제어 잠금/루트모션 영향이 남아있다면 정리
        if (UCharacterMovementComponent* Move = GetCharacterMovement())
        {
            if (Move->MovementMode == MOVE_None)
                Move->SetMovementMode(MOVE_Walking);
            // 필요 시: Move->StopMovementImmediately();
        }

        if (DashComponent)
        {
            DashComponent->StartDash();
        }
    }
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
    // 버퍼를 여기서 바로 소모하지 않는 이유:
    //  → BlendOut이 약간 남아 있어도 마지막 프레임(DashReady)에서 터뜨리는 게 체감이 가장 좋기 때문
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
        PlayerWidget->SetUltimatePoints(UltimateCurrentPoints, UltimateMaxPoints);
}

void ACPlayerCharacter::CalculateUltimatePoint(float AttackDamage)
{
    if (UltimateStack >= UltimateMaxStacks && UltimateCurrentPoints >= UltimateMaxPoints)
        return;
    
    float Total = UltimateCurrentPoints + AttackDamage * 2;
    
    int32 NewStack = FMath::FloorToInt(Total / UltimateMaxPoints);
    float Remainder = FMath::Fmod(Total, UltimateMaxPoints);

    UltimateStack += NewStack;

    CLog::Log(UltimateCurrentPoints);
    CLog::Log(UltimateMaxPoints);
    CLog::Log(UltimateStack);
    CLog::Log(UltimateMaxStacks);
    
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

