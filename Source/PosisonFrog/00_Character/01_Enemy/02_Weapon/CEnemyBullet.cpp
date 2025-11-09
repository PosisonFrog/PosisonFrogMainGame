#include "CEnemyBullet.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/01_Enemy/CEnemyCharacterBase.h"

ACEnemyBullet::ACEnemyBullet()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;

    Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
    Sphere->InitSphereRadius(6.f);
    Sphere->SetCollisionProfileName(TEXT("Projectile"));
    Sphere->SetNotifyRigidBodyCollision(true);
    Sphere->SetGenerateOverlapEvents(true);
    RootComponent = Sphere;

    Proj = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    Proj->bRotationFollowsVelocity = true;
    Proj->bShouldBounce = false;
    Proj->ProjectileGravityScale = 0.0f;

    Sphere->OnComponentHit.AddDynamic(this, &ACEnemyBullet::OnCompHit);
    Sphere->OnComponentBeginOverlap.AddDynamic(this, &ACEnemyBullet::OnCompOverlap);

    InitialLifeSpan = 0.f; // LifeSeconds로 관리
}

void ACEnemyBullet::BeginPlay()
{
    Super::BeginPlay();
    if (LifeSeconds > 0.f) SetLifeSpan(LifeSeconds);
}

void ACEnemyBullet::InitBullet(AActor* InShooter, float InDamage, float InSpeed, FVector InDir)
{
    Shooter = InShooter;
    Damage  = InDamage;

    InDir = InDir.GetSafeNormal();
    Proj->InitialSpeed = InSpeed;
    Proj->MaxSpeed     = InSpeed;
    Proj->Velocity     = InDir * InSpeed;
}

void ACEnemyBullet::OnCompHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp,
                              FVector NormalImpulse, const FHitResult& Hit)
{
    if (Other == this || Other == Shooter.Get()) return;

    // 같은 팀(적) 무시
    if (Other && Other->IsA(ACEnemyCharacterBase::StaticClass()))
    {
        if (Sphere)
        {
            Sphere->IgnoreActorWhenMoving(Other, true);
        }
        return;
    }

    // 플레이어만 맞히고 싶다면 필터
    if (bHitOnlyPlayers && Other && !Other->IsA(ACPlayerCharacter::StaticClass()))
    {
        // 월드/오브젝트 등에는 충돌하면 파괴
        ExplodeAt(Hit.ImpactPoint, Hit.ImpactNormal.Rotation(), Other);
        Destroy();
        return;
    }

    if (Other && Other->IsA(ACPlayerCharacter::StaticClass()))
    {
        UGameplayStatics::ApplyDamage(Other, Damage, Shooter.IsValid() ? Shooter->GetInstigatorController() : nullptr, Shooter.Get(), UDamageType::StaticClass());
    }

    ExplodeAt(Hit.ImpactPoint, Hit.ImpactNormal.Rotation(), Other);
    Destroy();
}

void ACEnemyBullet::OnCompOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
                                  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
    if (!bFromSweep) return;
    OnCompHit(OverlappedComp, Other, OtherComp, FVector::ZeroVector, Sweep);
}

void ACEnemyBullet::ExplodeAt(const FVector& Pos, const FRotator& Rot, AActor* HitActor)
{
    if (ImpactFX)
        UGameplayStatics::SpawnEmitterAtLocation(this, ImpactFX, Pos, Rot);
    if (ImpactSFX)
        UGameplayStatics::PlaySoundAtLocation(this, ImpactSFX, Pos);
}
