#include "CPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"

// ─ 프로젝트 컴포넌트/유틸
#include "CPlayerController.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "00_Character/CMainGameModeBase.h"
#include "00_Character/02_Component/00_PlayerComponent/CPlayerDashComponent.h"
#include "00_Character/02_Component/00_PlayerComponent/CPlayerWeaponComponent.h"
#include "00_Character/02_Component/00_PlayerComponent/CPlayerHealthComponent.h"
#include "00_Character/02_Component/00_PlayerComponent/CPlayerMovementBuffComponent.h"
#include "00_Character/02_Component/00_PlayerComponent/CEnhancedInputComponent.h"
#include "00_Character/02_Component/00_PlayerComponent/CGameplayTags.h"
#include "00_Character/02_Component/00_PlayerComponent/CUltimateBuffComponent.h"
#include "00_Character/02_Component/00_PlayerComponent/CFuryGaugeComponent.h"
#include "01_Widget/CPlayerWidget.h"
#include "04_Skill/CSkill_CommandLaunchSlam.h"
#include "04_Skill/CSkill_SpinAttack.h"
#include "00_Character/01_Enemy/CTankerBrute.h"
#include "00_Character/02_Component/01_EnemyComponent/CTankerChargeComponent.h"

#include "99_Util/CLog.h"

// ────────────────────────────────────────────────────────────────────────────
// 생성자
// ────────────────────────────────────────────────────────────────────────────
ACPlayerCharacter::ACPlayerCharacter()
{
    // 캡슐
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

    // 서브오브젝트
    DashComponent = CreateDefaultSubobject<UCPlayerDashComponent>(TEXT("DashComponent"));
    WeaponComponent = CreateDefaultSubobject<UCPlayerWeaponComponent>(TEXT("WeaponComponent"));
    HealthComponent = CreateDefaultSubobject<UCPlayerHealthComponent>(TEXT("HealthComponent"));
    MovementBuffComponent = CreateDefaultSubobject<UCPlayerMovementBuffComponent>(TEXT("MovementBuff"));
    UltimateBuffComponent = CreateDefaultSubobject<UCUltimateBuffComponent>(TEXT("UltimateBuffComponent"));
    FuryGaugeComponent = CreateDefaultSubobject<UCFuryGaugeComponent>(TEXT("FuryComponent"));
    SpinAttackComponent = CreateDefaultSubobject<UCSkill_SpinAttack>(TEXT("SkillSpinAttack"));
    CommandLaunchSlamComponent = CreateDefaultSubobject<UCSkill_CommandLaunchSlam>(TEXT("CommandLaunchSlam"));
    
    check(DashComponent);
    check(WeaponComponent);
    check(HealthComponent);
    check(MovementBuffComponent);
    check(UltimateBuffComponent);
    check(FuryGaugeComponent);
    check(SpinAttackComponent);
    check(CommandLaunchSlamComponent);

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 400.f;
    SpringArm->bUsePawnControlRotation = true;
    SpringArm->bDoCollisionTest = true;
    
    PlayerCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    PlayerCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    PlayerCamera->bUsePawnControlRotation = false;

    //TransparentCameraComponent = CreateDefaultSubobject<UTransparentCameraComponent>(TEXT("TransparentCamera"));
    //TransparentCameraComponent->SetupAttachment(RootComponent);
    
    
    // 이동(3인칭 기본값)
    // 카메라 관성(Lag) 설정
    SpringArm->bEnableCameraLag = true;              // 위치 관성 활성화
    SpringArm->CameraLagSpeed = 5.f;                // 관성 속도 (낮을수록 느림, 보통 3~15)
    SpringArm->CameraLagMaxDistance = 200.f;          // 최대 지연 거리
    
    
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
        Move->MaxStepHeight = FMath::Max(60.f, Move->MaxStepHeight);
        Move->bCanWalkOffLedges = true;
    }
}

