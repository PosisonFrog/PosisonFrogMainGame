#include "CSkill_CommandLaunchSlam.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/DamageType.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

#include "Global.h"
#include "00_Character/01_Enemy/CEnemyCharacterBase.h"
#include "00_Character/02_Component/00_PlayerComponent/CPlayerWeaponComponent.h"
#include "00_Character/02_Component/CHitStopComponent.h"

UCSkill_CommandLaunchSlam::UCSkill_CommandLaunchSlam()
{
    PrimaryComponentTick.bCanEverTick = true;
    ShockwaveDamageType = UDamageType::StaticClass(); // Fury 스택 금지
    LaunchDamageType    = UDamageType::StaticClass();

    LaunchAllowTags = { TEXT("Enemy.Type.Normal"), TEXT("Enemy.Type.Ranged") };
    LaunchDenyTags  = { TEXT("Enemy.Type.Tank"),   TEXT("Enemy.Type.Boss") };

    HitStopComponent = CreateDefaultSubobject<UCHitStopComponent>(TEXT("HitStopComponent"));
}

void UCSkill_CommandLaunchSlam::BeginPlay()
{
    Super::BeginPlay();
    OwnerChar = Cast<ACharacter>(GetOwner());
    if (OwnerChar.IsValid())
        MoveComp = OwnerChar->GetCharacterMovement();
    
}

void UCSkill_CommandLaunchSlam::TickComponent(float DeltaTime, ELevelTick, FActorComponentTickFunction*)
{
    if (!OwnerChar.IsValid() || !MoveComp.IsValid())
    {
        if (State != ECommandAirState::Inactive)
        {
            AbortCommand(false);
        }
        return;
    }

    if (State == ECommandAirState::Launching)
    {
        const bool bHasLaunchMontage = LaunchCharMontage != nullptr;

        if (bHasLaunchMontage)
        {
            bool bMontagePlaying = false;
            if (USkeletalMeshComponent* Mesh = OwnerChar->GetMesh())
            {
                if (UAnimInstance* Anim = Mesh->GetAnimInstance())
                {
                    bMontagePlaying = Anim->Montage_IsPlaying(LaunchCharMontage);
                }
            }

            if (!bMontagePlaying)
            {
                AbortCommand(false);
                return;
            }

            if (bLaunchByNotify && LaunchStallTolerance > 0.f)
            {
                if (UWorld* World = GetWorld())
                {
                    if (LaunchStateEnterTime > 0.f && World->GetTimeSeconds() - LaunchStateEnterTime >= LaunchStallTolerance)
                    {
                        AbortCommand(false);
                        return;
                    }
                }
            }
        }
        else if (LaunchStallTolerance > 0.f)
        {
            if (UWorld* World = GetWorld())
            {
                if (LaunchStateEnterTime > 0.f && World->GetTimeSeconds() - LaunchStateEnterTime >= LaunchStallTolerance)
                {
                    AbortCommand(false);
                    return;
                }
            }
        }
    }

    if (State != ECommandAirState::Descending)
        return;
 
    // 착지 감지 (노티 미사용/보조)
    if (IsOnGroundNow())
    {
        if (!bImpactDone)
        {
            bImpactDone = true;
             
            if (bPendingSlam && !bImpactByNotify)
            {
                DoShockwaveImpact(); // 노티 대신 착지 순간에 임팩트
            }
            State = ECommandAirState::Inactive;
            bPendingSlam = false;
            LaunchStateEnterTime = 0.f;

            StartCooldown();

            if (bBlockOtherActionsWhileAir)
                OnAirCommandLockChanged.Broadcast(false);
        }
    }
}

