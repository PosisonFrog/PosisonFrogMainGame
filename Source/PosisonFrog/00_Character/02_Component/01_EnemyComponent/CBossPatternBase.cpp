#include "CBossPatternBase.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

UCBossPatternBase::UCBossPatternBase()
{
	PatternId = NAME_None;
	CurrentPhaseIndex = 0;
}

void UCBossPatternBase::Initialize(ACEnemyBossCharacter* InOwnerBoss, UCEnemyWeaponComponent* InWeaponComponent)
{
	OwnerBoss = InOwnerBoss;
	WeaponComponent = InWeaponComponent;
}

void UCBossPatternBase::ExecutePattern(int32 PhaseIndex)
{
	CurrentPhaseIndex = PhaseIndex;
	StartCooldown();
	// 자식 클래스에서 구현
}

void UCBossPatternBase::OnPatternEnd()
{
	// 자식 클래스에서 구현
}

void UCBossPatternBase::Cleanup()
{
	// 자식 클래스에서 구현
}

void UCBossPatternBase::UpdatePhaseSettings(int32 PhaseIndex)
{
	CurrentPhaseIndex = PhaseIndex;
	// 자식 클래스에서 구현
}

void UCBossPatternBase::PlayMontage(UAnimMontage* Montage)
{
	if (!OwnerBoss.IsValid() || !Montage)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = OwnerBoss->GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Play(Montage);
	}
}

AActor* UCBossPatternBase::GetPlayerTarget() const
{
	if (!OwnerBoss.IsValid())
	{
		return nullptr;
	}

	return UGameplayStatics::GetPlayerPawn(OwnerBoss->GetWorld(), 0);
}

AAIController* UCBossPatternBase::GetBossAI() const
{
	if (!OwnerBoss.IsValid())
	{
		return nullptr;
	}

	return Cast<AAIController>(OwnerBoss->GetController());
}

bool UCBossPatternBase::IsOnCooldown() const
{
	if (LastUsedTime < 0.f)
	{
		return false; // 아직 한 번도 사용되지 않았으므로 쿨다운이 아님
	}

	// GetWorld()가 안전한지 확인
	if (OwnerBoss.IsValid() && OwnerBoss->GetWorld())
	{
		// (현재 시간 - 마지막 사용 시간)이 쿨다운 시간보다 작으면 쿨다운 상태
		return (OwnerBoss->GetWorld()->GetTimeSeconds() - LastUsedTime) < CooldownDuration;
	}

	return false;
}

void UCBossPatternBase::StartCooldown()
{
	// GetWorld()가 안전한지 확인
	if (OwnerBoss.IsValid() && OwnerBoss->GetWorld())
	{
		LastUsedTime = OwnerBoss->GetWorld()->GetTimeSeconds();
	}
}