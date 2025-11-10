#include "CBossPattern_BasicAttack.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"

UCBossPattern_BasicAttack::UCBossPattern_BasicAttack()
{
	PatternId = FName("BasicAttack");
	AttackIndex = 0;
}

void UCBossPattern_BasicAttack::BeginDestroy()
{
	UE_LOG(LogTemp, Log, TEXT("[BasicAttack] BeginDestroy called"));
	Super::BeginDestroy();
}

void UCBossPattern_BasicAttack::ExecutePattern(int32 PhaseIndex)
{
	Super::ExecutePattern(PhaseIndex);

	UE_LOG(LogTemp, Log, TEXT("[BasicAttack] Executing basic attack - Phase %d"), PhaseIndex);

	if (!OwnerBoss.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[BasicAttack] Invalid OwnerBoss"));
		return;
	}

	// 맨손 공격 - 애니메이션만 재생
	if (AttackMontage)
	{
		PlayMontage(AttackMontage);
	}
	else if (WeaponComponent.IsValid())
	{
		// 몽타주가 없으면 WeaponComponent의 DoAttack 사용
		WeaponComponent->SetCurrentAttackIndex(AttackIndex);
		WeaponComponent->DoAttack();
	}
}

void UCBossPattern_BasicAttack::OnPatternEnd()
{
	Super::OnPatternEnd();

	UE_LOG(LogTemp, Log, TEXT("[BasicAttack] Pattern ended"));
}