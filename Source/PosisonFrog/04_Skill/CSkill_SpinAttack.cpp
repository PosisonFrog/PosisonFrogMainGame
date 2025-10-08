#include "CSkill_SpinAttack.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraShakeBase.h"
#include "TimerManager.h"
#include "00_Character/02_Component/00_PlayerComponent/CFuryGaugeComponent.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/DamageType.h"

#include "03_Combat/Damage/DamageType_FuryCountable.h"

UCSkill_SpinAttack::UCSkill_SpinAttack()
{
    PrimaryComponentTick.bCanEverTick = false;

    /*if (!DamageTypeClass)
        DamageTypeClass = UDamageType_FuryCountable::StaticClass();
    if (!FinisherDamageTypeClass)
        FinisherDamageTypeClass = UDamageType_FuryCountable::StaticClass();*/

    DamageTypeClass = UDamageType::StaticClass();
    FinisherDamageTypeClass = UDamageType::StaticClass();
}

bool UCSkill_SpinAttack::DoActivate()
{
    if (TimerHandle_SpinTick.IsValid())
        return false; // 이미 동작 중

    // 발동 시점 Fury 스냅샷(원하시면 OnFuryStarted/Ended에서 동적 갱신)
    bFuryActiveSnapshot = (FuryRef && FuryRef->IsEffectActive());

    LastTickTime = GetWorld()->GetTimeSeconds();

    GetWorld()->GetTimerManager().SetTimer(
        TimerHandle_SpinTick,
        this, &UCSkill_SpinAttack::SpinTick,
        TickInterval, true);

    SpinTick(); // 즉시 1회
    return true;
}

bool UCSkill_SpinAttack::DoCancel()
{
    if (!TimerHandle_SpinTick.IsValid())
        return false;

    GetWorld()->GetTimerManager().ClearTimer(TimerHandle_SpinTick);
    return true;
}

void UCSkill_SpinAttack::OnFuryStarted(int32, float, float, int32)
{
    // 스킬 동작 중 Fury가 켜지면 이후 틱에 반영하고 싶을 때:
    // bFuryActiveSnapshot = true;
}

void UCSkill_SpinAttack::OnFuryEnded(bool /*bCanceled*/, float /*Remain*/)
{
    // 스킬 동작 중 Fury가 꺼지면 배율 제거하고 싶을 때:
    // bFuryActiveSnapshot = false;
}

void UCSkill_SpinAttack::OnFuryFinisher(float FinisherDamage)
{
    // Fury 10칸 피니시 발생 → ‘망치 내려찍기’ 연출
    PendingFinisherDamage = (FinisherDamage > 0.f) ? FinisherDamage : FinisherDamageDefault;
    PlayFinisherMontageAndScheduleImpact(PendingFinisherDamage);
}

void UCSkill_SpinAttack::SpinTick()
{
    if (!GetWorld()) return;

    const float Now = GetWorld()->GetTimeSeconds();
    const float Delta = FMath::Max(0.f, Now - LastTickTime);
    LastTickTime = Now;

    AActor* Owner = GetOwner();
    if (!Owner) return;

    // 1) 자동 회전
    if (bAutoRotateOwner && SpinYawSpeedDegPerSec > 0.f)
    {
        const float DeltaYaw = SpinYawSpeedDegPerSec * Delta;
        FRotator R = Owner->GetActorRotation();
        R.Yaw = FMath::UnwindDegrees(R.Yaw + DeltaYaw);
        Owner->SetActorRotation(R);
    }

    // 2) 반경 내 대상 수집
    TArray<AActor*> Targets;
    CollectTargetsInRadius(Targets, AttackRadius);

    // 3) 틱 피해 적용
    float DPS = BaseDPS;
    if (bFuryActiveSnapshot) DPS *= FuryDPSMultiplier;
    const float DamageThisTick = DPS * Delta;

    for (AActor* T : Targets)
    {
        ApplyDamageTo(T, DamageThisTick, DamageTypeClass);
    }
}

