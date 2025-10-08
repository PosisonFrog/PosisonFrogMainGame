// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/02_Component/CBaseWeaponComponent.h"

#include "00_Character/CWeaponBase.h"
#include "99_Util/CLog.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

UCBaseWeaponComponent::UCBaseWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCBaseWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar.IsValid())
	{
		CLog::Log(TEXT("[BaseWeaponComp] OwnerCharacter invalid"));
		return;
	}

	if (!WeaponClass)
	{
		CLog::Log(TEXT("[BaseWeaponComp] WeaponClass not set"));
		// 지금 적의 무기가 없어서 아직 return 안했음
		// return;
	}

	SpawnWeapon();

	if (CurrentWeapon)
		CurrentWeapon->OnWeaponBaseHit.AddDynamic(this, &UCBaseWeaponComponent::HandleWeaponHit);
}

void UCBaseWeaponComponent::EnableAttackBoxCollider()
{
	if (IsValid(CurrentWeapon)) CurrentWeapon->ActivateDamage();
}

void UCBaseWeaponComponent::DisableAttackBoxCollider()
{
	if (IsValid(CurrentWeapon)) CurrentWeapon->DeactivateDamage();
}

void UCBaseWeaponComponent::HandleWeaponHit(AActor* InstigatorActor, AActor* HitActor, float Damage, FHitResult Hit)
{
	if (!IsValid(HitActor) || !IsValid(InstigatorActor))
		return;

	AController* InstigatorCtrl = InstigatorActor ? InstigatorActor->GetInstigatorController() : nullptr;

	// HitInfo가 유효하면 포인트 데미지로 위치/노멀 전달
	if (Hit.bBlockingHit)
	{
		UGameplayStatics::ApplyPointDamage(
			HitActor,
			Damage,
			Hit.TraceStart.IsNearlyZero() ? FVector::ZeroVector : (Hit.ImpactPoint - Hit.TraceStart).GetSafeNormal(),
			Hit,
			InstigatorCtrl,
			InstigatorActor,
			UDamageType::StaticClass());
	}
	else
	{
		UGameplayStatics::ApplyDamage(
			HitActor,
			Damage,
			InstigatorCtrl,
			InstigatorActor,
			UDamageType::StaticClass());
	}
    
	OnWeaponHit.Broadcast(HitActor, Damage);
}

void UCBaseWeaponComponent::AttachWeaponToCharacter()
{
	if (!IsValid(CurrentWeapon) || !OwnerChar.IsValid()) return;

	USkeletalMeshComponent* Mesh = OwnerChar->GetMesh();
	if (!IsValid(Mesh))
	{
		CLog::Log(TEXT("[WeaponComp] Owner mesh invalid"));
		return;
	}

	if (AttachSocketName.IsNone() || !Mesh->DoesSocketExist(AttachSocketName))
	{
		CLog::Log(FString::Printf(TEXT("[WeaponComp] Socket not found: %s"),
			*AttachSocketName.ToString()));
		CurrentWeapon->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform);
		return;
	}

	// 원래 회전값 저장
	OriginalWeaponRotation = CurrentWeapon->GetActorRotation();
	bWeaponRotationModified = false;

	const bool bOk = CurrentWeapon->AttachToComponent(
		Mesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		AttachSocketName);

	if (!bOk)
	{
		CLog::Log(TEXT("[WeaponComp] Attach failed, fallback KeepRelative"));
		CurrentWeapon->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform);
	}
}

void UCBaseWeaponComponent::RotateWeapon(const FRotator& AdditionalRotation)
{
	if (!IsValid(CurrentWeapon) || bWeaponRotationModified)
		return;
	FRotator NewRotation = OriginalWeaponRotation + AdditionalRotation;
	CurrentWeapon->SetActorRelativeRotation(NewRotation);
	bWeaponRotationModified = true;
	
	UE_LOG(LogTemp, Log, TEXT("[WeaponComp] Weapon rotated by %s"), *AdditionalRotation.ToString());
}

void UCBaseWeaponComponent::RestoreWeaponRotation()
{
	if (!IsValid(CurrentWeapon) || !bWeaponRotationModified)
		return;
	
	CurrentWeapon->SetActorRelativeRotation(OriginalWeaponRotation);
	bWeaponRotationModified = false;
	
	UE_LOG(LogTemp, Log, TEXT("[WeaponComp] Weapon rotation restored"));
}


