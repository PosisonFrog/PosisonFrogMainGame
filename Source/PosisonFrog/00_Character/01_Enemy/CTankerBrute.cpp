#include "CTankerBrute.h"

#include "AIController.h"
#include "00_Character/02_Component/01_EnemyComponent/CTankerChargeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"       
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Sound/SoundBase.h"


#include "05_System/01_Sound/CSoundManagerSubsystem.h"
#include "05_System/01_Sound//CSoundDataAsset.h"
#include "00_Character/CMainGameModeBase.h"
#include "00_Character/00_Player/CPlayerCharacter.h"

namespace TankerBrute
{
    const FName AttackEffectSocketName = TEXT("SignSocket");
}

using namespace TankerBrute;


ACTankerBrute::ACTankerBrute()
{
    ChargeComp = CreateDefaultSubobject<UCTankerChargeComponent>(TEXT("ChargeComp"));
 
    Tags.AddUnique(TEXT("Enemy.Type.Tank"));
    ApplyPerceptionTuning();
 
    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        MeshComp->SetGenerateOverlapEvents(false);
        MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

        MeshComp->SetCollisionResponseToAllChannels(ECR_Overlap);
    }

    if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
    {
        const float DesiredSeparation = CapsuleComp->GetScaledCapsuleRadius() * 2.f + 5.f;
        SeparationRadius = FMath::Max(SeparationRadius, DesiredSeparation);
        
        CapsuleComp->SetGenerateOverlapEvents(true);
        CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

        CapsuleComp->SetCollisionResponseToAllChannels(ECR_Block);
        CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
        CapsuleComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        CapsuleComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
        CapsuleComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
        CapsuleComp->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);
    }
}

void ACTankerBrute::PostInitProperties()
{
    Super::PostInitProperties();
    SyncAttackTuning();
}

void ACTankerBrute::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyPerceptionTuning();
}

void ACTankerBrute::PostLoad()
{
    Super::PostLoad();
    ApplyPerceptionTuning();
}

#if WITH_EDITOR
void ACTankerBrute::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    SyncAttackTuning();
    ApplyPerceptionTuning();
}
#endif

void ACTankerBrute::BeginPlay()
{
    Super::BeginPlay();

    // 하위 호환성: HitMontage가 설정되어 있고 ComboHitReactionMontages가 비어있으면
    // 모든 콤보 인덱스에 동일한 몽타주 할당
    if (HitMontage && ComboHitReactionMontages.Num() == 0)
    {
        ComboHitReactionMontages.SetNum(3);
        for (int32 i = 0; i < 3; ++i)
        {
            ComboHitReactionMontages[i] = HitMontage;
        }
        UE_LOG(LogTemp, Warning, TEXT("[TankerBrute] Auto-migrated HitMontage to ComboHitReactionMontages. Please set combo-specific montages in Blueprint."));
    }
    
    ApplyPerceptionTuning();
    InitialiseChargeComponent();
    BindPlayerToChargeDelegate(nullptr);
}

void ACTankerBrute::HandlePlayerRespawned(ACPlayerCharacter* NewPlayer)
{
    Super::HandlePlayerRespawned(NewPlayer);
    BindPlayerToChargeDelegate(NewPlayer);
    
    if (ChargeComp)
    {
        ChargeComp->ResetForRespawn();
    }
}

void ACTankerBrute::OnResetForRespawn_Implementation()
{
    Super::OnResetForRespawn_Implementation();
    
    ClearAttackTimers();
    
    if (ChargeComp)
    {
        ChargeComp->ResetForRespawn();
    }
}

void ACTankerBrute::OnRespawned_Implementation()
{
    Super::OnRespawned_Implementation();
    StopMovement();
}

void ACTankerBrute::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (ChargeStopOverrideRestoreTime < 0.f)
    {
        return;
    }
    
    const float Now = GetWorld()->GetTimeSeconds();
    if (Target)
    {
        LastSeenTime = Now;
    }
    
    const bool bCharging = ChargeComp && ChargeComp->IsChargingOrWindup();
    if (bCharging)
    {
        return;
    }
    
    UpdateChargeStopOverride(Now);
}


