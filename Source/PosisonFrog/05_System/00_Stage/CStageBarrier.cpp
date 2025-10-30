// Fill out your copyright notice in the Description page of Project Settings.


#include "CStageBarrier.h"

#include "99_Util/CLog.h"
#include "Components/BoxComponent.h"


// Sets default values
ACStageBarrier::ACStageBarrier()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetupAttachment(RootComponent);
	CollisionBox->SetBoxExtent(FVector(100.0f, 500.0f, 300.0f));
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetCollisionObjectType(ECC_WorldStatic);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Block);

	BarrierMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrierMesh"));
	BarrierMesh->SetupAttachment(CollisionBox);
	BarrierMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// Called when the game starts or when spawned
void ACStageBarrier::BeginPlay()
{
	Super::BeginPlay();

	CloseBarrier();

	CLog::Log(FString::Printf(TEXT("[Barrier] 섹션 %d 바리게이트 생성 완료"), SectionID));
}

void ACStageBarrier::PlayOpenEffects()
{
	if (BarrierMesh)
		BarrierMesh->SetVisibility(false);

	// 여기에 사운드/VFX 추가
}

void ACStageBarrier::PlayCloseEffects()
{
	if (BarrierMesh)
	{
		BarrierMesh->SetVisibility(true);
	}
	
	// 여기에 사운드/VFX 추가
}

void ACStageBarrier::FullyDeactivate()
{
	if (!bIsOpen)
	{
		return;
	}

	if (BarrierMesh)
	{
		BarrierMesh->SetVisibility(false);
		BarrierMesh->SetActive(false);
	}

	if (CollisionBox)
	{
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CollisionBox->SetActive(false);
	}

	SetActorTickEnabled(false);
	CLog::Log(FString::Printf(TEXT("[ACStageBarrier::FullyDeactivate] 섹션 %d FullyDeactivate"), SectionID));
}

void ACStageBarrier::OpenBarrier()
{
	if (bIsOpen)
	{
		CLog::Log(FString::Printf(TEXT("[ACStageBarrier::OpenBarrier] 섹션 %d 이미 열려있음."), SectionID));
		return;
	}

	bIsOpen = true;
	CLog::Log(FString::Printf(TEXT("[ACStageBarrier::OpenBarrier] 섹션 %d 열리는 중."), SectionID));

	if (CollisionBox)
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PlayOpenEffects();

	OnBarrierOpened.Broadcast(SectionID);

	FTimerHandle DeactivateTimerHandle;
	GetWorldTimerManager().SetTimer(DeactivateTimerHandle, this, &ACStageBarrier::FullyDeactivate, 2.0f, false);
}

void ACStageBarrier::CloseBarrier()
{
	if (!bIsOpen)
	{
		CLog::Log(FString::Printf(TEXT("[ACStageBarrier::CloseBarrier] 섹션 %d 이미 닫혀있음."), SectionID));
		return;
	}

	bIsOpen = false;
	CLog::Log(FString::Printf(TEXT("[ACStageBarrier::CloseBarrier] 섹션 %d 닫는 중."), SectionID));

	if (CollisionBox)
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	if (BarrierMesh)
		BarrierMesh->SetVisibility(true);

	PlayCloseEffects();
}