bool UCSkill_CommandLaunchSlam::TryStartCommand()
{
    if (IsOnCooldown())
    {
        return false;
    }
    UCPlayerWeaponComponent* WeaponComp = OwnerChar->FindComponentByClass<UCPlayerWeaponComponent>();
    Hammer = WeaponComp ? WeaponComp->GetHammer() : nullptr;
    
    if (State != ECommandAirState::Inactive)
        return false;
    
    if (!OwnerChar.IsValid() || !MoveComp.IsValid() || !Hammer.IsValid())
        return false;

    if (!IsOnGroundNow())
        return false;
    
    // Launch 연동: 노티 우선 / 즉시
    if (!LaunchCharMontage && !LaunchHammerMontage)
        return false;

    State = ECommandAirState::Launching;

    
    if (UWorld* World = GetWorld())
        LaunchStateEnterTime = World->GetTimeSeconds();
    else
        LaunchStateEnterTime = 0.f;

    
    PlayCharMontageSafe(LaunchCharMontage, LaunchSection);
    PlayHammerMontageSafe(LaunchHammerMontage, LaunchSection);
    
    if (bLaunchByNotify)
    {
        // 노티가 실제 Launch와 EnterAirborneWaiting을 호출
        return true;
    }
    else
    {
        Anim_PerformLaunch(); // 즉시 실행
        return true;
    }
}

bool UCSkill_CommandLaunchSlam::TryConfirmSlam()
{
    UCPlayerWeaponComponent* WeaponComp = OwnerChar->FindComponentByClass<UCPlayerWeaponComponent>();
    Hammer = WeaponComp ? WeaponComp->GetHammer() : nullptr;
    
    if (!OwnerChar.IsValid() || !MoveComp.IsValid() || !Hammer.IsValid())
        return false;

    if (State != ECommandAirState::AirborneWaiting)
        return false;

    if (!bAwaitingSlamConfirm)
        return false;
    
    UWorld* World = GetWorld();
    if (!World)
        return false;
    
    if (World->GetTimeSeconds() < EarliestSlamConfirmTime)
        return false;

    // 1초 대기 타이머 종료
    GetWorld()->GetTimerManager().ClearTimer(TimerHandle_AirWindow);

    // 강제 하강(노티/착지 감지 중 어떤 경로든 임팩트 보장)
    ClearSlamWaiting();
    ForceDescend(/*bAsSlam=*/true);

    if (SlamCharMontage)
        PlayCharMontageSafe(SlamCharMontage, SlamSection);

    if (SlamHammerMontage)
        PlayHammerMontageSafe(SlamHammerMontage, SlamSection);

    return true;
}

void UCSkill_CommandLaunchSlam::Anim_PerformLaunch()
{
    if (!OwnerChar.IsValid() || !MoveComp.IsValid())
        return;

    if (State != ECommandAirState::Launching)
        return;

    // 플레이어 띄우기
    OwnerChar->LaunchCharacter(FVector(0,0,PlayerLaunchZ), true, true);

    // 주변 적(Launch 허용) 띄우기
    TArray<ACharacter*> Neighbors;
    
    CollectCharactersInRadius(Neighbors, LaunchRadius, /*bIncludeLaunchedIgnoringZ=*/true, /*bRiotOnly=*/true);
    
    AController* Inst = OwnerChar->GetController();
        const TSubclassOf<UDamageType> LaunchDamageClass = IsValid(LaunchDamageType)
            ? LaunchDamageType
            : TSubclassOf<UDamageType>(UDamageType::StaticClass());
    
    for (ACharacter* C : Neighbors)
    {
        if (!C || C == OwnerChar.Get()) continue;
        if (!C->IsA(ACEnemyCharacterBase::StaticClass())) continue;
        if (!IsLaunchableEnemy(C)) continue; // 탱커/보스 면역
        C->LaunchCharacter(FVector(0,0,EnemyLaunchZ), true, true);

      
        if (LaunchDamage > 0.f)
        {
            UGameplayStatics::ApplyDamage(
            C,
            LaunchDamage,
            Inst,
            OwnerChar.Get(),
            LaunchDamageClass);
        }
    }

    // 공중 대기 진입 (입력 대기 1초)
    EnterAirborneWaiting();
}