bool ACTankerBrute::HasVisualOnTarget() const
{
    if (!Target)
    {
        return false;
    }
    
    const float DistSq = FVector::DistSquared(GetActorLocation(), Target->GetActorLocation());
    return DistSq <= FMath::Square(SightDistance);
}

void ACTankerBrute::DoChase()
{
    // PreCharge/Windup/Charging 중이면 상위 FSM 로직 일시 중지
    if (ChargeComp && ChargeComp->IsChargingOrWindup())
    {
        return;
    }
    
    // 돌진 우선
    if (TryStartCharge())
    {
        return; // 컴포넌트가 이후 전이 주도
    }

    // 기본 추격
    Super::DoChase();
}

void ACTankerBrute::DoAttack()
{
    if (!Target)
    {
        SetState(EEnemyState::ReturnHome);
        return;
    }

    if (ChargeComp && ChargeComp->IsChargingOrWindup())
    {
        bIsPerformingMelee = false;
        return;
    }

     
    if (bIsAttacking || bIsPerformingMelee)
    {
        return;
    }
    
    if (TryStartCharge())
    {
        bIsPerformingMelee = false;
        LastSeenTime = GetWorld()->GetTimeSeconds();
        return;
    }
   
    
    const float Dist = DistToTarget();
    const float ExitDistance = FMath::Max(AttackExitDistance, MeleeAttackDistance * 1.1f);
    if (Dist > ExitDistance)
    {
        bIsPerformingMelee = false;
        SetState(EEnemyState::Chase);
        return;
    }

    const float DesiredRange = FMath::Max(MeleeAttackDistance, AttackRange);
    if (Dist > DesiredRange * 0.9f)
    {
        if (bUseNavigation)
        {
            RequestMoveTo(Target->GetActorLocation(), AttackMoveAcceptanceRadius);
        }
        else
        {
            FVector Dir = Target->GetActorLocation() - GetActorLocation();
            Dir.Z = 0.f;
            if (Dir.Normalize())
            {
                AddMovementInput(Dir, 1.f);
            }
        }
    }
    else
    {
        StopMove();
    }

    
    if (!bIsPerformingMelee && IsAttackReady() && Dist <= MeleeAttackDistance)
    {
        StartAttack();
    }
}

void ACTankerBrute::OnDead()
{
    Super::OnDead();

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        if (UAnimInstance* Anim = MeshComp->GetAnimInstance())
        {
            if (DeadMontage)
            {
                Anim->Montage_Play(DeadMontage, 1.0f);
                UE_LOG(LogTemp, Log, TEXT("[TankerBrute] Playing death montage"));
            }
        }
    }
    
    if (HitEffect)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, HitEffect, GetActorLocation(), GetActorRotation());
    }
}

void ACTankerBrute::StartAttack()
{
    if (!Target) return;

    LastAttackTime = GetWorld()->GetTimeSeconds();
    bIsPerformingMelee = true;
    AttackStartedTime = GetWorld()->GetTimeSeconds();

    StopMove();
    PlayMontageIfValid(AttackMontage);
    PlayEnemySound(CachedAttackSound, 1.0f);
    
    
    SpawnAttackEffect();

    GetWorldTimerManager().SetTimer(
        Timer_WindUp,
        this,
        &ACTankerBrute::BeginAttackWindow,
        AttackWindUpTime,
        false);
}

void ACTankerBrute::BeginAttackWindow()
{
    // Base의 분할 스윕 시스템 사용: 창을 열고, 시작 시 즉시 1회 판정
    AttackWindowBegin(AttackActiveWindow);
    ApplyAttackDamage(/*bCheckAngle=*/true);
    SpawnHitEffectAtForward();
    
    FTimerManager& TimerManager = GetWorldTimerManager();
    const FTimerDelegate EndWindowDelegate = FTimerDelegate::CreateUObject(this, &ACTankerBrute::EndAttackWindow, false);
    TimerManager.SetTimer(Timer_EndWindow, EndWindowDelegate, AttackActiveWindow, false);

    const float FinishDelay = AttackWindUpTime + AttackActiveWindow + AttackRecoveryTime;
    TimerManager.SetTimer(Timer_Finish, this, &ACTankerBrute::FinishAttack, FinishDelay, false);
 
    if (bDebugAttackLog)
    {
        UE_LOG(LogTemp, Log, TEXT("[TankerBrute] Attack window opened (%.2fs)"), AttackActiveWindow);
    }
}

