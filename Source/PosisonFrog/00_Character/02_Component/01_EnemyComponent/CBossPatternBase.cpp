#include "CBossPatternBase.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "AIController.h"
#include "CBossPatternManager.h"
#include "CEnemyBossPhaseComponent.h"
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

void UCBossPatternBase::ExecutePattern(int32 PhaseIndex, const FBossPatternDefinition& PatternData)
{
	CurrentPhaseIndex = PhaseIndex;
	RuntimeCooldown = PatternData.Cooldown; // DataAsset에서 쿨다운 값 읽기
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
	
	if (OwnerBoss.IsValid() && OwnerBoss->GetBossPhaseComponent())
	{
		const FBossPhaseDefinition* Phase = OwnerBoss->GetBossPhaseComponent()->GetCurrentPhaseDefinition();
		if (Phase)
		{
			for (const FBossPatternDefinition& Pattern : Phase->Patterns)
			{
				if (Pattern.PatternId == PatternId)
				{
					RuntimeCooldown = Pattern.Cooldown;
					break;
				}
			}
		}
	}
}

float UCBossPatternBase::PlayMontage(UAnimMontage* Montage)
{
	if (!OwnerBoss.IsValid() || !Montage)
	{
		return 0.0f;
	}

	if (UAnimInstance* AnimInstance = OwnerBoss->GetMesh()->GetAnimInstance())
	{
		float Duration = AnimInstance->Montage_Play(Montage);
		return Duration;
	}

	return 0.0f;
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
		return false;
	}

	if (OwnerBoss.IsValid() && OwnerBoss->GetWorld())
	{
		return (OwnerBoss->GetWorld()->GetTimeSeconds() - LastUsedTime) < RuntimeCooldown;
	}

	return false;
}

void UCBossPatternBase::StartCooldown()
{
	if (OwnerBoss.IsValid() && OwnerBoss->GetWorld())
	{
		LastUsedTime = OwnerBoss->GetWorld()->GetTimeSeconds();
	}
}


void UCBossPatternBase::FinishPattern(bool bApplyCooldown)
{
	OnPatternEnd();

	if (OwnerBoss.IsValid())
	{
		if (UCBossPatternManager* Manager = OwnerBoss->FindComponentByClass<UCBossPatternManager>())
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] FinishPattern Called. Cooldown Applied: %s"), 
				*PatternId.ToString(), bApplyCooldown ? TEXT("YES") : TEXT("NO"));
			
			Manager->NotifyCurrentPatternEnd(bApplyCooldown);
		}
	}
}