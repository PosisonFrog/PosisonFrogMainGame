#include "CAnimNotify_TankerChargeStart.h"
#include "00_Character/02_Component/01_EnemyComponent/CTankerChargeComponent.h"
#include "GameFramework/Character.h"

void UCAnimNotify_TankerChargeStart::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase*)
{
	if (!MeshComp) return;
	if (ACharacter* Ch = Cast<ACharacter>(MeshComp->GetOwner()))
		if (UCTankerChargeComponent* Comp = Ch->FindComponentByClass<UCTankerChargeComponent>())
			Comp->Anim_ChargeStart();
}
