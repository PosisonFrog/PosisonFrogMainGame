// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/02_Weapon/CHammer.h"

#include "99_Util/CLog.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
ACHammer::ACHammer()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	auto DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	SetRootComponent(DefaultSceneRoot);
	
	HammerMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HammerMesh"));
	HammerMesh->SetupAttachment(DefaultSceneRoot);
	
	DamageBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageBox"));
	DamageBox->SetupAttachment(HammerMesh);
	DamageBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DamageBox->SetGenerateOverlapEvents(true);
	DamageBox->SetCollisionObjectType(ECC_WorldDynamic);
	DamageBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	DamageBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

// Called when the game starts or when spawned
void ACHammer::BeginPlay()
{
	Super::BeginPlay();

	if (!IsValid(DamageBox))
	{
		CLog::Log("CHammer::BeginPlay - DamageBox가 유효하지 않음!");
		return;
	}

	if (DamageBox->OnComponentBeginOverlap.IsBound())
	{
		CLog::Log("CHammer::BeginPlay - 이미 바인딩됨");
	}
	else
	{
		// overlap 바인딩
		DamageBox->OnComponentBeginOverlap.AddDynamic(this, &ACHammer::OnDamageBoxBeginOverlap);
	}
	

	// 기본 데미지 적용 핸들러 델리게이트에 바인딩
	OnHammerHit.AddDynamic(this, &ACHammer::ApplyDamageHandler);
}

void ACHammer::OnDamageBoxBeginOverlap(UPrimitiveComponent* OverlapComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bDamageActive)
		return;

	if (!IsValid(OtherActor) || !OtherActor->ActorHasTag("Enemy"))
		return;

	AActor* InstigatorActor = GetOwner();
	if (!IsValid(InstigatorActor) || OtherActor == InstigatorActor)
		return;
	
	if (HitActors.Contains(OtherActor))
		return;
	
	HitActors.Add(OtherActor);
	CLog::Log(FString::Printf(TEXT("CHammer - 히트: %s"), *OtherActor->GetName()));
	
	OnHammerHit.Broadcast(InstigatorActor, OtherActor, Damage, SweepResult);
}

void ACHammer::ApplyDamageHandler(AActor* InstigatorActor, AActor* HitActor, float InDamage, FHitResult HitInfo)
{
	if (!IsValid(HitActor) || !IsValid(InstigatorActor))
	{
		CLog::Log("CHammer::ApplyDamageHandler - Invalid Actor");
		return;
	}
	
	UGameplayStatics::ApplyDamage(
		HitActor,
		InDamage,
		InstigatorActor ? InstigatorActor->GetInstigatorController() : nullptr,
		InstigatorActor,
		nullptr);
	CLog::Log(FString::Printf(TEXT("CHammer - 데미지 적용: %.1f to %s"), InDamage, *HitActor->GetName()));
}

void ACHammer::ResetHitActors()
{
	HitActors.Reset();
}

void ACHammer::ActivateDamage()
{
	if (!IsValid(DamageBox))
	{
		CLog::Log("CHammer::ActivateDamage - DamageBox가 유효하지 않음!");
		return;
	}
	
	bDamageActive = true;
	ResetHitActors();
	DamageBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void ACHammer::DeactivateDamage()
{
	if (!IsValid(DamageBox))
	{
		CLog::Log("CHammer::DeactivateDamage - DamageBox가 유효하지 않음!");
		return;
	}
	
	bDamageActive = false;
	DamageBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ResetHitActors();
}
