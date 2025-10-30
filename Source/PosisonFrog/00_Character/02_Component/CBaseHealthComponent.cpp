// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/02_Component/CBaseHealthComponent.h"

#include "00_Character/03_AssetData/CPlayerStatAssetData.h"
#include "99_Util/CLog.h"
#include "GameFramework/Character.h"

UCBaseHealthComponent::UCBaseHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCBaseHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerChar = Cast<ACharacter>(GetOwner());
	if (!IsValid(OwnerChar.Get()))
	{
		CLog::Log(TEXT("[BaseHealthComp] OwnerCharacter invalid"));
		return;
	}
	
	// 자식 클래스에서 오버라이드 가능한 초기화
	InitializeHealth();
	bIsDead = (CurrentHealth <= 0.0f);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

float UCBaseHealthComponent::Healing(float InAmount)
{
	// 무효 입력/사망 상태는 무시
	if (InAmount <= 0.f || bIsDead)
		return 0.f;

	const float Old = CurrentHealth;
	const float New = FMath::Clamp(CurrentHealth + InAmount, 0.f, MaxHealth);

	if (FMath::IsNearlyEqual(New, Old))
		return 0.f;

	SetHealthClamped(New);
	return New - Old; // 실제 회복량
}

float UCBaseHealthComponent::Damage(float InAmount)
{
	// 무효 입력/사망 상태는 무시
	if (InAmount <= 0.f || bIsDead)
		return 0.f;
	
	const float Old = CurrentHealth;
	const float New = FMath::Clamp(CurrentHealth - InAmount, 0.f, MaxHealth);
	
	if (FMath::IsNearlyEqual(New, Old))
		return 0.f;

	SetHealthClamped(New);
	return Old - New; // 실제 피해량
}

void UCBaseHealthComponent::SetHealth(float NewHealth)
{
	CurrentHealth = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UCBaseHealthComponent::SetMaxHealth(float NewMax, bool bClampCurrent, bool bResetToMax)
{
	const float PrevMax = MaxHealth;
	MaxHealth = FMath::Max(1.f, NewMax);

	if (bResetToMax)
	{
		SetHealthClamped(MaxHealth);
	}
	else if (bClampCurrent)
	{
		SetHealthClamped(CurrentHealth); // 내부에서 새 Max 기준으로 클램프
	}
	else
	{
		// Max만 바뀌고 Current 유지 → 사망/이벤트 판정은 변동 가능성 있으므로 브로드캐스트
		const bool bWasDead = bIsDead;
		bIsDead = (CurrentHealth <= 0.f);
		if (!FMath::IsNearlyEqual(PrevMax, MaxHealth))
		{
			OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
		}
		if (!bWasDead && bIsDead)
		{
			OnDeath.Broadcast(GetOwner());
		}
	}
}

void UCBaseHealthComponent::InitializeHealth()
{
	// 1) 에셋 기반 최대 체력 반영
	if (PlayerStatAssetData)
	{
		// 최소 1 보장
		MaxHealth = FMath::Max(1.f, PlayerStatAssetData->MaxHp);
	}
	else
	{
		MaxHealth = FMath::Max(1.f, MaxHealth);
	}

	// 2) 시작 체력 결정
	if (bStartAtMaxHealth)
	{
		CurrentHealth = MaxHealth;
	}
	else
	{
		CurrentHealth = FMath::Clamp(CurrentHealth, 0.f, MaxHealth);
	}
}

void UCBaseHealthComponent::SetHealthClamped(float NewValue)
{
	const float Clamped = FMath::Clamp(NewValue, 0.f, MaxHealth);
	if (FMath::IsNearlyEqual(Clamped, CurrentHealth))
		return;

	CurrentHealth = Clamped;

	// 사망 전이 감지
	const bool bWasDead = bIsDead;
	bIsDead = (CurrentHealth <= 0.f);

	// 체력 변경 알림
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	// 이번 호출로 막 사망한 경우에만 1회 알림
	if (!bWasDead && bIsDead)
	{
		OnDeath.Broadcast(GetOwner());
	}
}