void ACTankerBrute::EndAttackWindow(bool bForced)
{
    AttackWindowEnd(bForced);
    GetWorldTimerManager().ClearTimer(Timer_EndWindow);
}

void ACTankerBrute::FinishAttack()
{
    bIsAttacking = false;
    bIsPerformingMelee = false;  
    
    ClearAttackTimers();
 
    if (bDebugAttackLog)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[TankerBrute] Attack finished"));
    }
}

void ACTankerBrute::CancelAttack()
{
    const bool bWasAttacking = bIsAttacking;
 
    if (bWasAttacking)
    {
        EndAttackWindow(true);
        bIsAttacking = false;
    }
 
    ClearAttackTimers();
 
    if (bDebugAttackLog && bWasAttacking)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[TankerBrute] Attack canceled"));
    }
}

// Attack 상태에서 벗어날 때(Chase 등으로 전환 시) 안전하게 타이머 정리
void ACTankerBrute::ExitState(EEnemyState OldState)
{
    Super::ExitState(OldState);
    if (OldState == EEnemyState::Attack)
        CancelAttack();
}

void ACTankerBrute::SpawnAttackEffect() const
{
    if (!AttackEffect)
        return;

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        UNiagaraFunctionLibrary::SpawnSystemAttached(
            AttackEffect,
            MeshComp,
            TankerBrute::AttackEffectSocketName,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::SnapToTarget,
            true);
    }
}

void ACTankerBrute::SpawnHitEffectAtForward() const
{
    if (!HitEffect)
        return;

    const FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * 100.f;
    UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, HitEffect, SpawnLocation, GetActorRotation());
}

void ACTankerBrute::SpawnHitEffectAtLocation() const
{
    if (!HitEffect)
        return;

    UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, HitEffect, GetActorLocation(), GetActorRotation());
}

AAIController* ACTankerBrute::GetEnemyAIController() const
{
    return Cast<AAIController>(GetController());
}

void ACTankerBrute::StopMovement() const
{
    if (AAIController* AI = GetEnemyAIController())
    {
        AI->StopMovement();
    }
}

void ACTankerBrute::StopMovementAndFaceTarget()
{
    StopMovement();

    if (!Target)
        return;

    const FRotator LookAt = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), Target->GetActorLocation());
    SetActorRotation(FRotator(0.f, LookAt.Yaw, 0.f));
}

void ACTankerBrute::ClearAttackTimers()
{
    FTimerManager& TimerManager = GetWorldTimerManager();
    TimerManager.ClearTimer(Timer_WindUp);
    TimerManager.ClearTimer(Timer_EndWindow);
    TimerManager.ClearTimer(Timer_Finish);
}

void ACTankerBrute::InitialiseChargeComponent()
{
    if (!ensure(ChargeComp))
    {
        return;
    }
    
    ChargeComp->OnChargeStateChanged.AddDynamic(this, &ACTankerBrute::HandleChargeStateChanged);
    ChargeComp->OnChargeFinished.AddDynamic(this, &ACTankerBrute::HandleChargeFinished);
}

void ACTankerBrute::ApplyPerceptionTuning()
{
    const float DesiredSight = FMath::Max(SightDistance, ChargeStopDistanceOverride);
    SightDistance = DesiredSight;
    ChaseStartDistance = FMath::Max(ChaseStartDistance, SightDistance);
    ChaseStopDistance = FMath::Max(ChaseStopDistance, ChaseStartDistance);
}    

bool ACTankerBrute::ShouldAttemptCharge() const
{
    if (!(bPreferCharge
        && ChargeComp
        && Target
        && !ChargeComp->IsOnCooldown()
        && HasVisualOnTarget()))
    {
        return false;
    }

    const float DistanceToTarget = DistToTarget();
    if (DistanceToTarget <= MinimumChargeDistance)
    {
        return false;
    }
   
    return true;
}

bool ACTankerBrute::TryStartCharge()
{
    if (!ShouldAttemptCharge())
    {
        return false;
    }
    
    if (!ChargeComp->RequestCharge(Target.Get()))
    {
        return false;
    }
    
    LastSeenTime = GetWorld()->GetTimeSeconds();
    return true;
}

