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
}

void UCBossPattern_BasicAttack::BeginDestroy()
{
	UE_LOG(LogTemp, Log, TEXT("[BasicAttack] BeginDestroy called"));
	ClearTimers();
	Super::BeginDestroy();
}

void UCBossPattern_BasicAttack::ExecutePattern(int32 PhaseIndex, const FBossPatternDefinition& PatternData)
{
	Super::ExecutePattern(PhaseIndex, PatternData);
	UE_LOG(LogTemp, Log, TEXT("[BasicAttack] Executing basic attack - Phase %d"), PhaseIndex);
	UE_LOG(LogTemp, Log, TEXT("[BasicAttack] ExecutionTime: %.2f, RecoveryTime: %.2f"), 
		PatternData.ExecutionTime, PatternData.RecoveryTime);

	if (!OwnerBoss.IsValid()) 
	{
		UE_LOG(LogTemp, Error, TEXT("[BasicAttack] Invalid OwnerBoss"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[BasicAttack] World is null!"));
		return;
	}

	CurrentPatternData = PatternData;
	
	HitActors.Empty();
	bCollisionActive = false;

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

	float TotalTime = Duration + PatternData.RecoveryTime;
	
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
	
	UE_LOG(LogTemp, Log, TEXT("[BasicAttack] Pattern will finish in %.2f seconds"), TotalTime);
}

void UCBossPattern_BasicAttack::Anim_AttackStart()
{
	UE_LOG(LogTemp, Warning, TEXT("[BasicAttack] Attack collision ACTIVATED"));
	
	bCollisionActive = true;
	HitActors.Empty(); // 새로운 공격이므로 히트 리스트 초기화
	
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
	
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CollisionCheckTimer);
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

	// 오른손 소켓 위치 가져오기
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
	
	// 구체 트레이스로 충돌 감지
	TArray<FHitResult> HitResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerBoss.Get());
	QueryParams.bTraceComplex = false;

	bool bHit = World->SweepMultiByChannel(
		HitResults,
		SocketLocation,
		SocketLocation, // 시작과 끝이 같음 (구체 오버랩)
		FQuat::Identity,
		ECC_Pawn, // 플레이어 채널
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

		// 이미 데미지를 입힌 액터는 건너뛰기
		if (HitActors.Contains(HitActor))
		{
			continue;
		}

		ACharacter* HitCharacter = Cast<ACharacter>(HitActor);
		if (!HitCharacter) continue;

		UGameplayStatics::ApplyDamage(
			HitCharacter,
			BasicAttackDamage,
			OwnerBoss->GetController(),
			OwnerBoss.Get(),
			UDamageType::StaticClass()
		);

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
	
	UE_LOG(LogTemp, Log, TEXT("[BasicAttack] OnPatternEnd called"));
}

void UCBossPattern_BasicAttack::FinishPatternInternal()
{
	UE_LOG(LogTemp, Log, TEXT("[BasicAttack] FinishPatternInternal called"));
	
	bCollisionActive = false;
	
	ClearTimers();
	OnPatternEnd();
	FinishPattern(true);
}

void UCBossPattern_BasicAttack::ClearTimers()
{
	if (!GetWorld()) return;
	
	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	TimerManager.ClearTimer(CollisionCheckTimer);
	TimerManager.ClearTimer(FinishTimer);
}