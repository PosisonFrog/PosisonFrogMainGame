// Fill out your copyright notice in the Description page of Project Settings.


#include "CWeaponBase.h"

#include "99_Util/CLog.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"


// Sets default values
ACWeaponBase::ACWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;

	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	SetRootComponent(DefaultSceneRoot);
	
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HammerMesh"));
	WeaponMesh->SetupAttachment(DefaultSceneRoot);
	
	DamageBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageBox"));
	DamageBox->SetupAttachment(WeaponMesh);
	DamageBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DamageBox->SetGenerateOverlapEvents(false);
	DamageBox->SetCollisionObjectType(ECC_WorldDynamic);
	DamageBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	DamageBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// (선택) 물리 바디/월드 다이나믹 등 부딪히고 싶은 채널 추가
	// DamageBox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);

	if (!DamageTypeClass)
		DamageTypeClass = UDamageType::StaticClass();
}

void ACWeaponBase::BeginPlay()
{
	Super::BeginPlay();
    
	if (AActor* MyOwner = GetOwner())
	{
		CachedOwnerCharacter = Cast<ACharacter>(MyOwner);
	}

	// Socket에 DamageBox 부착
	SetupDamageBox();

	if (!IsValid(DamageBox))
	{
		CLog::Log(TEXT("CHammer::BeginPlay - DamageBox가 유효하지 않음!"));
		return;
	}

	// 중복 바인딩 방지 후 overlap 바인딩
	DamageBox->OnComponentBeginOverlap.RemoveAll(this);
	DamageBox->OnComponentBeginOverlap.AddDynamic(this, &ACWeaponBase::OnDamageBoxBeginOverlap);
}

void ACWeaponBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DeactivateDamage();

	if (IsValid(DamageBox))
		DamageBox->OnComponentBeginOverlap.RemoveAll(this);
	
	Super::EndPlay(EndPlayReason);
}

void ACWeaponBase::ActivateDamage()
{
	if (!IsValid(DamageBox))
	{
		CLog::Log(TEXT("CWeaponBase::ActivateDamage - DamageBox가 유효하지 않음!"));
		return;
	}
	
	bDamageActive = true;
	ResetHitActors();
	DamageBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DamageBox->SetGenerateOverlapEvents(true);
}

void ACWeaponBase::DeactivateDamage()
{
	if (!IsValid(DamageBox))
	{
		CLog::Log(TEXT("CWeaponBase::DeactivateDamage - DamageBox가 유효하지 않음!"));
		return;
	}
	
	bDamageActive = false;
	ResetHitActors();
	DamageBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DamageBox->SetGenerateOverlapEvents(false);
}

void ACWeaponBase::ResetHitActors()
{
	HitActors.Reset();
}

float ACWeaponBase::GetOwnerSpeed() const
{
	if (ACharacter* OwnerChar = CachedOwnerCharacter.Get())
	{
		return OwnerChar->GetVelocity().Size();
	}
	return 0.0f;
}

void ACWeaponBase::SetupDamageBox()
{
	if (!WeaponMesh || !DamageBox)
		return;

	if (!DamageBoxSocketName.IsNone() && WeaponMesh->DoesSocketExist(DamageBoxSocketName))
	{
		DamageBox->AttachToComponent(
				WeaponMesh,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				DamageBoxSocketName
		);

		//위치, 회전, 크기 적용
		DamageBox->SetRelativeLocation(DamageBoxRelativeLocation);
		DamageBox->SetRelativeRotation(DamageBoxRelativeRotation); // 회전 추가
		DamageBox->SetBoxExtent(DamageBoxExtent);
            
		if (bDebugLog)
			CLog::Log(FString::Printf(TEXT("데미지 박스가 소켓에 붙여짐 : %s"), *DamageBoxSocketName.ToString()));
	}
	else
	{
		DamageBox->SetRelativeLocation(DamageBoxRelativeLocation);
		DamageBox->SetRelativeRotation(DamageBoxRelativeRotation);
		DamageBox->SetBoxExtent(DamageBoxExtent);

		if (bDebugLog && !DamageBoxSocketName.IsNone())
			CLog::Log(FString::Printf(TEXT("WeaponBase 데미지 박스 소켓 못 찾음 : '%s'"), *DamageBoxSocketName.ToString()));
	}
}

bool ACWeaponBase::ShouldHitActor(AActor* OtherActor) const
{
	if (!IsValid(OtherActor))
		return false;

	if (OtherActor == this || OtherActor == GetOwner())
		return false;

	if (!OtherActor->CanBeDamaged())
		return false;
	
	return true;
}

void ACWeaponBase::OnDamageBoxBeginOverlap(UPrimitiveComponent* OverlapComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bDamageActive)
		return;

	if (bServerOnlyDamage)
	{
		if (CachedOwnerCharacter.IsValid() && !CachedOwnerCharacter->HasAuthority())
			return;
	}

	if (!ShouldHitActor(OtherActor))
	{
		CLog::Log(FString::Printf(TEXT("[WeaponBase] ShouldHitActor failed : %s"), *GetNameSafe(OtherActor)));
		return;
	}
		
	if (HitActors.Contains(OtherActor))
		return;
	
	HitActors.Add(OtherActor);

	if (bDebugLog)
		CLog::Log(FString::Printf(TEXT("ACWeaponBase Hit: %s"), *GetNameSafe(OtherActor)));

	AActor* InstigatorActor = GetOwner();
	OnWeaponBaseHit.Broadcast(InstigatorActor, OtherActor, Damage, SweepResult);
}
