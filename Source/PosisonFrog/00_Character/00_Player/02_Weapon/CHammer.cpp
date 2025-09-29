// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/02_Weapon/CHammer.h"

#include "99_Util/CLog.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"

// Sets default values
ACHammer::ACHammer()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	SetRootComponent(DefaultSceneRoot);
	
	HammerMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HammerMesh"));
	HammerMesh->SetupAttachment(DefaultSceneRoot);
	
	DamageBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageBox"));
	DamageBox->SetupAttachment(HammerMesh);

	DamageBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DamageBox->SetGenerateOverlapEvents(false);
	DamageBox->SetCollisionObjectType(ECC_WorldDynamic);
	DamageBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	
	DamageBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// (선택) 물리 바디/월드 다이나믹 등 부딪히고 싶은 채널 추가
	DamageBox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);

	if (!DamageTypeClass)
		DamageTypeClass = UDamageType::StaticClass();
}

ACharacter* ACHammer::GetOwnerCharacter() const
{
	return CachedOwnerCharacter.Get();
}

float ACHammer::GetOwnerSpeed() const
{
	if (ACharacter* OwnerChar = CachedOwnerCharacter.Get())
	{
		return OwnerChar->GetVelocity().Size();
	}
	return 0.0f;
}

// Called when the game starts or when spawned
void ACHammer::BeginPlay()
{
	Super::BeginPlay();
    
	if (AActor* MyOwner = GetOwner())
	{
		CachedOwnerCharacter = Cast<ACharacter>(MyOwner);
	}

	// Socket에 DamageBox 부착
	if (HammerMesh && DamageBox && !DamageBoxSocketName.IsNone())
	{
		if (HammerMesh->DoesSocketExist(DamageBoxSocketName))
		{
			DamageBox->AttachToComponent(
				HammerMesh,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				DamageBoxSocketName
			);

			//위치, 회전, 크기 적용
			DamageBox->SetRelativeLocation(DamageBoxRelativeLocation);
			DamageBox->SetRelativeRotation(DamageBoxRelativeRotation); // 회전 추가
			DamageBox->SetBoxExtent(DamageBoxExtent);
            
			if (bDebugLog)
				CLog::Log(FString::Printf(TEXT("데미지 박스가 소켓에 붙여짐: %s"), *DamageBoxSocketName.ToString()));
		}
		else
		{
			if (bDebugLog)
				CLog::Log(FString::Printf(TEXT("Socket '%s' 메쉬가 없음!"), *DamageBoxSocketName.ToString()));
		}
	}

	if (!IsValid(DamageBox))
	{
		CLog::Log(TEXT("CHammer::BeginPlay - DamageBox가 유효하지 않음!"));
		return;
	}

	// 중복 바인딩 방지 후 overlap 바인딩
	DamageBox->OnComponentBeginOverlap.RemoveAll(this);
	DamageBox->OnComponentBeginOverlap.AddDynamic(this, &ACHammer::OnDamageBoxBeginOverlap);
    
	// 기본 데미지 적용 핸들러 델리게이트에 바인딩 (한번만)
	OnHammerHit.RemoveAll(this);
	OnHammerHit.AddDynamic(this, &ACHammer::ApplyDamageHandler);
}

void ACHammer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 안전하게 off & 바인딩 해제
	DeactivateDamage();
	
	if (IsValid(DamageBox))
		DamageBox->OnComponentBeginOverlap.RemoveAll(this);

	OnHammerHit.RemoveDynamic(this, &ACHammer::ApplyDamageHandler);

	Super::EndPlay(EndPlayReason);
}

void ACHammer::ActivateDamage()
{
	if (!IsValid(DamageBox))
	{
		CLog::Log(TEXT("CHammer::ActivateDamage - DamageBox가 유효하지 않음!"));
		return;
	}
	
	bDamageActive = true;
	ResetHitActors();
	DamageBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DamageBox->SetGenerateOverlapEvents(true);
}

void ACHammer::DeactivateDamage()
{
	if (!IsValid(DamageBox))
	{
		CLog::Log(TEXT("CHammer::DeactivateDamage - DamageBox가 유효하지 않음!"));
		return;
	}
	
	bDamageActive = false;
	DamageBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ResetHitActors();
	DamageBox->SetGenerateOverlapEvents(false);
}

void ACHammer::ResetHitActors()
{
	HitActors.Reset();
}



void ACHammer::OnDamageBoxBeginOverlap(UPrimitiveComponent* OverlapComponent, AActor* OtherActor,
                                       UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bDamageActive)
		return;

	if (bServerOnlyDamage)
	{
		if (const AActor* MyOwner = GetOwner())
		{
			
			if (CachedOwnerCharacter.IsValid() && !CachedOwnerCharacter->HasAuthority())
				return;
		}
	}

	if (!ShouldHitActor(OtherActor))
		return;
		
	if (HitActors.Contains(OtherActor))
		return;
	
	HitActors.Add(OtherActor);

	if (bDebugLog)
		CLog::Log(FString::Printf(TEXT("CHammer Hit: %s"), *GetNameSafe(OtherActor)));

	AActor* InstigatorActor = GetOwner();
	OnHammerHit.Broadcast(InstigatorActor, OtherActor, Damage, SweepResult);
}

void ACHammer::ApplyDamageHandler(AActor* InstigatorActor, AActor* HitActor, float InDamage, FHitResult HitInfo)
{
	if (!IsValid(HitActor) || !IsValid(InstigatorActor))
	{
		CLog::Log(TEXT("CHammer::ApplyDamageHandler - Invalid Actor"));
		return;
	}

	AController* InstigatorCtrl = InstigatorActor ? InstigatorActor->GetInstigatorController() : nullptr;

	// HitInfo가 유효하면 포인트 데미지로 위치/노멀 전달
	if (HitInfo.bBlockingHit)
	{
		UGameplayStatics::ApplyPointDamage(
			HitActor,
			InDamage,
			HitInfo.TraceStart.IsNearlyZero() ? FVector::ZeroVector : (HitInfo.ImpactPoint - HitInfo.TraceStart).GetSafeNormal(),
			HitInfo,
			InstigatorCtrl,
			InstigatorActor,
			DamageTypeClass);
	}
	else
	{
		UGameplayStatics::ApplyDamage(
		HitActor,
		InDamage,
		InstigatorCtrl,
		InstigatorActor,
		DamageTypeClass);
	}
	
	if (bDebugLog)
		CLog::Log(FString::Printf(TEXT("CHammer - 데미지 적용: %.1f to %s"), InDamage, *HitActor->GetName()));
}

bool ACHammer::ShouldHitActor(AActor* OtherActor) const
{
	if (!IsValid(OtherActor))
		return false;

	if (OtherActor == this || OtherActor == GetOwner())
		return false;

	if (!EnemyTag.IsNone() && !OtherActor->ActorHasTag(EnemyTag))
		return false;

	if (!OtherActor->CanBeDamaged())
		return false;
	
	return true;
}


