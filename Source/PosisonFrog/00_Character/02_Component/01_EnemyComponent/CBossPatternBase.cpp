#include "CBossPatternBase.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyWeaponComponent.h"
#include "00_Character/02_Component/01_EnemyComponent/CEnemyBossPhaseComponent.h"
#include "03_Combat/Boss/BossPhaseDataAsset.h"
#include "AIController.h"
#include "CBossPatternManager.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

UCBossPatternBase::UCBossPatternBase()
{
	PrimaryComponentTick.bCanEverTick = false;
	PatternId = NAME_None;
	CurrentPhaseIndex = 0;
}

void UCBossPatternBase::BeginPlay()
{
	Super::BeginPlay();
	
	// Owner 캐싱
	OwnerBoss = Cast<ACEnemyBossCharacter>(GetOwner());
	if (OwnerBoss.IsValid())
	{
		// PhaseComponent 참조 획득
		PhaseComponent = OwnerBoss->FindComponentByClass<UCEnemyBossPhaseComponent>();
		WeaponComponent = OwnerBoss->FindComponentByClass<UCEnemyWeaponComponent>();
		
		if (!PhaseComponent.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[PatternBase:%s] PhaseComponent not found!"), *PatternId.ToString());
		}
		
		UE_LOG(LogTemp, Log, TEXT("[PatternBase:%s] BeginPlay - References acquired"), *PatternId.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PatternBase:%s] Owner is not CEnemyBossCharacter!"), *PatternId.ToString());
	}
}

void UCBossPatternBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogTemp, Log, TEXT("[PatternBase:%s] EndPlay called"), *PatternId.ToString());
	
	Cleanup();
	Super::EndPlay(EndPlayReason);
}

void UCBossPatternBase::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// 자식 클래스에서 필요시 오버라이드
}

bool  UCBossPatternBase::ExecutePattern(int32 PhaseIndex, const FBossPatternDefinition& PatternData)
{
	CurrentPhaseIndex = PhaseIndex;
	RuntimeCooldown = PatternData.Cooldown;
	
	
	UE_LOG(LogTemp, Log, TEXT("[PatternBase:%s] ExecutePattern - Phase %d, Cooldown %.2f"), 
		*PatternId.ToString(), PhaseIndex, RuntimeCooldown);

	return true;  

}

void UCBossPatternBase::OnPatternEnd()
{
	UE_LOG(LogTemp, Log, TEXT("[PatternBase:%s] OnPatternEnd called"), *PatternId.ToString());
}

void UCBossPatternBase::Cleanup()
{
	// 자식 클래스에서 구현
}

void UCBossPatternBase::UpdatePhaseSettings(int32 PhaseIndex)
{
	CurrentPhaseIndex = PhaseIndex;
	
	// DataAsset에서 최신 쿨다운 값 읽기
	const FBossPatternDefinition* Data = GetMyPatternData();
	if (Data)
	{
		RuntimeCooldown = Data->Cooldown;
		UE_LOG(LogTemp, Log, TEXT("[PatternBase:%s] UpdatePhaseSettings - Phase %d, Cooldown %.2f"), 
			*PatternId.ToString(), PhaseIndex, RuntimeCooldown);
	}
}

const FBossPatternDefinition* UCBossPatternBase::GetMyPatternData() const
{
	if (!PhaseComponent.IsValid() || !PhaseComponent->PhaseData)
	{
		return nullptr;
	}
	
	const FBossPhaseDefinition* Phase = PhaseComponent->GetCurrentPhaseDefinition();
	if (!Phase)
	{
		return nullptr;
	}
	
	// 현재 페이즈의 패턴 목록에서 내 패턴 찾기
	for (const FBossPatternDefinition& Pattern : Phase->Patterns)
	{
		if (Pattern.PatternId == PatternId)
		{
			return &Pattern;
		}
	}
	
	return nullptr;
}

bool UCBossPatternBase::IsOnCooldown() const
{
	if (!PhaseComponent.IsValid())
	{
		return false;
	}
	
	// PhaseComponent의 쿨다운 맵에서 직접 확인
	const float* CooldownPtr = PhaseComponent->PatternCooldowns.Find(PatternId);
	if (CooldownPtr && *CooldownPtr > 0.f)
	{
		return true;
	}
	
	return false;
}

void UCBossPatternBase::StartCooldown()
{
	if (!PhaseComponent.IsValid())
	{
		return;
	}
	
	// DataAsset에서 쿨다운 값 읽어서 PhaseComponent 맵에 추가
	const FBossPatternDefinition* Data = GetMyPatternData();
	if (Data)
	{
		PhaseComponent->PatternCooldowns.Add(PatternId, Data->Cooldown);
		UE_LOG(LogTemp, Log, TEXT("[PatternBase:%s] Cooldown added to Phase map: %.2f"), 
			*PatternId.ToString(), Data->Cooldown);
	}
}

void UCBossPatternBase::FinishPattern(bool bApplyCooldown)
{
	OnPatternEnd();
	
	// ✅ 패턴이 성공적으로 끝났으면 쿨다운 적용
	if (bApplyCooldown)
	{
		StartCooldown();
		UE_LOG(LogTemp, Log, TEXT("[PatternBase:%s] Cooldown applied: %.2f seconds"), 
			*PatternId.ToString(), RuntimeCooldown);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PatternBase:%s] Pattern interrupted - No cooldown"), 
			*PatternId.ToString());
	}
	
	// PhaseComponent에 직접 알림 (Manager 우회)
	if (PhaseComponent.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PatternBase:%s] FinishPattern - Calling PhaseComponent->FinishPattern(bInterrupted=%s)"), 
			*PatternId.ToString(), bApplyCooldown ? TEXT("false") : TEXT("true"));
		
		PhaseComponent->FinishPattern(!bApplyCooldown);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PatternBase:%s] FinishPattern - PhaseComponent is invalid!"), 
			*PatternId.ToString());
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