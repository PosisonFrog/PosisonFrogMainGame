#include "AnimNotify_PatternEnd.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CBossPatternManager.h"

void UAnimNotify_PatternEnd::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	// Boss Character 가져오기
	ACEnemyBossCharacter* BossCharacter = Cast<ACEnemyBossCharacter>(MeshComp->GetOwner());
	if (!BossCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_PatternEnd] Owner is not CEnemyBossCharacter"));
		return;
	}

	// PatternManager 가져오기
	UCBossPatternManager* PatternManager = BossCharacter->FindComponentByClass<UCBossPatternManager>();
	if (!PatternManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[AnimNotify_PatternEnd] PatternManager not found!"));
		return;
	}

	// 패턴 종료 알림
	UE_LOG(LogTemp, Log, TEXT("[AnimNotify_PatternEnd] Notifying pattern end: %s"), bSuccess ? TEXT("Success") : TEXT("Failed"));
	PatternManager->NotifyCurrentPatternEnd(bSuccess);
}