#include "CRiotRobot.h"
#include "Kismet/GameplayStatics.h"

ACRiotRobot::ACRiotRobot()
{
	AttackInterval = 1.0f;
	AttackEnterDistance = 160.f;
	AttackExitDistance  = 220.f;
	AttackRange = 180.f;
	AttackRadius = 60.f;
}

void ACRiotRobot::DoAttack()
{
	if (!IsInAttackDistance())
	{
		SetState(EEnemyState::Chase);
		return;
	}

	if (IsAttackReady())
	{
		LastAttackTime = GetWorld()->GetTimeSeconds();
		if (Target && FVector::Dist(GetActorLocation(), Target->GetActorLocation()) <= AttackRange)
		{
			UGameplayStatics::ApplyDamage(Target, BaseDamage, GetController(), this, UDamageType::StaticClass());
		}
	}
}