// ────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ────────────────────────────────────────────────────────────────────────────
void ACPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (MovementBuffComponent)
        MovementBuffComponent->SetBaseMaxWalkSpeed(WalkingSpeed);
    if (CommandLaunchSlamComponent)
        CommandLaunchSlamComponent->OnAirCommandLockChanged.AddDynamic(
            this, &ACPlayerCharacter::HandleCommandMovementLockChanged);
    
    // 체력 이벤트 → HP UI 갱신
    if (ensureMsgf(HealthComponent != nullptr, TEXT("HealthComponent missing")))
    {
        HealthComponent->OnHealthChanged.AddDynamic(this, &ACPlayerCharacter::HandleHealthChanged);
        HealthComponent->OnDeath.AddDynamic(this, &ACPlayerCharacter::HandleDeath);

        HealthComponent->OnOverHealChanged.AddDynamic(this, &ACPlayerCharacter::HandleOverHealChanged);
    }
    
    // 탱커 돌진 델리게이트 바인딩
    if (UWorld* World = GetWorld())
    {
        TArray<AActor*> FoundTankers;
        UGameplayStatics::GetAllActorsOfClass(World, ACTankerBrute::StaticClass(), FoundTankers);
        
        for (AActor* Actor : FoundTankers)
        {
            if (ACTankerBrute* Tanker = Cast<ACTankerBrute>(Actor))
            {
                if (UCTankerChargeComponent* ChargeComp = Tanker->FindComponentByClass<UCTankerChargeComponent>())
                {
                    ChargeComp->OnPlayerHitByCharge.AddDynamic(this, &ACPlayerCharacter::OnHitByTankerCharge);
                }
            }
        }
    }
    
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
            ResetDashCooldown();
            
            if (FuryGaugeComponent)
                    FuryGaugeComponent->OnStacksChanged.AddDynamic(PlayerWidget, &UCPlayerWidget::UpdateFuryStacks);
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
    checkf(UltimateBuffComponent != nullptr, TEXT("UltimateBuffComponent missing"));

    checkf(FuryGaugeComponent != nullptr, TEXT("FuryGauge missing"));
    checkf(SpinAttackComponent != nullptr, TEXT("SkillSpinAttack missing"));
    
    // TransparentCameraComponent 설정 개선
 /*  if (TransparentCameraComponent)
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
    }*/
}

// ────────────────────────────────────────────────────────────────────────────
// 입력 바인딩(Enhanced Input + 태그)
// ────────────────────────────────────────────────────────────────────────────
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
    EIC->BindActionByTag(InputConfig, CGameplayTags::InputTag_Ultimate, ETriggerEvent::Started, this, &ACPlayerCharacter::UseUltimate);
    EIC->BindActionByTag(InputConfig, CGameplayTags::InputTag_Spin, ETriggerEvent::Started, this, &ACPlayerCharacter::OnSpinPressed);
    EIC->BindActionByTag(InputConfig, CGameplayTags::InputTag_Spin, ETriggerEvent::Completed, this, &ACPlayerCharacter::OnSpinReleased);
    EIC->BindActionByTag(InputConfig, CGameplayTags::InputTag_Command, ETriggerEvent::Started, this, &ACPlayerCharacter::OnCommandPressed);
}

void ACPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(WeaponComponent))
    {
        if (ACWeaponBase* Weapon = WeaponComponent->GetCurrentWeapon())
        {
            if (IsValid(Weapon))
            {
                Weapon->Destroy();
            }
        }
    }

    CleanupUltVFX();
    
    Super::EndPlay(EndPlayReason);
}

// ────────────────────────────────────────────────────────────────────────────
// 이동/시야
// ────────────────────────────────────────────────────────────────────────────
void ACPlayerCharacter::Move(const FInputActionValue& Value)
{
    if (CommandLaunchSlamComponent && CommandLaunchSlamComponent->ShouldBlockOtherActions())
    {
        HandleCommandMovementLockChanged(true);
        return;
    }
    
    if (bCommandMovementLocked)
    {
        HandleCommandMovementLockChanged(false);
    }
    
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

// ────────────────────────────────────────────────────────────────────────────
// 공격
// ────────────────────────────────────────────────────────────────────────────
void ACPlayerCharacter::Attack()
{
    if (SpinAttackComponent && SpinAttackComponent->IsSkillActive())
        return;
    
    if (CommandLaunchSlamComponent && CommandLaunchSlamComponent->ShouldBlockOtherActions())
        return;
    
    if (IsValid(WeaponComponent))
        WeaponComponent->DoAttack();
    else
        CLog::Log(TEXT("WeaponComponent missing"));
}

// 무기/애님에서 공격 시작 시점에 호출(있으면 더 견고)
void ACPlayerCharacter::OnAttackStarted()
{
    ApplyAttackMovementOverride(true);
    bDashLocked = true;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC && GetWorld())
    {
        PC = GetWorld()->GetFirstPlayerController();
    }
  
    if (PC && AttackCameraShakeClass)
    {
        if (APlayerCameraManager* CamManager = PC->PlayerCameraManager)
        {
            CamManager->StartCameraShake(AttackCameraShakeClass, AttackCameraShakeScale);
        }
    }
 
    if (!bAttackSlowActive)
    {
        SetAttackMovementSlowMultiplier(DefaultAttackMoveSpeedMultiplier);
    }
}

