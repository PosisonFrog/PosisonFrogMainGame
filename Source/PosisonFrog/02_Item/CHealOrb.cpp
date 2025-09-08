#include "02_Item/CHealOrb.h"

#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "00_Character/02_Component/CHealthComponent.h"
#include "Components/SphereComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "99_Util/CLog.h"

ACHealOrb::ACHealOrb()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(false); // 타겟 잡히면 Tick을 활성화하기 위해 일단 비활성화

	// === Components ===
	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Sphere);
	
	Sphere->InitSphereRadius(SphereRadius);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionObjectType(ECC_WorldDynamic);
	Sphere->SetGenerateOverlapEvents(true);

	DetectSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DetectSphere"));
	DetectSphere->SetupAttachment(Sphere);
	DetectSphere->InitSphereRadius(DetectRadius);
	DetectSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectSphere->SetCollisionObjectType(ECC_WorldDynamic);
	DetectSphere->SetGenerateOverlapEvents(true);

	// 기본 충돌 설정
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(PlayerBodyChannel, ECR_Overlap);
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	
	DetectSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	DetectSphere->SetCollisionResponseToChannel(PlayerBodyChannel, ECR_Overlap);
}

void ACHealOrb::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (Sphere)
	{
		Sphere->SetSphereRadius(SphereRadius, true);
		Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
		Sphere->SetCollisionResponseToChannel(PlayerBodyChannel, ECR_Overlap);
		Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	}

	if (DetectSphere)
	{
		DetectSphere->SetSphereRadius(DetectRadius, true);
		DetectSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
		DetectSphere->SetCollisionResponseToChannel(PlayerBodyChannel, ECR_Overlap);
	}
}

void ACHealOrb::BeginPlay()
{
	Super::BeginPlay();
	
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &ACHealOrb::OnSphereOverlap);
	DetectSphere->OnComponentBeginOverlap.AddDynamic(this, &ACHealOrb::OnDetectBeginOverlap);
	DetectSphere->OnComponentEndOverlap.AddDynamic(this, &ACHealOrb::OnDetectEndOverlap);

	// Spawn VFX / SFX
	if (VFX_Spawn)
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), VFX_Spawn, GetActorLocation());

	if (SFX_Spawn)
		UGameplayStatics::PlaySoundAtLocation(this, SFX_Spawn, GetActorLocation());
}

void ACHealOrb::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!TargetPlayer.IsValid())
	{
		SetActorTickEnabled(false);
		Velocity = FVector::ZeroVector;
		return;
	}

	// LOS 체크
	const bool bHasLOS = HasLineOfSightToTarget();
	TimeSinceNoLOS = bHasLOS ? 0.0f : (TimeSinceNoLOS + DeltaTime);

	// DetectSphere 체류 여부에 따른 누적 시간
	TimeSinceDetectLost = bTargetInDetect ? 0.0f : (TimeSinceDetectLost + DeltaTime);

	// 경로 재계산 (시야 없고 네비 사용 중일 때만)
	TimeSinceRepath += DeltaTime;
	if (!bHasLOS && bUseNavMesh && TimeSinceRepath >= RepathInterval)
	{
		RebuildPath();
		TimeSinceRepath = 0.0f;
	}

	// ==== 재획득 정책 ====
	// Detect 밖 + LOS 없음 + (경로 없음 또는 유지시간 초과) = 타겟 포기
	const bool bHasPath = (PathPoints.Num() > 0 && PathIndex < PathPoints.Num());
	const bool bPathExpired = (!bHasPath) || (TimeSinceDetectLost > PathHoldTime);
	if (!bTargetInDetect && !bHasLOS && bPathExpired)
	{
		if (bDebugDraw)
			CLog::Print(TEXT("HealOrb : Target dropped"), -1, 1.0f, FColor::Yellow);

		TargetPlayer = nullptr;
		PathPoints.Reset();
		PathIndex = 0;
		SetActorTickEnabled(false);
		Velocity = FVector::ZeroVector;
		return;
	}

	// ==== 이동 ====
	if (bHasLOS || PathPoints.Num() == 0)
		FollowSteering(DeltaTime);
	else
		FollowPath(DeltaTime);
}

