#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CBossProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;
class UParticleSystem;
class UParticleSystemComponent;
class USoundBase;

/**
 * 보스 BARRAGE 패턴용 발사체
 * - 직선으로 날아감 (중력 없음)
 * - 플레이어에게 맞으면 데미지
 * - 일정 시간 후 자동 제거
 */
UCLASS()
class POSISONFROG_API ACBossProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	ACBossProjectile();

	virtual void BeginPlay() override;

	/** 발사체 초기화 (방향, 속도, 데미지 설정) */
	UFUNCTION(BlueprintCallable, Category="Projectile")
	void InitProjectile(AActor* InShooter, float InDamage, float InSpeed, const FVector& InDirection);

protected:
	/** 충돌 이벤트 */
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, 
	           FVector NormalImpulse, const FHitResult& Hit);

	/** 발사체 폭발/소멸 처리 */
	void ExplodeAndDestroy(const FVector& Location);

	/** 플레이어인지 확인 */
	bool IsPlayerActor(AActor* Actor) const;

protected:
	// ───────── 컴포넌트 ─────────
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UParticleSystemComponent> TrailEffect;

	// ───────── 설정 ─────────
	
	/** 플레이어 식별 태그 */
	UPROPERTY(EditAnywhere, Category="Projectile|Targeting")
	FName PlayerTag = TEXT("Player");

	/** 발사체 데미지 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile|Damage")
	float Damage = 20.f;

	/** 발사체 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile|Movement")
	float ProjectileSpeed = 1500.f;

	/** 발사체 수명 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile|Lifetime")
	float LifeSpan = 10.f;

	/** 충돌 반경 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile|Collision")
	float CollisionRadius = 20.f;

	// ───────── 이펙트/사운드 ─────────
	
	/** 발사 시 재생할 파티클 */
	UPROPERTY(EditAnywhere, Category="Projectile|Effects")
	TObjectPtr<UParticleSystem> TrailParticle;

	/** 충돌/폭발 파티클 */
	UPROPERTY(EditAnywhere, Category="Projectile|Effects")
	TObjectPtr<UParticleSystem> ExplosionEffect;

	/** 발사 사운드 */
	UPROPERTY(EditAnywhere, Category="Projectile|Sound")
	TObjectPtr<USoundBase> LaunchSound;

	/** 충돌/폭발 사운드 */
	UPROPERTY(EditAnywhere, Category="Projectile|Sound")
	TObjectPtr<USoundBase> ExplosionSound;

	// ───────── 런타임 ─────────
	
	/** 발사체를 쏜 액터 */
	UPROPERTY()
	TObjectPtr<AActor> Shooter;

	/** 이미 폭발했는지 여부 (중복 처리 방지) */
	bool bHasExploded = false;

	/** 디버그 로그 활성화 */
	UPROPERTY(EditAnywhere, Category="Debug")
	bool bDebugLog = false;
};