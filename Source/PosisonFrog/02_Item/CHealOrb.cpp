#include "02_Item/CHealOrb.h"

#include "00_Character/02_Component/CHealthComponent.h"
#include "Components/SphereComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
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
	DetectSphere->OnComponentBeginOverlap.AddDynamic(this, &ACHealOrb::OnDetectOverlap);

	// Spawn VFX / SFX
	if (VFX_Spawn)
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), VFX_Spawn, GetActorLocation());

	if (SFX_Spawn)
		UGameplayStatics::PlaySoundAtLocation(this, SFX_Spawn, GetActorLocation());
}

void ACHealOrb::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!TargetActor.IsValid())
	{
		SetActorTickEnabled(false);
		Velocity = FVector::ZeroVector;
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector ToTarget = TargetActor->GetActorLocation() - CurrentLocation;
	const float Distance = ToTarget.Size();
	if (Distance < KINDA_SMALL_NUMBER)
	{
		Velocity = FVector::ZeroVector;
		return;
	}

	const FVector Direction = ToTarget / Distance;

	// --- 원하는 속도 (거리 기반 부스트) ---
	float Boost = 0.0f;
	if (HomingBoost > 0.0f && HomingBoostRadius > 0.0f)
	{
		const float Alpha = FMath::Clamp(1.0f - (Distance / HomingBoostRadius), 0.0f, 1.0f);
		Boost = HomingBoost * Alpha;
	}
	const float DesiredSpeed = Speed + Boost;
	const FVector DesiredVelocity = Direction * DesiredSpeed;

	// --- 부드러운 조향/가속 ---
	Velocity = FMath::VInterpTo(Velocity, DesiredVelocity, DeltaTime, HomingTurnRate);

	// 곡선(횡방향) 궤적 추가
	FVector CurveOffset = FVector::ZeroVector;
	if (bUseSineCurve && CurveAmplitude > 0.0f && CurveFrequency > 0.0f)
	{
		CurvePhase += DeltaTime * CurveFrequency * 2.0f * PI;

		// Dir에 수직인 횡방향 벡터(Up에 평행할 때 보정)
		FVector Side = FVector::CrossProduct(FVector::UpVector, Direction);
		if (Side.IsNearlyZero())
			Side = FVector::CrossProduct(Direction, FVector::RightVector);
		Side.Normalize();

		float Amp = CurveAmplitude;
		if (bCurveDampWithDistance)
		{
			// 멀리선 크게, 가까울수록 줄이기 (0.2~1.0 범위)
			const float Damp = FMath::Clamp(Distance / (HomingBoostRadius > 0.f ? HomingBoostRadius : DetectRadius), 0.2f, 1.f);
			Amp *= Damp;
		}

		CurveOffset = Side * (Amp * FMath::Sin(CurvePhase));
	}

	// 이동량
	const FVector Move = Velocity * DeltaTime;

	// 1) 주 이동(조향 이동) 먼저 적용
	FHitResult Hit;
	AddActorWorldOffset(Move, /*bSweep*/ true, &Hit);

	// 벽에 막히면 바운스/슬라이드 처리
	if (Hit.bBlockingHit)
	{
		if (VFX_Bounce)
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), VFX_Bounce, Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
		if (SFX_Bounce)
			UGameplayStatics::PlaySoundAtLocation(this, SFX_Bounce, Hit.ImpactPoint);

		if (bSlideOnBlock)
		{
			const FVector N = Hit.Normal.GetSafeNormal();
			const FVector TangentVel = FVector::VectorPlaneProject(Velocity, N);

			// 약간 밀어내기 (끼임 방지)
			AddActorWorldOffset(N * 0.5f, /*bSweep*/ false);
			
			// 여기를 수정해야함
			
			Velocity = TangentVel; // 표면을 따라 미끄러짐
		}
		else
		{
			TargetActor = nullptr;
			SetActorTickEnabled(false);
			Velocity = FVector::ZeroVector;
			return;
		}
	}

	// 2) 곡선 오프셋 적용(소량, 충돌 고려)
	if (!CurveOffset.IsNearlyZero())
	{
		FHitResult SideHit;
		AddActorWorldOffset(CurveOffset, /*bSweep*/ true, &SideHit);
		// 막히면 곡선 오프셋은 무시 (주 이동만 유지)
	}

	// 안전: 목표에 거의 근접하면 스텝 클램프(오버슈트 방지)
	if (bClampStepToDistance)
	{
		const float NewDist = FVector::Dist(GetActorLocation(), TargetActor->GetActorLocation());
		if (NewDist < KINDA_SMALL_NUMBER)
			SetActorTickEnabled(false);
	}
}

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

void ACHealOrb::OnDetectOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
								UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
								bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this)
		return;

	if (AActor* CurActor = TargetActor.Get())
	{
		const float CurrentDistSq = FVector::DistSquared(GetActorLocation(), CurActor->GetActorLocation());
		const float NewDistSq = FVector::DistSquared(GetActorLocation(), OtherActor->GetActorLocation());
		if (NewDistSq >= CurrentDistSq)
			return;
	}

	TargetActor = OtherActor;

	// 현재 위치에서 TargetActor까지 향하는 단위 방향 벡터
	const FVector Direction = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	Velocity = Direction * Speed;

	SetActorTickEnabled(true);
}
