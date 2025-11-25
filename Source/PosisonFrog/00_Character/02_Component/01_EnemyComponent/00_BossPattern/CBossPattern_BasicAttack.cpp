#include "CBossPattern_BasicAttack.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "00_Character/02_Component/00_PlayerComponent/CPlayerKnockbackComponent.h" // 넉백 컴포넌트
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/DamageEvents.h"

UCBossPattern_BasicAttack::UCBossPattern_BasicAttack()
{
	PatternId = FName("BasicAttack");
	AttackIndex = 0;
	PrimaryComponentTick.bCanEverTick = false;
}

bool UCBossPattern_BasicAttack::ExecutePattern(int32 PhaseIndex, const FBossPatternDefinition& PatternData)
{
	Super::ExecutePattern(PhaseIndex, PatternData);
	
	if (!OwnerBoss.IsValid()) 
	{
		UE_LOG(LogTemp, Error, TEXT("[BasicAttack] ExecutePattern REJECTED - Invalid OwnerBoss"));
		return false;
	}

	if (IsOnCooldown())
	{
		UE_LOG(LogTemp, Error, TEXT("[BasicAttack] ExecutePattern REJECTED - Pattern is on cooldown"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[BasicAttack] Executing Phase %d (Dmg: %.1f)"), PhaseIndex, BasicAttackDamage);

	CurrentPatternData = PatternData;
	
	HitActors.Empty();
	ClearTimers();

	float Duration = 0.0f;

	if (AttackMontage)
	{
		Duration = PlayMontage(AttackMontage);
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
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FinishTimer, FinishDelegate, TotalTime, false);
	}
	
	return true; 
}

void UCBossPattern_BasicAttack::StartAttackCollision()
{
	HitActors.Empty();
	UE_LOG(LogTemp, Log, TEXT("[BasicAttack] Collision Check START (Hit List Reset)"));
}

// 매 프레임 충돌 검사
void UCBossPattern_BasicAttack::CheckAttackCollision()
{
	if (!OwnerBoss.IsValid()) return;

    UWorld* World = GetWorld();
    if (!World) return;

    USkeletalMeshComponent* Mesh = OwnerBoss->GetMesh();
    if (!Mesh) return;

    FVector SocketLocation = FVector::ZeroVector;
    if (Mesh->DoesSocketExist(RightHandSocketName))
    {
        SocketLocation = Mesh->GetSocketLocation(RightHandSocketName);
    }
    else
    {
        SocketLocation = OwnerBoss->GetActorLocation() + OwnerBoss->GetActorForwardVector() * 100.0f;
    }
    
    //스윕 검사
    TArray<FHitResult> HitResults;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerBoss.Get());
    QueryParams.bTraceComplex = false;
	
    bool bHit = World->SweepMultiByChannel(
        HitResults,
        SocketLocation,
        SocketLocation,
        FQuat::Identity,
        ECC_GameTraceChannel1, 
        FCollisionShape::MakeSphere(AttackSphereRadius),
        QueryParams
    );

    if (bDrawDebug)
    {
        DrawDebugSphere(World, SocketLocation, AttackSphereRadius, 12, 
            bHit ? FColor::Red : FColor::Green, false, 0.05f); 
    }

    if (!bHit) return;

    for (const FHitResult& Hit : HitResults)
    {
        AActor* HitActor = Hit.GetActor();
        if (!HitActor) continue;

        // 이미 맞은 대상은 건너뜀
        if (HitActors.Contains(HitActor)) continue;

        // 플레이어인지 확인 (Character)
        ACharacter* HitCharacter = Cast<ACharacter>(HitActor);
        if (!HitCharacter) continue;

        // [데미지 적용]
        UGameplayStatics::ApplyDamage(
            HitCharacter,
            BasicAttackDamage,
            OwnerBoss->GetController(),
            OwnerBoss.Get(),
            UDamageType::StaticClass()
        );
    	
        
    	if (HitCharacter)
    	{
    		FVector KnockDirection = OwnerBoss->GetActorForwardVector() ;
    		FVector LaunchVelocity = KnockDirection * KnockbackPower;
    		LaunchVelocity.Z += KnockbackUpForce;
    		HitCharacter->LaunchCharacter(LaunchVelocity, true, true);

    		if (UCPlayerKnockbackComponent* KnockbackComp = HitCharacter->FindComponentByClass<UCPlayerKnockbackComponent>())
    		{
    			KnockbackComp->StartKnockback(OwnerBoss.Get());
    		}
    	}

        // 피격 목록에 등록 (이번 공격 중복 피격 방지)
        HitActors.Add(HitActor);

        UE_LOG(LogTemp, Warning, TEXT("[BasicAttack] HIT CONFIRMED: %s (Dmg: %.1f)"), *HitActor->GetName(), BasicAttackDamage);
    }
}

void UCBossPattern_BasicAttack::FinishPatternInternal()
{
	//UE_LOG(LogTemp, Log, TEXT("[BasicAttack] Pattern Duration Finished"));
	ClearTimers();
	FinishPattern(true); // 쿨다운 적용하며 종료
}

void UCBossPattern_BasicAttack::OnPatternEnd()
{
	Super::OnPatternEnd();
	ClearTimers();
	HitActors.Empty();
}

void UCBossPattern_BasicAttack::Cleanup()
{
	Super::Cleanup();
	ClearTimers();
	HitActors.Empty();
}

void UCBossPattern_BasicAttack::ClearTimers()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FinishTimer);
	}
}