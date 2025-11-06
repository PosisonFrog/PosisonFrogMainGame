// CHealthComponent.cpp

#include "00_Character/02_Component/00_PlayerComponent/CPlayerHealthComponent.h"

#include "00_Character/02_Component/00_PlayerComponent/Buffable.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "99_Util/CLog.h"

void UCPlayerHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentOverHeal = 0.0f;
}

void UCPlayerHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(OverHealDecayTimerHandle);
	}
	
	Super::EndPlay(EndPlayReason);
}

void UCPlayerHealthComponent::StartOverHealDecayTimer()
{
	if (!GetWorld())
		return;
	
	GetWorld()->GetTimerManager().ClearTimer(OverHealDecayTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(
		OverHealDecayTimerHandle,
		this,
		&UCPlayerHealthComponent::DecayOverHeal,
		0.1f,
		true,
		OverHealDecayDelay
	);
}

void UCPlayerHealthComponent::DecayOverHeal()
{
	if (CurrentOverHeal <= 0.f)
	{
		GetWorld()->GetTimerManager().ClearTimer(OverHealDecayTimerHandle);
		CurrentOverHeal = 0.f;
		return;
	}
	
	// 초당 감소량을 0.1초 단위로 환산
	const float DecayAmount = OverHealDecayRate * 0.1f;
	CurrentOverHeal = FMath::Max(0.f, CurrentOverHeal - DecayAmount);
	
	// UI 갱신
	OnOverHealChanged.Broadcast(CurrentOverHeal, OverHealMax);
	
	// 오버힐이 모두 소진되면 타이머 정리
	if (CurrentOverHeal <= 0.f)
	{
		GetWorld()->GetTimerManager().ClearTimer(OverHealDecayTimerHandle);
	}
}

float UCPlayerHealthComponent::Healing(float InAmount)
{
	if (InAmount <= 0.0f || bIsDead)
		return 0.0f;

	float ActualHealing = 0.0f;

	// 일반 체력 회복 부분
	if (CurrentHealth < MaxHealth)
	{
		const float NormalHealing = FMath::Min(InAmount, MaxHealth - CurrentHealth);
		ActualHealing += Super::Healing(NormalHealing);
		InAmount -= NormalHealing;
	}

	// 오버힐 적용 부분
	if (InAmount > 0.f && CurrentHealth >= MaxHealth && OverHealMax > 0.0f)
	{
		const float OverHealRoom = OverHealMax - CurrentOverHeal;
		const float OverHealAmount = FMath::Min(InAmount, OverHealRoom);

		if (OverHealAmount > 0.0f)
		{
			CurrentOverHeal += OverHealAmount;
			ActualHealing += OverHealAmount;

			if (bOverHealDecayEnabled)
			{
				StartOverHealDecayTimer();
			}
		}

		// UI 갱신 부분 추가해야함
		OnOverHealChanged.Broadcast(CurrentOverHeal, OverHealMax);
	}
	
	return ActualHealing;
}

float UCPlayerHealthComponent::Damage(float InAmount)
{
	if (InAmount <= 0.0f || bIsDead)
		return 0.0f;

	// 오버힐부터 소진
	if (CurrentOverHeal > 0.0f)
	{
		const float OverHealDamage = FMath::Min(CurrentOverHeal, InAmount);
		CurrentOverHeal -= OverHealDamage;
		InAmount -= OverHealDamage;
		
		if (CurrentOverHeal > 0.0f && bOverHealDecayEnabled)
		{
			StartOverHealDecayTimer();
		}
		else if (CurrentOverHeal <= 0.0f)
		{
			GetWorld()->GetTimerManager().ClearTimer(OverHealDecayTimerHandle);
		}

		OnOverHealChanged.Broadcast(CurrentOverHeal, OverHealMax);

		// 오버힐만 깍였으면 UI 갱신 후 반환
		if (InAmount <= 0.0f)
		{
			return InAmount;
		}
	}

	Super::Damage(InAmount);
	return InAmount;
}

void UCPlayerHealthComponent::OnDeathInternal()
{
	CurrentOverHeal = 0.0f;
	GetWorld()->GetTimerManager().ClearTimer(OverHealDecayTimerHandle);

	OnOverHealChanged.Broadcast(CurrentOverHeal, OverHealMax);
	
	CLog::Log(TEXT("UCPlayerHealthComponent::OnDeathInternal -> 플레이어 사망"));
	OnDeath.Broadcast(GetOwner());
}

