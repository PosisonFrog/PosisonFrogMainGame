#include "AnimNotify_StopRush.h"
#include "00_Character/01_Enemy/CEnemyBossCharacter.h"
#include "00_Character/02_Component/01_EnemyComponent/00_BossPattern/CBossPattern_Rush.h"

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
		UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_StopRush] Owner is not CEnemyBossCharacter"));
		return;
	}

	// Rush 패턴 컴포넌트 직접 가져오기 (Tag 기반)
	TArray<UActorComponent*> RushComponents = BossCharacter->GetComponentsByTag(UCBossPattern_Rush::StaticClass(), FName("Rush"));
	
	if (RushComponents.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[AnimNotify_StopRush] Rush pattern component not found!"));
		return;
	}

	UCBossPattern_Rush* RushPattern = Cast<UCBossPattern_Rush>(RushComponents[0]);
	if (!RushPattern)
	{
		UE_LOG(LogTemp, Error, TEXT("[AnimNotify_StopRush] Failed to cast to Rush pattern!"));
		return;
	}

	// Rush 이동 종료 - HandleRushMovementStop은 Recovery로 전환
	UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_StopRush] Calling HandleRushMovementStop on Rush pattern"));
	RushPattern->HandleRushMovementStop();
}
