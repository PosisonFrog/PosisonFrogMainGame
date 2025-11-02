#include "AnimNotify_StartRush.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/CBossPatternManager.h"

void UAnimNotify_StartRush::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::Notify(MeshComp, Animation);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	// Boss Character 가져오기
	ACEnemyBossCharacter* BossCharacter = Cast<ACEnemyBossCharacter>(MeshComp->GetOwner());
	if (!BossCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_StartRush] Owner is not CEnemyBossCharacter"));
		return;
	}

	// PatternManager 가져오기
	UCBossPatternManager* PatternManager = BossCharacter->FindComponentByClass<UCBossPatternManager>();
	if (!PatternManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[AnimNotify_StartRush] PatternManager not found!"));
		return;
	}

	// Rush 이동 시작
	UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_StartRush] Notifying PatternManager to start rush movement"));
	PatternManager->HandleRushMovementStart();
}