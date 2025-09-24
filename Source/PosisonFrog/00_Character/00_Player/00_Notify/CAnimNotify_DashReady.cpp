#include "00_Character/00_Player/00_Notify/CAnimNotify_DashReady.h"
#include "00_Character/00_Player/CPlayerCharacter.h"

void UCAnimNotify_DashReady::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Animation*/)
{
	if (!MeshComp) return;
	if (ACPlayerCharacter* PC = Cast<ACPlayerCharacter>(MeshComp->GetOwner()))
	{
		PC->OnAttackDashReady(); // 버퍼가 있으면 같은 프레임에 즉시 대시
	}
}