void UCSkill_CommandLaunchSlam::Anim_SlamImpact()
{
    // Slam 몽타주의 임팩트 프레임에서 호출
    if (!OwnerChar.IsValid()) return;

    if (bImpactDone) return; // 중복 방지

    // 임팩트 강제 처리 (지면 여부와 무관하게 연출 우선)
    DoShockwaveImpact();
    bImpactDone = true;

    State = ECommandAirState::Inactive;
    bPendingSlam = false;
    LaunchStateEnterTime = 0.0f;
    
    StartCooldown();
    
    if (bBlockOtherActionsWhileAir)
        OnAirCommandLockChanged.Broadcast(false);
}


void UCSkill_CommandLaunchSlam::EnterAirborneWaiting()
{
    State        = ECommandAirState::AirborneWaiting;
    bPendingSlam = false;
    bImpactDone  = false;
    LaunchStateEnterTime = 0.0f;

    StartSlamConfirmDelay();
    
    if (bBlockOtherActionsWhileAir)
        OnAirCommandLockChanged.Broadcast(true);

    GetWorld()->GetTimerManager().ClearTimer(TimerHandle_AirWindow);
    GetWorld()->GetTimerManager().SetTimer(
        TimerHandle_AirWindow, this, &UCSkill_CommandLaunchSlam::OnAirWindowExpired,
        AirCommandWindow, false);
}

void UCSkill_CommandLaunchSlam::OnAirWindowExpired()
{
    if (State != ECommandAirState::AirborneWaiting)
        return;

    ForceDescend(/*bAsSlam=*/false); // 자연하강 (충격파 없음)
}

void UCSkill_CommandLaunchSlam::ForceDescend(bool bAsSlam)
{
    if (!OwnerChar.IsValid() || !MoveComp.IsValid())
        return;

    ClearSlamWaiting();
    
    bPendingSlam = bAsSlam;
    bImpactDone  = false;
    
    if (!bAsSlam)
    {
        FVector V = MoveComp->Velocity;
        V.Z = -AutoDescendSpeed;
        MoveComp->Velocity = V;

        if (MoveComp->MovementMode != MOVE_Falling)
            MoveComp->SetMovementMode(MOVE_Falling);

        State = ECommandAirState::Descending;
        LaunchStateEnterTime = 0.0f;
        return;
    }
   
    if (EnemyDropDelay > 0.f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            TimerHandle_EnemyDrop,
            [this]()
            {
                // 1. 적들 강제 낙하
                ForceDropEnemiesInRange();
                
                // 2. 플레이어 하강 시작
                if (MoveComp.IsValid())
                {
                    FVector V = MoveComp->Velocity;
                    V.Z = -SlamDownSpeed;
                    MoveComp->Velocity = V;
                    
                    if (MoveComp->MovementMode != MOVE_Falling)
                        MoveComp->SetMovementMode(MOVE_Falling);
                }
            },
            EnemyDropDelay,
            false);
    }
    else
    {
        ForceDropEnemiesInRange();
    }


    State = ECommandAirState::Descending;
}

void UCSkill_CommandLaunchSlam::AbortCommand(bool bResetCooldown)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TimerHandle_AirWindow);
        World->GetTimerManager().ClearTimer(TimerHandle_EnemyDrop);
    }
    
    bPendingSlam = false;
    bImpactDone = false;
    bAwaitingSlamConfirm = false;
    LaunchStateEnterTime = 0.f;
    State = ECommandAirState::Inactive;
    
    if (bBlockOtherActionsWhileAir)
        OnAirCommandLockChanged.Broadcast(false);
    
    if (bResetCooldown)
        StartCooldown();
}

void UCSkill_CommandLaunchSlam::StartSlamConfirmDelay()
{
    bAwaitingSlamConfirm = true;

    if (UWorld* World = GetWorld())
    {
        EarliestSlamConfirmTime = World->GetTimeSeconds() + SlamConfirmDelay;
    }
    else
    {
        EarliestSlamConfirmTime = 0.f;
    }
}

