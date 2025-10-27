#include "CAnimNotify_TankerChargeStart.h"
#include "00_Character/02_Component/01_EnemyComponent/CTankerChargeComponent.h"
#include "GameFramework/Character.h"


void UCAnimNotify_TankerChargeStart::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::Notify(MeshComp, Animation);
	
	if (!MeshComp)
		return;
	
	
	ACharacter* Character = Cast<ACharacter>(MeshComp->GetOwner());
	if(!Character)
	 	return;
	 
	
	if (UCTankerChargeComponent* ChargeComponent = Character->FindComponentByClass<UCTankerChargeComponent>())
	{
		ChargeComponent->Anim_ChargeStart();
	}
}