// 무기/애님에서 공격 완전 종료 시 호출(정보용 – 실동작은 DashReady에서 처리)
void ACPlayerCharacter::OnAttackEnded()
{
    ApplyAttackMovementOverride(false);
    bDashLocked = false;
    ResetAttackMovementSlowMultiplier();
    ConsumeDashBufferIfValid(true);   // 노티 미스 대비 2차 소비 시도
}

// 애님 노티(마지막 몇 프레임): 여기서 버퍼를 “같은 프레임에” 소모 → 즉발 체감
void ACPlayerCharacter::OnAttackDashReady()
{
    bDashLocked = false;
    ConsumeDashBufferIfValid(false);  // 같은 프레임 즉발
}



// ────────────────────────────────────────────────────────────────────────────
// 대시 (입력 및 버퍼/락)
// ────────────────────────────────────────────────────────────────────────────
void ACPlayerCharacter::DashStart()
{
    if (SpinAttackComponent && SpinAttackComponent->IsSkillActive())
        SpinAttackComponent->StopSpin();
    
    if (CommandLaunchSlamComponent && CommandLaunchSlamComponent->ShouldBlockOtherActions())
        return;
    
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

// ─ 대쉬 UI
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

// ────────────────────────────────────────────────────────────────────────────
// HP
// ────────────────────────────────────────────────────────────────────────────
// ─ UI 연동
void ACPlayerCharacter::HandleHealthChanged(float CurrentHealth, float MaxHealth)
{
    if (PlayerWidget)
        PlayerWidget->UpdateHpBar(CurrentHealth, MaxHealth);
}

void ACPlayerCharacter::HandleDeath(AActor* DeadActor)
{
    if (bIsDead) return;

    bIsDead = true;
    CLog::Log(TEXT("[Player] Death processing started"));

    if (SpinAttackComponent && SpinAttackComponent->IsSkillActive())
        SpinAttackComponent->StopSpin();
    
    // 입력 차단
    ACPlayerController* Pc = Cast<ACPlayerController>(GetController());
    if (Pc)
        DisableInput(Pc);

    // 이동 차단
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->DisableMovement();
        MoveComp->StopMovementImmediately();
    }

    // 타이머 정리
    GetWorldTimerManager().ClearAllTimersForObject(this);

    CleanupUltVFX();
    
    // 죽을 때 사용할 애니메이션 재생
    if (DeathPlayerMontage && DeathHammerMontage)
    {
        if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
        {
            AnimInstance->Montage_Play(DeathPlayerMontage);
        }

        if (ACHammer* Hammer = WeaponComponent->GetHammer())
        {
            if (UAnimInstance* HammerAnimInst = Hammer->GetWeaponMesh()->GetAnimInstance())
            {
                HammerAnimInst->Montage_Play(DeathHammerMontage);
            }
        }
    }

    // UI가 존재 한다면 여기에 작성하기
    if (PlayerWidget)
    {
        // 게임 오버 UI 및 기존 UI 숨기기
    }

    // 나중에 UI가 생기면 이 코드는 삭제하거나 UI에서 버튼을 누르면 이 함수 호출하기
    if (UWorld* World = GetWorld())
    {
        if (ACMainGameModeBase* GameMode = Cast<ACMainGameModeBase>(World->GetAuthGameMode()))
            GameMode->OnPlayerDeath(Pc);
    }
}

void ACPlayerCharacter::HandleOverHealChanged(float CurrentOverHeal, float MaxOverHeal)
{
    if (PlayerWidget)
    {
        PlayerWidget->UpdateOverHealHPBar(CurrentOverHeal, MaxOverHeal);
    }
}

void ACPlayerCharacter::KnockBackTankerDash()
{
    if (KnockbackMontage)
    {
        if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
        {
            AnimInstance->Montage_Play(DeathPlayerMontage);
        }
    }
}

void ACPlayerCharacter::OnHitByTankerCharge(AActor* HitPlayer, FVector KnockbackDirection, float KnockbackStrength)
{
    // 자기 자신이 맞은 경우만 처리
    if (HitPlayer != this)
    {
        return;
    }
    
    // 넉백 몽타주 재생
    if (KnockbackMontage)
    {
        if (USkeletalMeshComponent* MeshComp = GetMesh())
        {
            if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
            {
                AnimInstance->Montage_Play(KnockbackMontage, 1.0f);
            }
        }
    }
}

void ACPlayerCharacter::UpdateHpUI() const
{
    if (PlayerWidget && HealthComponent)
        PlayerWidget->UpdateHpBar(HealthComponent->GetHealth(), HealthComponent->GetMaxHealth());
}

float ACPlayerCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
    class AController* EventInstigator, AActor* DamageCauser)
{
    const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    

    if (AppliedDamage > 0.0f && HealthComponent)
    {
        HealthComponent->Damage(AppliedDamage);
    }

    if (bUltActive == false)
    {
        CLog::Log(FString::Printf(TEXT("[PlayerCharacter] Took %.1f Damage from %s"), AppliedDamage, *GetNameSafe(DamageCauser)));
    }
    
    return AppliedDamage;
}

// ────────────────────────────────────────────────────────────────────────────
// 궁극기
// ────────────────────────────────────────────────────────────────────────────
void ACPlayerCharacter::UseUltimate()
{
    if (bUltActive || !UltimateBuffComponent || CurUltGauge < MaxUltGauge)
        return;

    bUltActive = true;
    UltimateBuffComponent->ActivateUltimate();
    UE_LOG(LogTemp, Log, TEXT("[ULT] UseUltimate On Gauge=%.1f/%.1f"), CurUltGauge, MaxUltGauge);

    GetWorldTimerManager().ClearTimer(TimerHandle_UltDuration);
    GetWorldTimerManager().SetTimer(
        TimerHandle_UltDuration,
        this, &ACPlayerCharacter::OnUltimateExpired,
        UltDuration, false);

    GetWorldTimerManager().ClearTimer(TimerHandle_UltUITick);
    GetWorldTimerManager().SetTimer(
        TimerHandle_UltUITick,
        this, &ACPlayerCharacter::TickUltimateUI,
        0.05f, true);

    SpawnUltVFXOnHammer();
    
    TickUltimateUI();
    UpdateHpUI();
}

// ─ 궁극기 사용 종료
void ACPlayerCharacter::OnUltimateExpired()
{
    GetWorldTimerManager().ClearTimer(TimerHandle_UltUITick);

    bUltActive = false;
    CurUltGauge = 0.0f;
    if (UltimateBuffComponent)
        UltimateBuffComponent->DeactivateUltimate();

    CleanupUltVFX();
    
    CLog::Log(TEXT("[ULT] UseUltimate OFF"));

    UpdateUltimateUI();
    UpdateHpUI();
}

void ACPlayerCharacter::AddUltimateGain(float Gain)
{
    if (bUltActive || CurUltGauge >= MaxUltGauge)
        return;

    CurUltGauge = FMath::Clamp(CurUltGauge + Gain, 0.0f, MaxUltGauge);
    UE_LOG(LogTemp, Log, TEXT("[ULT][Gain] +%.2f -> %.2f/%.2f"), Gain, CurUltGauge, MaxUltGauge);

    UpdateUltimateUI();
}

// ─ 궁극기 UI 업데이트
void ACPlayerCharacter::UpdateUltimateUI()
{
    if (PlayerWidget)
        PlayerWidget->UpdateUltimateBar(CurUltGauge, MaxUltGauge);
}

void ACPlayerCharacter::TickUltimateUI()
{
    const float RemainingTime = GetWorldTimerManager().GetTimerRemaining(TimerHandle_UltDuration);
    
    if (UltDuration > KINDA_SMALL_NUMBER)
        CurUltGauge = MaxUltGauge * (RemainingTime / UltDuration);
    else
        CurUltGauge = 0.0f;
    
    CurUltGauge = FMath::Max(0.0f, CurUltGauge);
    
    UpdateUltimateUI();
}

void ACPlayerCharacter::CleanupUltVFX()
{
    if (IsValid(HammerUltFXComp))
    {
        HammerUltFXComp->Deactivate();
        HammerUltFXComp->DestroyComponent();
    }
    HammerUltFXComp = nullptr;
}

void ACPlayerCharacter::SpawnUltVFXOnHammer()
{
    if (!HammerUltFX || !WeaponComponent)
        return;

    CleanupUltVFX();
    
    if (ACHammer* Hammer = WeaponComponent->GetHammer())
    {
        if (USkeletalMeshComponent* WeaponMesh = Hammer->GetWeaponMesh())
        {
            HammerUltFXComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
                HammerUltFX,
                WeaponMesh,
                HammerUltSocketName,
                FVector::ZeroVector,
                FRotator::ZeroRotator,
                EAttachLocation::SnapToTargetIncludingScale,
                false);

            if (HammerUltFXComp)
                HammerUltFXComp->Activate(true);
        }
    }
}

void ACPlayerCharacter::SetUltimateGauge(float UltGauge)
{
    CurUltGauge = FMath::Clamp(UltGauge, 0.0f, MaxUltGauge);

    UpdateUltimateUI();
}