void ACTankerBrute::UpdateChargeStopOverride(float CurrentTime)
{
    if (CurrentTime < ChargeStopOverrideRestoreTime)
    {
        return;
    }
    
    if (CachedChaseStopDistance >= 0.f)
    {
        ChaseStopDistance = CachedChaseStopDistance;
    }
    
    ChargeStopOverrideRestoreTime = -1.f;
}

void ACTankerBrute::HandleImmediatePostCharge(float CurrentTime)
{
    LastChargeFinishedTime = CurrentTime;
    LastSeenTime = CurrentTime;
    
    if (PostChargeChaseGraceTime > 0.f)
    {
        ChargeStopOverrideRestoreTime = CurrentTime + PostChargeChaseGraceTime;
    }
    else
    {
        ChargeStopOverrideRestoreTime = CurrentTime;
    }
}

void ACTankerBrute::HandleChargeStateChanged(EChargeState NewState, EChargeState /*PrevState*/)
{
    // 사운드/FX/상태표시 등 필요 시 구현
    if (NewState == EChargeState::PreCharge || NewState == EChargeState::Windup || NewState == EChargeState::Charging)
    {
        if (CachedChaseStopDistance < 0.f)
        {
            CachedChaseStopDistance = ChaseStopDistance;
        }
           
        ChaseStopDistance = FMath::Max(ChaseStopDistance, ChargeStopDistanceOverride);
        ChargeStopOverrideRestoreTime = -1.f;
            
        LastSeenTime = GetWorld()->GetTimeSeconds();
    }
}

void ACTankerBrute::HandleChargeFinished(EChargeEndReason Reason, AActor* HitActor)
{
    // 돌진 종료 → 상위 FSM 정상 복귀
    if (State != EEnemyState::Dead)
    {
        if (Target)
            SetState(EEnemyState::Chase);
        else
            SetState(EEnemyState::ReturnHome);
    }
    const float Now = GetWorld()->GetTimeSeconds();
    HandleImmediatePostCharge(Now);
}



void ACTankerBrute::SyncAttackTuning()
{
    AttackInterval = AttackIntervalTanker;
    AttackRange = AttackRangeTanker;
    BaseDamage = AttackDamage;
}

//---------------- 사운드 ----------------
void ACTankerBrute::CacheSoundsFromDataAsset()
{
    if (ACMainGameModeBase* GM = Cast<ACMainGameModeBase>(GetWorld()->GetAuthGameMode()))
    {
        if (UCSoundDataAsset* SoundData = GM->GetSoundDataAsset())
        {
            const FCharacterSoundCollection* Sounds = SoundData->GetCharacterSounds(TEXT("TankerBrute"));
            if (Sounds)
            {
                CachedAttackSound = Sounds->AttackSound;
                CachedHitSound = Sounds->HitSound;
                CachedDeathSound = Sounds->DeathSound;
                
                // 돌진 사운드도 추가
                if (Sounds->ChargeSound)
                {
                    CachedChargeSound = Sounds->ChargeSound;
                }
            }
        }
    }
}

void ACTankerBrute::BindPlayerToChargeDelegate(ACPlayerCharacter* PlayerOverride)
{
    if (!ChargeComp)
    {
        return;
    }
 
    ACPlayerCharacter* Player = PlayerOverride;
    if (!Player)
    {
        UWorld* World = GetWorld();
        if (!World)
        {
            return;
        }
        
        APlayerController* PC = World->GetFirstPlayerController();
        if (!PC)
        {
            return;
        }
            
        APawn* PlayerPawn = PC->GetPawn();
        if (!PlayerPawn)
        {
            return;
        }
            
        Player = Cast<ACPlayerCharacter>(PlayerPawn);
    }
 
   
    if (Player)
    {
        ChargeComp->OnPlayerHitByCharge.Clear();
        ChargeComp->OnPlayerHitByCharge.AddUniqueDynamic(Player, &ACPlayerCharacter::OnHitByTankerCharge);
        UE_LOG(LogTemp, Log, TEXT("[TankerBrute] Successfully bound charge delegate to player"));
    }
}