void UCSkill_CommandLaunchSlam::ClearSlamWaiting()
{
    bAwaitingSlamConfirm = false;
    EarliestSlamConfirmTime = 0.f;
}

bool UCSkill_CommandLaunchSlam::IsOnGroundNow() const
{
    if (!OwnerChar.IsValid() || !MoveComp.IsValid()) return false;
    if (MoveComp->IsMovingOnGround()) return true;

    FHitResult Hit;
    const FVector S = OwnerChar->GetActorLocation();
    const FVector E = S + FVector(0,0,-(GroundTouchThreshold + 10.f));
    FCollisionQueryParams P(SCENE_QUERY_STAT(CmdSkillGround), false, OwnerChar.Get());
    return GetWorld()->LineTraceSingleByChannel(Hit, S, E, ECC_Visibility, P) && Hit.bBlockingHit;
}





void UCSkill_CommandLaunchSlam::DoShockwaveImpact()
{
    if (!OwnerChar.IsValid()) return;

    if (ShockwaveFX)
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ShockwaveFX, OwnerChar->GetActorLocation());
    if (ShockwaveSFX)
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), ShockwaveSFX, OwnerChar->GetActorLocation());
    if (ShockwaveCameraShake)
        if (APlayerController* PC = Cast<APlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
            PC->ClientStartCameraShake(ShockwaveCameraShake);
    
    
    // 탱커/보스 포함 모든 적에 피해 적용
    TArray<ACharacter*> Affected;
    CollectCharactersInRadius(Affected, ShockwaveRadius, /*bIncludeLaunchedIgnoringZ=*/true, /*bRiotOnly=*/false);
    AController* Inst = OwnerChar->GetController();

    for (ACharacter* C : Affected)
    {
        if (!C || C == OwnerChar.Get()) continue;
        if (!C->IsA(ACEnemyCharacterBase::StaticClass())) continue;

        if (!ShockwaveDamageType)
            ShockwaveDamageType = UDamageType::StaticClass();
        
        UGameplayStatics::ApplyDamage(
            C,
            ShockwaveDamage,
            Inst,
            OwnerChar.Get(),
            ShockwaveDamageType); // Fury 스택 X
    }

    // 히트스톱 적용
    if (bEnableHitStop && IsValid(HitStopComponent) && Affected.Num() > 0)
    {
        TArray<AActor*> HitStopTargets;
        HitStopTargets.Add(OwnerChar.Get());
        
        for (ACharacter* C : Affected)
        {
            if (C && C->IsA(ACEnemyCharacterBase::StaticClass()))
                HitStopTargets.Add(C);
        }

        if (Hammer.IsValid())
            HitStopTargets.Add(Hammer.Get());

        // 플레이어 애니메이션 명시적 정지
        if (OwnerChar->GetMesh())
        {
            if (UAnimInstance* PlayerAnimInst = OwnerChar->GetMesh()->GetAnimInstance())
            {
                if (UAnimMontage* CurrentMontage = PlayerAnimInst->GetCurrentActiveMontage())
                {
                    PlayerAnimInst->Montage_Pause(CurrentMontage);
                    
                    FTimerHandle ResumeTimer;
                    GetWorld()->GetTimerManager().SetTimer(
                        ResumeTimer,
                        [PlayerAnimInst, CurrentMontage]()
                        {
                            if (IsValid(PlayerAnimInst) && IsValid(CurrentMontage))
                            {
                                PlayerAnimInst->Montage_Resume(CurrentMontage);
                            }
                        },
                        SlamHitStopDuration,
                        false
                    );
                }
            }
        }

        // 해머 애니메이션 명시적 정지
        if (Hammer.IsValid() && Hammer->GetHammerMesh())
        {
            if (UAnimInstance* HammerAnimInst = Hammer->GetHammerMesh()->GetAnimInstance())
            {
                if (UAnimMontage* CurrentMontage = HammerAnimInst->GetCurrentActiveMontage())
                {
                    HammerAnimInst->Montage_Pause(CurrentMontage);
                    
                    FTimerHandle ResumeTimer;
                    GetWorld()->GetTimerManager().SetTimer(
                        ResumeTimer,
                        [HammerAnimInst, CurrentMontage]()
                        {
                            if (IsValid(HammerAnimInst) && IsValid(CurrentMontage))
                            {
                                HammerAnimInst->Montage_Resume(CurrentMontage);
                            }
                        },
                        SlamHitStopDuration,
                        false
                    );
                }
            }
        }

        HitStopComponent->StartMultipleHitStop(
            HitStopTargets,
            SlamHitStopDuration,
            SlamHitStopTimeScale);
    }
}