// ==== Overlaps ====
void ACHealOrb::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this)
		return;

	if (UCHealthComponent* Health = OtherActor->FindComponentByClass<UCHealthComponent>())
	{
		Health->Healing(HealAmount);

		if (VFX_Pickup)
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), VFX_Pickup, GetActorLocation());
		if (SFX_Pickup)
			UGameplayStatics::PlaySoundAtLocation(this, SFX_Pickup, GetActorLocation());
		
		Destroy();
	}
 }

void ACHealOrb::OnDetectBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this)
		return;

	if (ACPlayerCharacter* Player = Cast<ACPlayerCharacter>(OtherActor))
	{
		TargetPlayer = Player;
		bTargetInDetect = true;
		TimeSinceDetectLost = 0.0f;

		SetActorTickEnabled(true);
		if (bDebugDraw)
			CLog::Print(TEXT("HealOrb : Target acquired"), -1, 1.0f, FColor::Green);
	}
}

void ACHealOrb::OnDetectEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor == TargetPlayer)
	{
		// 즉시 포기 x - 재획득 정책으로 유지
		bTargetInDetect = false;
		if (bDebugDraw)
			CLog::Print(TEXT("HealOrb : Left detect - trying to keep pursuit"), -1, 1.0f, FColor::Cyan);
	}
}

// ==== Steering / Avoidance ====
bool ACHealOrb::HasLineOfSightToTarget() const
{
	if (!TargetPlayer.IsValid())
		return false;

	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(HealOrbLOS), false, this);
	P.AddIgnoredActor(this);

	const FVector From = GetActorLocation();
	const FVector To = TargetPlayer->GetActorLocation();

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, P);

	if (bDebugDraw)
		DrawDebugLine(GetWorld(), From, To, bHit ? FColor::Red : FColor::Blue, false, 0.0f, 0, 1.0f);

	return (!bHit || Hit.GetActor() == TargetPlayer);
}

void ACHealOrb::FollowSteering(float DeltaTime)
{
	if (!TargetPlayer.IsValid())
		return;

	FVector DesiredDir = ComputeDesiredDir();
	DesiredDir = AvoidObstacles(DesiredDir);

	// 기본 목표 속도 (도착 감소)
	const float Dist2D = GetDistToTarget2D();
	float TargetSpeed = (Dist2D < ArriveRadius) ? FMath::GetMappedRangeValueClamped(FVector2D(0.0f, ArriveRadius), FVector2D(0.0f, MaxSpeed), Dist2D) : MaxSpeed;

	if (SpeedByDistanceCurve)
	{
		const float Scale = SpeedByDistanceCurve->GetFloatValue(Dist2D);
		TargetSpeed = FMath::Clamp(TargetSpeed * Scale, 0.0f, MaxSpeed * 3.0f);
	}

	const FVector DesiredVel = DesiredDir * TargetSpeed;

	// 방향 보정 (프레임 독립)
	const float TurnLerp = (TurnAssist > 0.0f) ? FMath::Clamp(TurnAssist / FMath::Max(KINDA_SMALL_NUMBER, DeltaTime), 0.0f, 60.0f) : 0.0f;
	if (TurnLerp > 0.0f)
		Velocity = FMath::VInterpTo(Velocity, DesiredVel, DeltaTime, TurnLerp);

	// 가속 적용
	const FVector ToAdd = DesiredVel - Velocity;
	const FVector Clamped = ToAdd.GetClampedToMaxSize(Accel * DeltaTime);
	Velocity += Clamped;

	// 이동 (스윕 + 슬라이드)
	FVector Delta = Velocity * DeltaTime;
	FHitResult Hit;
	AddActorWorldOffset(Delta, true, &Hit);
	if (Hit.bBlockingHit)
	{
		FVector Slide = FVector::VectorPlaneProject(Delta, Hit.Normal);
		AddActorWorldOffset(Slide, true);
	}
}

float ACHealOrb::GetDistToTarget2D() const
{
	return IsValid(TargetPlayer.Get()) ? FVector::Dist2D(GetActorLocation(), TargetPlayer->GetActorLocation()) : 0.0f;
}

FVector ACHealOrb::ComputeDesiredDir() const
{
	if (!TargetPlayer.IsValid())
		return FVector::ZeroVector;

	FVector To = TargetPlayer->GetActorLocation() - GetActorLocation();
	return To.GetSafeNormal();
}

