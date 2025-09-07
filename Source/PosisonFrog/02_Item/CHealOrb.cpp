#include "02_Item/CHealOrb.h"

#include "00_Character/02_Component/CHealthComponent.h"
#include "99_Util/CLog.h"
#include "Components/SphereComponent.h"

ACHealOrb::ACHealOrb()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(false);
	
	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Sphere);
	Sphere->InitSphereRadius(SphereRadius);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionObjectType(ECC_WorldDynamic);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Sphere->SetGenerateOverlapEvents(true);

	DetectSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DetectSphere"));
	DetectSphere->SetupAttachment(Sphere);
	DetectSphere->InitSphereRadius(DetectRadius);
	DetectSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectSphere->SetCollisionObjectType(ECC_WorldDynamic);
	DetectSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	DetectSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DetectSphere->SetGenerateOverlapEvents(true);
}

void ACHealOrb::BeginPlay()
{
	Super::BeginPlay();
	
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &ACHealOrb::OnSphereOverlap);
	DetectSphere->OnComponentBeginOverlap.AddDynamic(this, &ACHealOrb::OnDetectOverlap);
}

void ACHealOrb::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!IsValid(TargetActor))
	{
		CLog::Log("대상 액터를 못 찾았습니다.");
		SetActorTickEnabled(false);
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();
	const FVector DeltaLocation = TargetLocation - CurrentLocation;
	const float DeltaDistance = DeltaLocation.Size();
	
	const FVector Direction = DeltaLocation / (DeltaDistance + KINDA_SMALL_NUMBER);
	const float Step = Speed * DeltaTime;

	AddActorWorldOffset(Direction * Step, true);
}

void ACHealOrb::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this)
		return;

	if (UCHealthComponent* Health = OtherActor->FindComponentByClass<UCHealthComponent>())
	{
		Health->Healing(HealAmount);
		Destroy();
	}
 }

void ACHealOrb::OnDetectOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
								UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this)
		return;

	if (OtherActor->FindComponentByClass<UCHealthComponent>())
	{
		TargetActor = OtherActor;
		SetActorTickEnabled(true);
	}
}