void UCSkill_CommandLaunchSlam::CollectCharactersInRadius(TArray<ACharacter*>& OutChars, float Radius,
    bool bIncludeLaunchedIgnoringZ, bool bRiotOnly) const
{
    OutChars.Reset();
    if (!OwnerChar.IsValid()) return;
    UWorld* W = GetWorld(); if (!W) return;
 
    const FVector Center = OwnerChar->GetActorLocation();
    FCollisionObjectQueryParams Obj;

    if (bRiotOnly)
    {
        Obj.AddObjectTypesToQuery(PF::Collision::RiotEnemy);
    }
    else
    {
        Obj.AddObjectTypesToQuery(ECC_Pawn);
        Obj.AddObjectTypesToQuery(PF::Collision::RiotEnemy);
    }  
    
    FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
    FCollisionQueryParams  Q(SCENE_QUERY_STAT(CmdOverlap), false, OwnerChar.Get());
 
    TArray<FOverlapResult> OS;
    if (!W->OverlapMultiByObjectType(OS, Center, FQuat::Identity, Obj, Sphere, Q)) return;

    for (const FOverlapResult& O : OS)
    {
        ACharacter* C = Cast<ACharacter>(O.GetActor());
        if (!C || C == OwnerChar.Get()) continue;

        if (OutChars.Contains(C)) continue;
    
       if (bRiotOnly)
       {
           const UCapsuleComponent* Capsule = C->FindComponentByClass<UCapsuleComponent>();
           if (!Capsule || Capsule->GetCollisionObjectType() != PF::Collision::RiotEnemy)
           {
               continue;
           }
       }       

        
        const float ZDiff = FMath::Abs(C->GetActorLocation().Z - Center.Z);
        if (ZDiff > ZTolerance)
        {
            if (!bIncludeLaunchedIgnoringZ)
                continue;
                
            bool bAllowAirborne = false;
            if (const UCharacterMovementComponent* EnemyMove = C->GetCharacterMovement())
            {
                bAllowAirborne = EnemyMove->IsFalling();
            }
                
            if (!bAllowAirborne)
                continue;
        }
 
        OutChars.Add(C);
    }
}


bool UCSkill_CommandLaunchSlam::IsLaunchableEnemy(ACharacter* C) const
{
    if (!C) return false;

    for (const FName& Tag : LaunchDenyTags)
        if (C->ActorHasTag(Tag))
            return false;

    bool bHasAllow = false;
    for (const FName& Tag : LaunchAllowTags)
        if (C->ActorHasTag(Tag))
            { bHasAllow = true; break; }

    if (bHasAllow) return true;

    for (const TSoftClassPtr<ACharacter>& SoftCls : LaunchAllowClassFallbacks)
        if (UClass* Cls = SoftCls.Get())
            if (C->IsA(Cls))
                return true;

    for (const TSoftClassPtr<ACharacter>& SoftCls : LaunchDenyClassFallbacks)
        if (UClass* Cls = SoftCls.Get())
            if (C->IsA(Cls))
                return false;

    return false; // 정보 없으면 보수적
}

