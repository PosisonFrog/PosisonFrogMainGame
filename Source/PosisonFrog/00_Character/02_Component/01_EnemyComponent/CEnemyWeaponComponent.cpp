// Fill out your copyright notice in the Description page of Project Settings.


#include "CEnemyWeaponComponent.h"

#include "00_Character/CWeaponBase.h"
#include "99_Util/CLog.h"
#include "GameFramework/Character.h"


UCEnemyWeaponComponent::UCEnemyWeaponComponent()
{
	// 중요! - 소켓 이름을 블루프린트에서 설정이 필요
	AttachSocketName = SocketName;
}

void UCEnemyWeaponComponent::DoAttack()
{
	if (!OwnerChar.IsValid() || !AttackMontage.IsValidIndex(CurrentAttackIndex))
	{
		CLog::Log(FString::Printf(TEXT("[EnemyWeaponComp] DoAttack Failed : Invalid AttackIndex %d"), CurrentAttackIndex));
		return;
	}

	UAnimMontage* MontageToPlay = AttackMontage[CurrentAttackIndex];
	if (!MontageToPlay)
	{
		CLog::Log(FString::Printf(TEXT("[EnemyWeaponComp] Montage is null at index %d"), CurrentAttackIndex));
		return;
	}

	UAnimInstance* AnimInst = OwnerChar->GetMesh()->GetAnimInstance();
	if (AnimInst && !AnimInst->IsAnyMontagePlaying())
	{
		AnimInst->Montage_Play(MontageToPlay);
	}
}

void UCEnemyWeaponComponent::SpawnWeapon()
{
	if (!WeaponClass)
		return;

	UWorld* World = GetWorld();
	if (!IsValid(World) || !OwnerChar.IsValid())
		return;

	FActorSpawnParameters Params;
	Params.Owner = OwnerChar.Get();
	Params.Instigator = OwnerChar.Get();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	CurrentWeapon = World->SpawnActor<ACWeaponBase>(WeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);

	if (!IsValid(CurrentWeapon))
	{
		CLog::Log(TEXT("[CEnemyWeaponComponent] Weapon Spawn Failed"));
		return;
	}

	CurrentWeapon->DeactivateDamage();
	AttachWeaponToCharacter();
}

void UCEnemyWeaponComponent::HandleWeaponHit(AActor* InstigatorActor, AActor* HitActor, float Damage, FHitResult Hit)
{
	// 부모 클래스의 데미지 처리 로직을 그대로 호출해서 사용
	// 추가적인 로직 (예 : 적중 시 사운드, 이펙트) 필요시 여기에 구현 가능
	Super::HandleWeaponHit(InstigatorActor, HitActor, Damage, Hit);
}