FVector ACHealOrb::AvoidObstacles(const FVector& DesiredDir) const
{
	const FVector Pos = GetActorLocation();
	const FVector Right = FVector::CrossProduct(DesiredDir, FVector::UpVector).GetSafeNormal();

	auto Probe = [&](const FVector& From, const FVector& Dir, float Len) -> bool
	{
		FHitResult Hit;
		FCollisionQueryParams P(SCENE_QUERY_STAT(HealOrbProbe), false, this);
		P.AddIgnoredActor(this);
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, From, From + Dir * Len, ECC_Visibility, P);

		if (bDebugDraw)
			DrawDebugLine(GetWorld(), From, From + Dir * Len, bHit ? FColor::Red : FColor::Green, false, 0.0f, 0, 0.5f);
		
		return bHit;
	};
	
	const bool bFrontBlocked = Probe(Pos, DesiredDir, ProbeLength);
	if (!bFrontBlocked)
		return DesiredDir;

	const bool bLeftBlocked = Probe(Pos + Right * -SideProbeOffset, DesiredDir, ProbeLength);
	const bool bRightBlocked = Probe(Pos + Right * SideProbeOffset, DesiredDir, ProbeLength);

	FVector TangentLeft = (DesiredDir + Right * -0.9f).GetSafeNormal();
	FVector TangentRight = (DesiredDir + Right * 0.9f).GetSafeNormal();

	if (bLeftBlocked && !bRightBlocked) return TangentRight;
	if (!bLeftBlocked && bRightBlocked) return TangentLeft;

	return bRightBlocked ? TangentLeft : TangentRight;
}

// ==== NavMesh Path ====
void ACHealOrb::RebuildPath()
{
	PathPoints.Reset();
	PathIndex = 0;

	if (!bUseNavMesh || !TargetPlayer.IsValid())
		return;

	if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
	{
		const FVector Start = GetActorLocation();
		const FVector Goal = TargetPlayer->GetActorLocation();

		if (UNavigationPath* Path = NavSys->FindPathToLocationSynchronously(GetWorld(), Start, Goal))
		{
			for (const FNavPathPoint& Pt : Path->GetPath()->GetPathPoints())
				PathPoints.Add(Pt.Location);

			if (PathPoints.Num() >= 2 && FVector::Dist2D(PathPoints[0], Start) < 50.0f)
				PathIndex = 1;

			if (bDebugDraw)
			{
				for (int32 i = 1; i < PathPoints.Num(); ++i)
					DrawDebugLine(GetWorld(), PathPoints[i-1], PathPoints[i], FColor::Cyan, false, RepathInterval, 0, 2.f);
			}
		}
	}
}

void ACHealOrb::FollowPath(float DeltaTime)
{
	if (PathPoints.Num() == 0 || PathIndex >= PathPoints.Num())
		return;

	const FVector Cur = GetActorLocation();

	// 코너 스킵
	for (int32 i = FMath::Min(PathIndex + 2, PathPoints.Num() - 1); i > PathIndex; --i)
	{
		FHitResult Hit;
		FCollisionQueryParams P(SCENE_QUERY_STAT(HealOrbCornerCut), false, this);
		P.AddIgnoredActor(this);
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Cur, PathPoints[i], ECC_Visibility, P))
		{
			PathIndex = i;
			break;
		}
	}

	FVector To = PathPoints[PathIndex] - Cur;
	FVector Dir = To.GetSafeNormal();
	Dir = AvoidObstacles(Dir);

	// 커브가 있다면, 목표는 '플레이어와의 거리' 기준으로 동일하게 적용
	float TargetSpeed = MaxSpeed;
	if (SpeedByDistanceCurve)
	{
		const float Dist2D = GetDistToTarget2D();
		const float Scale = SpeedByDistanceCurve->GetFloatValue(Dist2D);
		TargetSpeed = FMath::Clamp(MaxSpeed * Scale, 0.0f, MaxSpeed * 3.0f);
	}

	FVector DesiredVel = Dir * TargetSpeed;
	const FVector ToAdd = (DesiredVel - Velocity);
	const FVector Clamped = ToAdd.GetClampedToMaxSize(Accel * DeltaTime);
	Velocity += Clamped;

	FVector Delta = Velocity * DeltaTime;

	FHitResult Hit;
	AddActorWorldOffset(Delta, true, &Hit);
	if (Hit.bBlockingHit)
	{
		FVector Slide = FVector::VectorPlaneProject(Delta, Hit.Normal);
		AddActorWorldOffset(Slide, true);
	}

	if (FVector::Dist2D(Cur, PathPoints[PathIndex]) < WaypointReachRadius)
		++PathIndex;
}
