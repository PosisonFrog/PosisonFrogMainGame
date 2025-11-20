
#include "CStageTriggerVolume.h"

#include "CStageManager.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "Kismet/GameplayStatics.h"

ACStageTriggerVolume::ACStageTriggerVolume()
{
	OnActorBeginOverlap.AddDynamic(this, &ACStageTriggerVolume::OnTriggerEnter);
}

void ACStageTriggerVolume::BeginPlay()
{
	Super::BeginPlay();
	
	if (!IsValid(StageManager))
	{
		TArray<AActor*> Managers;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACStageManager::StaticClass(), Managers);
		if (Managers.Num() > 0)
		{
			StageManager = Cast<ACStageManager>(Managers[0]);
		}
	}
}

void ACStageTriggerVolume::OnTriggerEnter(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!IsValid(StageManager))
		return;
	
	if (!OtherActor || !OtherActor->IsA<ACPlayerCharacter>())
		return;
	
	StageManager->HandleTrigger(TriggerTag);
}

