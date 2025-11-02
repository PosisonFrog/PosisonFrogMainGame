#include "AnimNotify_PatternEnd.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CBossPatternManager.h"

void UAnimNotify_PatternEnd::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::Notify(MeshComp, Animation);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	ACEnemyBossCharacter* BossCharacter = Cast<ACEnemyBossCharacter>(MeshComp->GetOwner());
	if (!BossCharacter)
	{
		return;
	}

	UCBossPatternManager* PatternManager = BossCharacter->FindComponentByClass<UCBossPatternManager>();
	if (!PatternManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[AnimNotify_PatternEnd] PatternManager not found!"));
		return;
	}

	// 패턴 종료 알림
	UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_PatternEnd] Notifying pattern end (Success: %s)"), 
		   bSuccess ? TEXT("True") : TEXT("False"));
	PatternManager->NotifyCurrentPatternEnd(bSuccess);
}
