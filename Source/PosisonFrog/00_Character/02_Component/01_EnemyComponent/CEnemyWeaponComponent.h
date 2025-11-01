// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/CBaseWeaponComponent.h"
#include "CEnemyWeaponComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCEnemyWeaponComponent : public UCBaseWeaponComponent
{
	GENERATED_BODY()

public:
	UCEnemyWeaponComponent();

	// CBaseWeaponComponent의 가상 함수를 재정의해서 적의 공격을 수행
	virtual void DoAttack() override;

	/** 현재 사용 중인 공격 인덱스를 변경합니다. */
	void SetCurrentAttackIndex(int32 NewIndex);
	
	/** 지정한 인덱스에 유효한 공격 몽타주가 있는지 확인합니다. */
	bool IsAttackIndexValid(int32 Index) const;

protected:
	// 무기 액터를 스폰하고 캐릭터에 부착
	virtual void SpawnWeapon() override;

	// 무기 피격 시 실제 데미지 적용하는 로직
	virtual void HandleWeaponHit(AActor* InstigatorActor, AActor* HitActor, float Damage, FHitResult Hit) override;

protected:
	// 컴포넌트가 사용할 공격 몽타주 목록
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Monatage")
	TArray<UAnimMontage*> AttackMontage;

	// 현재 실행할 공격의 인덱스 (콤보가 존재한다면)
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon|State")
	int32 CurrentAttackIndex = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|State")
	FName SocketName = TEXT("None");
};
