#include "CBossPattern_BasicAttack.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

UCBossPattern_BasicAttack::UCBossPattern_BasicAttack()
{
	PatternId = FName("BasicAttack");
	AttackIndex = 0;
	PrimaryComponentTick.bCanEverTick = false;
}

bool UCBossPattern_BasicAttack::ExecutePattern(int32 PhaseIndex, const FBossPatternDefinition& PatternData)
{
	Super::ExecutePattern(PhaseIndex, PatternData);
	
	// ✅ 유효성 검증
	if (!OwnerBoss.IsValid()) 
	{
		UE_LOG(LogTemp, Error, TEXT("[BasicAttack] ExecutePattern REJECTED - Invalid OwnerBoss"));
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[BasicAttack] ExecutePattern REJECTED - World is null"));
		return false;
	}

	// ✅ 쿨다운 체크
	if (IsOnCooldown())
	{
		UE_LOG(LogTemp, Error, TEXT("[BasicAttack] ExecutePattern REJECTED - Pattern is on cooldown"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[BasicAttack] Executing basic attack - Phase %d"), PhaseIndex);
	UE_LOG(LogTemp, Log, TEXT("[BasicAttack] ExecutionTime: %.2f, RecoveryTime: %.2f"), 
		PatternData.ExecutionTime, PatternData.RecoveryTime);

	CurrentPatternData = PatternData;
	
	// ✅ 초기화
	HitActors.Empty();
	bCollisionActive = false;
	ClearTimers();

	float Duration = 0.0f;

	if (AttackMontage)
	{
		Duration = PlayMontage(AttackMontage);
		UE_LOG(LogTemp, Log, TEXT("[BasicAttack] Montage Duration: %.2f"), Duration);
	}
	else if (WeaponComponent.IsValid())
	{
		WeaponComponent->SetCurrentAttackIndex(AttackIndex);
		WeaponComponent->DoAttack();
		Duration = 1.0f;
	}

	if (Duration <= 0.0f) 
	{
		Duration = PatternData.ExecutionTime > 0.0f ? PatternData.ExecutionTime : 1.0f;
	}

	// ✅ DataAsset의 RecoveryTime 사용
	float TotalTime = Duration + CurrentPatternData.RecoveryTime;
	
	TWeakObjectPtr<UCBossPattern_BasicAttack> WeakThis(this);
	FTimerDelegate FinishDelegate;
	FinishDelegate.BindLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->FinishPatternInternal();
		}
	});
	
	World->GetTimerManager().SetTimer(FinishTimer, FinishDelegate, TotalTime, false);
	
	UE_LOG(LogTemp, Log, TEXT("[BasicAttack] Pattern will finish in %.2f seconds (Execution: %.2f + Recovery: %.2f)"), 
		TotalTime, Duration, CurrentPatternData.RecoveryTime);
	
	return true;  // ✅ 성공 반환
}

void UCBossPattern_BasicAttack::Anim_AttackStart()
{
	UE_LOG(LogTemp, Warning, TEXT("[BasicAttack] Attack collision ACTIVATED"));
	
	bCollisionActive = true;
	HitActors.Empty();
	
	UWorld* World = GetWorld();
	if (World)
	{
		TWeakObjectPtr<UCBossPattern_BasicAttack> WeakThis(this);
		FTimerDelegate CollisionDelegate;
		CollisionDelegate.BindLambda([WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->CheckCollision();
			}
		});
		
		World->GetTimerManager().SetTimer(
			CollisionCheckTimer,
			CollisionDelegate,
			0.016f, 
			true 
		);
	}
}

void UCBossPattern_BasicAttack::Anim_AttackEnd()
{
	UE_LOG(LogTemp, Warning, TEXT("[BasicAttack] Attack collision DEACTIVATED"));
	
	bCollisionActive = false;
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CollisionCheckTimer);
	}
}

void UCBossPattern_BasicAttack::CheckCollision()
{
	if (!bCollisionActive || !OwnerBoss.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	USkeletalMeshComponent* mesh = OwnerBoss->GetMesh();
	if (!mesh || !mesh->DoesSocketExist(RightHandSocketName))
	{
		if (bDrawDebug)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BasicAttack] Socket '%s' not found!"), *RightHandSocketName.ToString());
		}
		return;
	}

	FVector SocketLocation = mesh->GetSocketLocation(RightHandSocketName);
	
	TArray<FHitResult> HitResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerBoss.Get());
	QueryParams.bTraceComplex = false;

	bool bHit = World->SweepMultiByChannel(
		HitResults,
		SocketLocation,
		SocketLocation,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(AttackSphereRadius),
		QueryParams
	);

	if (bDrawDebug)
	{
		DrawDebugSphere(
			World,
			SocketLocation,
			AttackSphereRadius,
			12,
			bHit ? FColor::Red : FColor::Green,
			false,
			0.1f
		);
	}

	if (!bHit) return;

	for (const FHitResult& Hit : HitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor) continue;

		if (HitActors.Contains(HitActor))
		{
			continue;
		}

		ACharacter* HitCharacter = Cast<ACharacter>(HitActor);
		if (!HitCharacter) continue;

		// 데미지 적용
		UGameplayStatics::ApplyDamage(
			HitCharacter,
			BasicAttackDamage,
			OwnerBoss->GetController(),
			OwnerBoss.Get(),
			UDamageType::StaticClass()
		);

		// 넉백
		FVector KnockDirection = (HitCharacter->GetActorLocation() - OwnerBoss->GetActorLocation()).GetSafeNormal();
		FVector LaunchVelocity = KnockDirection * KnockbackPower;
		LaunchVelocity.Z += KnockbackUpForce;
		HitCharacter->LaunchCharacter(LaunchVelocity, true, true);

		HitActors.Add(HitActor);

		UE_LOG(LogTemp, Warning, TEXT("[BasicAttack] HIT: %s, Damage: %.1f, Knockback: %.1f"), 
			*HitActor->GetName(), BasicAttackDamage, KnockbackPower);
	}
}

void UCBossPattern_BasicAttack::OnPatternEnd()
{
	Super::OnPatternEnd();
	
	// ✅ 타이머 정리
	ClearTimers();
	
	// ✅ 충돌 비활성화
	bCollisionActive = false;
	
	UE_LOG(LogTemp, Log, TEXT("[BasicAttack] OnPatternEnd called"));
}

void UCBossPattern_BasicAttack::Cleanup()
{
	Super::Cleanup();
	ClearTimers();
	bCollisionActive = false;
}

void UCBossPattern_BasicAttack::FinishPatternInternal()
{
	UE_LOG(LogTemp, Log, TEXT("[BasicAttack] FinishPatternInternal called"));
	
	bCollisionActive = false;
	ClearTimers();
	
	FinishPattern(true);
}

void UCBossPattern_BasicAttack::ClearTimers()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(CollisionCheckTimer);
		TimerManager.ClearTimer(FinishTimer);
	}
}