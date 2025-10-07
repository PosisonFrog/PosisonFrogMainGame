// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/02_Weapon/CHammer.h"

#include "99_Util/CLog.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"

// Sets default values
ACHammer::ACHammer()
{
	DamageBoxSocketName = FName("HammerHead_Socket");
	DamageBoxRelativeLocation = FVector::ZeroVector;
	DamageBoxRelativeRotation = FRotator::ZeroRotator;
	DamageBoxExtent = FVector(54.0f, 60.0f, 60.0f);

	Damage = 20.0f;
}

// Called when the game starts or when spawned
void ACHammer::BeginPlay()
{
	Super::BeginPlay();
    
	if (bDebugLog)
		CLog::Log(FString::Printf(TEXT("ACHammer : Initializing, Targeting tag : '%s'"), *EnemyTag.ToString()));
}

void ACHammer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// BaseWeapon에서 정리 작업 수행
	// 해머 전용 정리 (필요시)
	
	Super::EndPlay(EndPlayReason);
}

void ACHammer::SetupDamageBox()
{
	// Base의 기본 설정 호출
	Super::SetupDamageBox();

	// 해머 전용 추가 설정 (필요시)
}

bool ACHammer::ShouldHitActor(AActor* OtherActor) const
{
	if (!Super::ShouldHitActor(OtherActor))
		return false;

	if (!EnemyTag.IsNone() && !OtherActor->ActorHasTag(EnemyTag))
	{
		if (bDebugLog)
			CLog::Log(FString::Printf(TEXT("ACHammer : 적 태그 없음 - %s"), *GetNameSafe(OtherActor)));

		return false;
	}
	
	return true;
}
