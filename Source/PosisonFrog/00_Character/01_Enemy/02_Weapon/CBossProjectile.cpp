#include "CBossProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"

ACBossProjectile::ACBossProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// 충돌 컴포넌트 (루트)
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->InitSphereRadius(CollisionRadius);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	RootComponent = CollisionComp;

	// 메쉬 컴포넌트
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(RootComponent);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetCastShadow(false);

	// 트레일 파티클
	TrailEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailEffect"));
	TrailEffect->SetupAttachment(RootComponent);
	TrailEffect->bAutoActivate = false;
	
	// 발사체 이동 컴포넌트
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->InitialSpeed = ProjectileSpeed;
	ProjectileMovement->MaxSpeed = ProjectileSpeed;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.f;  // ★ 중력 비활성화 (직선 발사)

	// 초기 수명 설정
	InitialLifeSpan = LifeSpan;
}

void ACBossProjectile::BeginPlay()
{
	Super::BeginPlay();

	// 충돌 이벤트 바인딩
	CollisionComp->OnComponentHit.AddDynamic(this, &ACBossProjectile::OnHit);

	// 트레일 이펙트 시작
	if (TrailParticle && TrailEffect)
	{
		TrailEffect->SetAsset(TrailParticle);
		TrailEffect->Activate();
	}

	// 발사 사운드 재생
	if (LaunchSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LaunchSound, GetActorLocation());
	}

	if (bDebugLog)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossProjectile] Spawned at %s, Speed: %.1f"), 
		       *GetActorLocation().ToString(), ProjectileSpeed);
	}
}

void ACBossProjectile::InitProjectile(AActor* InShooter, float InDamage, float InSpeed, const FVector& InDirection)
{
	Shooter = InShooter;
	Damage = InDamage;
	ProjectileSpeed = InSpeed;

	// 이동 컴포넌트 설정
	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = ProjectileSpeed;
		ProjectileMovement->MaxSpeed = ProjectileSpeed;
		ProjectileMovement->Velocity = InDirection.GetSafeNormal() * ProjectileSpeed;
	}

	// 방향으로 회전
	FRotator NewRotation = InDirection.Rotation();
	SetActorRotation(NewRotation);

	if (bDebugLog)
	{
		UE_LOG(LogTemp, Log, TEXT("[BossProjectile] Initialized - Shooter: %s, Damage: %.1f, Speed: %.1f, Direction: %s"),
		       *GetNameSafe(InShooter), InDamage, InSpeed, *InDirection.ToString());
	}
}

void ACBossProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                              FVector NormalImpulse, const FHitResult& Hit)
{
	UWorld* World = this->GetWorld();
	if (!World)
	{
		return;
	}
	
	// 중복 폭발 방지
	if (bHasExploded)
	{
		return;
	}

	// 자기 자신이나 Shooter는 무시
	if (!OtherActor || OtherActor == this || OtherActor == Shooter)
	{
		return;
	}

	if (bDebugLog)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossProjectile] Hit: %s (Component: %s)"), 
		       *GetNameSafe(OtherActor), *GetNameSafe(OtherComp));
	}

	// 플레이어인지 확인
	bool bHitPlayer = IsPlayerActor(OtherActor);

	if (bHitPlayer)
	{
		// 플레이어에게 데미지 적용
		if (Damage > 0.f)
		{
			FDamageEvent DamageEvent;
			OtherActor->TakeDamage(Damage, DamageEvent, 
			                       Shooter ? Shooter->GetInstigatorController() : nullptr, 
			                       Shooter ? Shooter : this);

			if (bDebugLog)
			{
				UE_LOG(LogTemp, Warning, TEXT("[BossProjectile] ★ Damaged Player: %s (Damage: %.1f)"), 
				       *GetNameSafe(OtherActor), Damage);
			}
		}
	}

	if (GroundImpactShake)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			PC->ClientStartCameraShake(GroundImpactShake);
		}
	}
	// 폭발 및 제거
	ExplodeAndDestroy(Hit.ImpactPoint);
}

void ACBossProjectile::ExplodeAndDestroy(const FVector& Location)
{
	
	if (bHasExploded)
	{
		return;
	}

	bHasExploded = true;

	// 폭발 이펙트 생성
	if (ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionEffect, Location, ExplosionEffectRotation);

	}

	// 폭발 사운드 재생
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, Location);
	}

	if (bDebugLog)
	{
		UE_LOG(LogTemp, Log, TEXT("[BossProjectile] Exploded at %s"), *Location.ToString());
	}

	// 발사체 제거
	Destroy();
}

bool ACBossProjectile::IsPlayerActor(AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	// PlayerTag로 플레이어 식별
	if (!PlayerTag.IsNone())
	{
		return Actor->ActorHasTag(PlayerTag);
	}

	// Tag가 없으면 폰인지 체크 (플레이어 컨트롤러 확인)
	if (APawn* Pawn = Cast<APawn>(Actor))
	{
		return Pawn->IsPlayerControlled();
	}

	return false;
}