// ────────────────────────────────────────────────────────────────────────────
// 차징 (스핀) 스킬
// ────────────────────────────────────────────────────────────────────────────
void ACPlayerCharacter::OnSpinPressed()
{
    if (CommandLaunchSlamComponent && CommandLaunchSlamComponent->ShouldBlockOtherActions())
        return;
    
    if (SpinAttackComponent)
        SpinAttackComponent->TryStartSpin();
}

void ACPlayerCharacter::OnSpinReleased()
{
    if (SpinAttackComponent)
        SpinAttackComponent->StopSpin();
}


// ────────────────────────────────────────────────────────────────────────────
// 커맨드 공격
// ────────────────────────────────────────────────────────────────────────────
void ACPlayerCharacter::OnCommandPressed()
{
    if (SpinAttackComponent && SpinAttackComponent->IsSkillActive())
        return;
    
    if (CommandLaunchSlamComponent)
    {
        if (CommandLaunchSlamComponent->IsAirCommandActive())
        {
            CommandLaunchSlamComponent->TryConfirmSlam();
        }
        else
        {
            if (CommandLaunchSlamComponent->ShouldBlockOtherActions())
                return;
            
            if (CommandLaunchSlamComponent->TryStartCommand())
                HandleCommandMovementLockChanged(true);
        }
    }
}

void ACPlayerCharacter::HandleCommandMovementLockChanged(bool bLocked)
{
    if (bCommandMovementLocked == bLocked)
        return;
    
    bCommandMovementLocked = bLocked;
    
    if (bCommandMovementLocked)
    {
        if (UCharacterMovementComponent* Move = GetCharacterMovement())
            Move->StopMovementImmediately();
    }
}

void ACPlayerCharacter::ApplyAttackMovementOverride(bool bEnable)
{

    /*
     *이동 애님을 8방향으로 갈꺼면 살려야 함. (지금은 이동이 단방향이라서 비활성화)
     */
    
    /*if (bEnable)
    {
        if (bAttackMovementOverrideActive)
            return;
        
        bAttackMovementOverrideActive = true;
            
        bUseControllerRotationYaw = true;
            
        if (Controller)
        {
            const FRotator ControlRot = Controller->GetControlRotation();
            const FRotator TargetYaw(0.f, ControlRot.Yaw, 0.f);
            SetActorRotation(TargetYaw);
        }
        
        if (UCharacterMovementComponent* Move = GetCharacterMovement())
        {
            Move->bOrientRotationToMovement = false;
            Move->bUseControllerDesiredRotation = true;
            if (Move->MovementMode == MOVE_None)
                Move->SetMovementMode(MOVE_Walking);
        }
    }
    else
    {
        if (!bAttackMovementOverrideActive)
            return;
        
        bAttackMovementOverrideActive = false;
        
        bUseControllerRotationYaw = false;
            
        if (UCharacterMovementComponent* Move = GetCharacterMovement())
        {
            Move->bOrientRotationToMovement = true;
            Move->bUseControllerDesiredRotation = false;
        }
    }*/
}

void ACPlayerCharacter::SetAttackMovementSlowMultiplier(float Multiplier)
{
      const float ClampedMultiplier = FMath::Clamp(Multiplier, 0.f, 1.f);
   
       CurrentAttackSlowMultiplier = ClampedMultiplier;
        bAttackSlowActive = ClampedMultiplier < 1.f - KINDA_SMALL_NUMBER;
   
       if (MovementBuffComponent)
           {
                  MovementBuffComponent->SetAdditionalMultiplier(ClampedMultiplier);
               }
  }

void ACPlayerCharacter::ResetAttackMovementSlowMultiplier()
{
    if (!bAttackSlowActive && FMath::IsNearlyEqual(CurrentAttackSlowMultiplier, 1.f))
        return;
    
    CurrentAttackSlowMultiplier = 1.f;
    bAttackSlowActive = false;

    if (MovementBuffComponent)
    {
        MovementBuffComponent->SetAdditionalMultiplier(1.f);
    }
}

// ────────────────────────────────────────────────────────────────────────────
// IBuffable
// ────────────────────────────────────────────────────────────────────────────
float ACPlayerCharacter::GetOutgoingDamageMultiplier() const
{
    if (bUltActive && UltimateBuffComponent)
        return UltimateBuffComponent->GetOutgoingDamageMultiplier();
    
    return 1.0f;
}

float ACPlayerCharacter::GetIncomingDamageScale() const
{
    if (bUltActive && UltimateBuffComponent)
        return UltimateBuffComponent->GetIncomingDamageScale();
    
    return 1.0f;
}

bool ACPlayerCharacter::IsBuffActive() const
{
    return bUltActive;
}