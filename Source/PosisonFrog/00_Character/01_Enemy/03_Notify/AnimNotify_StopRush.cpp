#include "AnimNotify_StopRush.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CBossPatternManager.h"

void UAnimNotify_StopRush::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

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
		UE_LOG(LogTemp, Error, TEXT("[AnimNotify_StopRush] PatternManager not found!"));
		return;
	}

	// Rush 이동 종료
	UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_StopRush] Notifying PatternManager to stop rush movement"));
	PatternManager->HandleRushMovementStop();
}