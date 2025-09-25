#include "00_Character/01_Enemy/00_Legacy/CEnemyCharacterBaseLegacy.h"
#include "Engine/DamageEvents.h"
#include "01_Item/CHealOrb.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "99_Util/CLog.h"

ACEnemyCharacterBaseLegacy::ACEnemyCharacterBaseLegacy()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ACEnemyCharacterBaseLegacy::BeginPlay()
{
    Super::BeginPlay();

    CurrentHealth = FMath::Max(1.f, MaxHealth);
    SetCanBeDamaged(true);
}

float ACEnemyCharacterBaseLegacy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    if (bIsDead)             return 0.f;
    if (DamageAmount <= 0.f) return 0.f;

    const float OldHealth = CurrentHealth;
    CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.f, MaxHealth);

    // 가벼운 노크백 적용 (너무 잦으면 쿨다운으로 제한)
    if (CurrentHealth > 0.f) // 사망 타격엔 노크백 생략(원하시면 위치 변경)
    {
        ApplyKnockback(DamageEvent, EventInstigator, DamageCauser);
    }

    // HP 이벤트 브로드캐스트
    OnEnemyDamaged.Broadcast(CurrentHealth, MaxHealth);

    if (CurrentHealth <= 0.f && !bIsDead)
    {
        bIsDead = true;
        OnDeath();
    }

    return OldHealth - CurrentHealth; // 실제 적용된 데미지 반환
}

void ACEnemyCharacterBaseLegacy::ApplyKnockback(const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* Causer)
{
    UWorld* World = GetWorld();
    if (!World) return;

    const float Now = World->GetTimeSeconds();
    if (Now - LastKnockTime < KnockbackCooldown)
        return;
    LastKnockTime = Now;

    // 방향 결정: 1) 포인트 데미지의 ShotDirection 사용 → 2) Causer 기준 → 3) 자기 전방의 반대
    FVector Dir = FVector::ZeroVector;
    
    if (DamageEvent.GetTypeID() == FPointDamageEvent::ClassID)
    {
        const FPointDamageEvent* PDE = static_cast<const FPointDamageEvent*>(&DamageEvent);
        if (PDE)
        {
            // ShotDirection은 "가해자 → 피해자" 방향(정규화)로 쓰이는 경우가 많음.
            // 피격자(적이든 나든) 입장에서 뒤로 밀리게 하려면 +ShotDirection이 자연스러워용.
            Dir = PDE->ShotDirection;
        }
    }
   

    if (Dir.IsNearlyZero() && IsValid(Causer))
    {
        // 가해자로부터 멀어지는 방향
        Dir = (GetActorLocation() - Causer->GetActorLocation());
    }

    if (Dir.IsNearlyZero())
    {
        // 최후의 보루: 내 전방의 반대
        Dir = -GetActorForwardVector();
    }

    // 수평만 사용 + 정규화
    Dir.Z = 0.f;
    Dir = Dir.GetSafeNormal();
    if (Dir.IsNearlyZero()) return;

    // 슬라이드 느낌: 잠깐 마찰/제동 낮추기 → 타이머로 복원
    UCharacterMovementComponent* Move = GetCharacterMovement();
    float SavedFriction = 0.f;
    if (Move)
    {
        SavedFriction = Move->GroundFriction;
        Move->GroundFriction = KnockbackGroundFriction;

        FTimerHandle RestoreFrictionTimer;
        World->GetTimerManager().SetTimer(RestoreFrictionTimer, [this, SavedFriction]()
            {
                if (UCharacterMovementComponent* M = GetCharacterMovement())
                {
                    M->GroundFriction = SavedFriction;
                }
            }, KnockbackFrictionTime, false);
    }

    // 실제 노크백: LaunchCharacter를 사용 (질량/물리 영향 없이 튜닝 쉬움)
    const FVector Launch = (Dir * KnockbackSpeedXY) + FVector(0, 0, KnockbackUpSpeed);
    LaunchCharacter(Launch, /*bXYOverride=*/true, /*bZOverride=*/true);
}

void ACEnemyCharacterBaseLegacy::OnDeath()
{
    if (!bIsDead) return;

    SetCanBeDamaged(false);

    // 이동/충돌 정리
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
        Move->DisableMovement();
    }
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // 컨트롤러 분리
    DetachFromControllerPendingDestroy();

    // 드롭
    SpawnHealOrb();

    // 외부에 알림
    OnEnemyDied.Broadcast();

    // 짧게 생존 후 제거(사운드/이펙트 보장)
    if (DeathLifeSpan > 0.f)
        SetLifeSpan(DeathLifeSpan);
    else
        Destroy();
}

void ACEnemyCharacterBaseLegacy::SpawnHealOrb()
{
    if (!HealOrbClass) return;

    UWorld* World = GetWorld();
    if (!World) return;

    // 경사/단차 보정
    const FVector Start = GetActorLocation() + FVector(0, 0, TraceUpOffset);
    const FVector End = GetActorLocation() - FVector(0, 0, TraceDownOffset);

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(Enemy_DeathTrace), false, this);
    const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

    const FVector SpawnLoc = bHit ? (Hit.ImpactPoint + FVector(0, 0, 5.f)) : GetActorLocation();
    const FRotator SpawnRot = FRotator::ZeroRotator;

    FActorSpawnParameters S;
    S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    S.Owner = this;

    World->SpawnActor<ACHealOrb>(HealOrbClass, SpawnLoc, SpawnRot, S);
}


/*노크백 방향

FPointDamageEvent::ShotDirection(있으면 최우선) →

DamageCauser 방향에서 멀어지는 벡터 →

마지막으로 내 전방의 반대.
이렇게 하면 근접/원거리/특수 케이스 모두 무난하게 커버됩니다.

‘살짝 미끄러짐’ 손맛/타격감
GroundFriction을 짧게 낮춰서 미끄러지는 느낌을 주고, 타이머로 복원합니다.
(필요하면 BrakingDecelerationWalking도 함께 낮췄다가 복원하는 방식으로 더 늘릴 수 있습니다.)

과도한 튕김 방지
KnockbackCooldown으로 너무 자주 밀리지 않도록 제한했습니다.
(콤보 히트가 매우 촘촘할 때 물리적으로 던져지거나 떨림이 생기는 문제 방지)

사망 처리 안정성
이동/충돌/컨트롤러를 정리하고 체력 구슬을 안정적으로 드롭한 뒤,
SetLifeSpan으로 짧게 유지 후 제거하여 이펙트/사운드가 안전하게 재생됩니다.

튜닝 티티티티티이이이ㅣ이이이이비비비ㅣ비빕 ㅣㅏ러 ㅗㅇㅂ노란ㅇ모러

노크백 세기: KnockbackSpeedXY = 500~800부터 시작하세요.

위로 튐: KnockbackUpSpeed = 50~120 (원치 않으면 0)

슬라이드 정도: KnockbackGroundFriction = 0.5~1.5 / KnockbackFrictionTime = 0.15~0.25

노크백 빈도 제한: KnockbackCooldown = 0.1~0.2*/