// CHealthComponent.cpp

#include "00_Character/02_Component/00_PlayerComponent/CPlayerHealthComponent.h"

#include "00_Character/02_Component/00_PlayerComponent/Buffable.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "99_Util/CLog.h"

float UCPlayerHealthComponent::Damage(float InAmount)
{
	if (IBuffable* Buffable = Cast<IBuffable>(OwnerChar.Get()))
	{
		if (Buffable->IsBuffActive())
		{
			float DamageScale = Buffable->GetIncomingDamageScale();
			InAmount *= DamageScale;
			
			UE_LOG(LogTemp, Log, TEXT("[PlayerHealth] Damage scale : %.2f"), DamageScale);
		}
	}

	return Super::Damage(InAmount);
}

void UCPlayerHealthComponent::OnDeathInternal()
{
	CLog::Log(TEXT("UCPlayerHealthComponent::OnDeathInternal -> 플레이어 사망"));
	OnDeath.Broadcast();
}
