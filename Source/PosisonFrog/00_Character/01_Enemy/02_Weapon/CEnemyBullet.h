#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CEnemyBullet.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UParticleSystem;
class USoundBase;

UCLASS()
class POSISONFROG_API ACEnemyBullet : public AActor
{
	GENERATED_BODY()
public:
	ACEnemyBullet();

	/** 스폰 직후 초기화 */
	UFUNCTION(BlueprintCallable)
	void InitBullet(AActor* InShooter, float InDamage, float InSpeed, FVector InDir);

protected:
	virtual void BeginPlay() override;

	UFUNCTION() void OnCompHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp,
							   FVector NormalImpulse, const FHitResult& Hit);
	UFUNCTION() void OnCompOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
								   UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	void ExplodeAt(const FVector& Pos, const FRotator& Rot, AActor* HitActor);

protected:
	UPROPERTY(VisibleAnywhere) USphereComponent* Sphere = nullptr;
	UPROPERTY(VisibleAnywhere) UProjectileMovementComponent* Proj = nullptr;

	UPROPERTY(EditAnywhere, Category="PF|Bullet")
	float LifeSeconds = 3.0f;

	UPROPERTY(EditAnywhere, Category="PF|Bullet")
	bool bHitOnlyPlayers = true;

	UPROPERTY(EditAnywhere, Category="PF|Bullet")
	UParticleSystem* TrailFX = nullptr;

	UPROPERTY(EditAnywhere, Category="PF|Bullet")
	UParticleSystem* ImpactFX = nullptr;

	UPROPERTY(EditAnywhere, Category="PF|Bullet")
	USoundBase* ImpactSFX = nullptr;

	UPROPERTY(Transient) TWeakObjectPtr<AActor> Shooter;
	float Damage = 10.f;
};