void UCSkill_SpinAttack::CollectTargetsInRadius(TArray<AActor*>& OutTargets, float Radius) const
{
    OutTargets.Reset();

    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!World || !Owner) return;

    const FVector Center = Owner->GetActorLocation();

    // Pawn 채널 오버랩
    FCollisionObjectQueryParams ObjParams;
    ObjParams.AddObjectTypesToQuery(ECC_Pawn);

    FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
    FCollisionQueryParams  QueryParams(SCENE_QUERY_STAT(SpinAttackOverlap), false, Owner);

    TArray<FOverlapResult> Overlaps;
    const bool bAny = World->OverlapMultiByObjectType(
        Overlaps, Center, FQuat::Identity, ObjParams, Sphere, QueryParams);

    if (!bAny) return;

    for (const FOverlapResult& O : Overlaps)
    {
        AActor* HitActor = O.GetActor();
        if (!HitActor || HitActor == Owner) continue;

        // 높이 필터
        const float ZDiff = FMath::Abs(HitActor->GetActorLocation().Z - Center.Z);
        if (ZDiff > ZTolerance) continue;

        // TODO: 팀/태그/클래스(ACEnemyCharacterBase 등) 필터링 추가 권장
        OutTargets.Add(HitActor);
    }
}

void UCSkill_SpinAttack::ApplyDamageTo(AActor* Target, float DamageAmount, TSubclassOf<UDamageType> InDamageType) const
{
    if (!Target || DamageAmount <= 0.f) return;

    AActor* Owner = GetOwner();
    AController* InstController = nullptr;
    if (APawn* Pawn = Cast<APawn>(Owner))
        InstController = Pawn->GetController();

    /*UGameplayStatics::ApplyDamage(Target, DamageAmount, InstController, Owner,
        InDamageType ? InDamageType : UDamageType_FuryCountable::StaticClass());*/

    // 일반 데미지 타입 -> 스택 증가 없음
    if (!InDamageType)
        InDamageType = UDamageType::StaticClass();
    
    UGameplayStatics::ApplyDamage(
        Target,
        DamageAmount,
        InstController,
        Owner,
        InDamageType);
}

void UCSkill_SpinAttack::PlayFinisherMontageAndScheduleImpact(float FinisherDamage)
{
    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    float ImpactDelay = FinisherImpactDelay;

    if (OwnerChar && FinisherMontage)
    {
        if (UAnimInstance* Anim = OwnerChar->GetMesh()->GetAnimInstance())
        {
            Anim->Montage_Play(FinisherMontage);

            // ※ 애님 노티파이로 정확 타이밍을 주면 더 좋지만(완전 C++라면),
            //   여기서는 간단히 FinisherImpactDelay 초 후에 충격 판정.
        }
    }

    // 타이머로 충격 판정 예약
    /*GetWorld()->GetTimerManager().SetTimerForNextTick([this, ImpactDelay]()
    {
        GetWorld()->GetTimerManager().SetTimer(FTimerHandle(), [this]() { DoFinisherImpact(); }, ImpactDelay, false);
    });*/
    
    FTimerHandle TempHandle;
    FTimerDelegate TimerDel;
    TimerDel.BindLambda([this]()
    {
        DoFinisherImpact();
    });

    GetWorld()->GetTimerManager().SetTimer(TempHandle, TimerDel, ImpactDelay, false);
}

void UCSkill_SpinAttack::DoFinisherImpact()
{
    if (!GetWorld()) return;

    AActor* Owner = GetOwner();
    if (!Owner) return;

    // 대상 수집(피니시는 별도 반경)
    TArray<AActor*> Targets;
    CollectTargetsInRadius(Targets, FinisherRadius);

    AController* Inst = nullptr;
    if (APawn* P = Cast<APawn>(Owner)) Inst = P->GetController();
    
    // 피해 적용
    /*for (AActor* T : Targets)
    {
        ApplyDamageTo(T, PendingFinisherDamage > 0.f ? PendingFinisherDamage : FinisherDamageDefault,
                      FinisherDamageTypeClass ? FinisherDamageTypeClass : UDamageType_FuryCountable::StaticClass());
    }*/


    if (!FinisherDamageTypeClass)
        FinisherDamageTypeClass = UDamageType::StaticClass();
    
    for (AActor* T : Targets)
    {
        UGameplayStatics::ApplyDamage(
            T,
            PendingFinisherDamage,
            Inst,
            Owner,
            FinisherDamageTypeClass);    // 일반 타입
    }
    
    // 연출(선택)
    if (FinisherImpactFX)
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), FinisherImpactFX, Owner->GetActorLocation(), FRotator::ZeroRotator);
    if (FinisherImpactSFX)
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), FinisherImpactSFX, Owner->GetActorLocation());
    if (FinisherCameraShake)
        if (APlayerController* PC = Cast<APlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
            PC->ClientStartCameraShake(FinisherCameraShake);
}

