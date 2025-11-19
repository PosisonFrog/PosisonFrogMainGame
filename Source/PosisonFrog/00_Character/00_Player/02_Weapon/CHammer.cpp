// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/02_Weapon/CHammer.h"

#include "99_Util/CLog.h"
#include "Components/BoxComponent.h"

// Sets default values
ACHammer::ACHammer()
{
	DamageBoxSocketName = FName("HammerHead_Socket");
	DamageBoxRelativeLocation = FVector::ZeroVector;
	DamageBoxRelativeRotation = FRotator::ZeroRotator;
	DamageBoxExtent = FVector(100.f, 100.f, 100.f);

	Damage = 20.0f;

	if (WeaponMesh)
	{
		WeaponMesh->SetGenerateOverlapEvents(false);
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		WeaponMesh->SetCollisionResponseToAllChannels(ECR_Block);
		WeaponMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		WeaponMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		WeaponMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		DamageBox->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
		DamageBox->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap);
		DamageBox->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);
	}

	if (DamageBox)
	{
		DamageBox->SetGenerateOverlapEvents(false);
		DamageBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		DamageBox->SetCollisionObjectType(ECC_WorldDynamic);
		DamageBox->SetCollisionResponseToAllChannels(ECR_Ignore);
		DamageBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		DamageBox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
		DamageBox->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap);
		DamageBox->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);
	}
}

/*void ACHammer::PlayAttackVFX(int32 ComboIndex)
{
	if (!AttackVFX.IsValidIndex(ComboIndex) && !AttackVFX[ComboIndex]->IsValid())
	{
		if (bDebugLog)
			CLog::Log(FString::Printf(TEXT("ACHammer : AttackVFX for Combo Index %d is not valid"), ComboIndex));
			
		return;
	}

	ACharacter* OwnerChar = GetCachedOwnerCharacter();
	if (!IsValid(OwnerChar))
		return;

	UWorld* World = GetWorld();
	if (!IsValid(World))
		return;

	const FVector BaseLocation = OwnerChar->GetActorLocation();
	const FRotator BaseRotation = OwnerChar->GetActorRotation();

	FVector FinalLocation = BaseLocation;
	FRotator FinalRotation = BaseRotation;

	if (AttackVFX_Transforms.IsValidIndex(ComboIndex))
	{
		const FComboVFX_Transform& TransformOffset = AttackVFX_Transforms[ComboIndex];

		FVector RotatedLocationOffset = BaseRotation.RotateVector(TransformOffset.LocationOffset);
		FinalLocation += RotatedLocationOffset;

		FinalRotation += TransformOffset.RotationOffset;
	}

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		AttackVFX[ComboIndex],
		FinalLocation,
		FinalRotation,
		FVector(1.0f),
		true,
		true);
}*/

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