void UCSkill_CommandLaunchSlam::ForceDropEnemiesInRange()
{
    if (!OwnerChar.IsValid())
        return;
    
    TArray<ACharacter*> Neighbors;
    CollectCharactersInRadius(Neighbors, FMath::Max(LaunchRadius, ShockwaveRadius), /*bIncludeLaunchedIgnoringZ=*/true, /*bRiotOnly=*/true);
         
    const float DropSpeed = FMath::Max(EnemyForceDropSpeed, 0.f);
    if (DropSpeed <= 0.f)
        return;
    
    for (ACharacter* C : Neighbors)
    {
        if (!C || C == OwnerChar.Get())
            continue;
            
        if (!C->IsA(ACEnemyCharacterBase::StaticClass()))
            continue;
            
        if (UCharacterMovementComponent* EnemyMove = C->GetCharacterMovement())
        {
            if (EnemyMove->MovementMode != MOVE_Falling)
            {
                EnemyMove->SetMovementMode(MOVE_Falling);
            }
                    
            FVector NewVelocity = EnemyMove->Velocity;
            NewVelocity.Z = -DropSpeed;
            EnemyMove->Velocity = NewVelocity;
                    
            C->LaunchCharacter(FVector(0.f, 0.f, -DropSpeed), /*bXYOverride=*/false, /*bZOverride=*/true);
        }}
}




UAnimInstance* UCSkill_CommandLaunchSlam::GetPlayerAnimInstance() const
{
    return (OwnerChar.IsValid() && OwnerChar->GetMesh())
        ? OwnerChar->GetMesh()->GetAnimInstance() : nullptr;
}

UAnimInstance* UCSkill_CommandLaunchSlam::GetHammerAnimInstance() const
{
    return (Hammer.IsValid() && Hammer->GetHammerMesh())
        ? Hammer->GetHammerMesh()->GetAnimInstance() : nullptr;
}

void UCSkill_CommandLaunchSlam::PlayCharMontageSafe(UAnimMontage* Montage, FName Section, float PlayRate)
{
    if (!Montage) return;
    if (UAnimInstance* Anim = GetPlayerAnimInstance())
    {
        float Len = Anim->Montage_Play(Montage, PlayRate);
        if (Len > 0.f && Section != NAME_None)
            Anim->Montage_JumpToSection(Section, Montage);
    }
}

void UCSkill_CommandLaunchSlam::PlayHammerMontageSafe(UAnimMontage* Montage, FName Section, float PlayRate)
{
    if (!Montage) return;
    
    if (UAnimInstance* Anim = GetHammerAnimInstance())
    {
        float Len = Anim->Montage_Play(Montage, PlayRate);
        if (Len > 0.f && Section != NAME_None)
            Anim->Montage_JumpToSection(Section, Montage);
    }
}


bool UCSkill_CommandLaunchSlam::IsOnCooldown() const
{
    if (UWorld* World = GetWorld())
    {
        const float TimeSinceUse = World->GetTimeSeconds() - LastUsedTime;
        return TimeSinceUse < CooldownTime;
    }
    return false;
}

float UCSkill_CommandLaunchSlam::GetRemainingCooldown() const
{
    if (!IsOnCooldown())
        return 0.f;
    
    if (UWorld* World = GetWorld())
    {
        const float TimeSinceUse = World->GetTimeSeconds() - LastUsedTime;
        return FMath::Max(0.f, CooldownTime - TimeSinceUse);
    }
    return 0.f;
}

float UCSkill_CommandLaunchSlam::GetCooldownPercent() const
{
    if (CooldownTime <= 0.f)
        return 0.f;
    
    const float Remaining = GetRemainingCooldown();
    return FMath::Clamp(Remaining / CooldownTime, 0.f, 1.f);
}

void UCSkill_CommandLaunchSlam::StartCooldown()
{
    if (UWorld* World = GetWorld())
    {
        LastUsedTime = World->GetTimeSeconds();
        UE_LOG(LogTemp, Log, TEXT("[CommandSlam] Cooldown started: %.1fs"), CooldownTime);
    }
}