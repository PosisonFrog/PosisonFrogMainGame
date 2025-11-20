#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "CStageTriggerVolume.generated.h"

class ACStageManager;
class ACPlayerCharacter;

/**
 * StageManager로만 신호를 보내는 전용 트리거 박스
 */
UCLASS()
class POSISONFROG_API ACStageTriggerVolume : public ATriggerBox
{
	GENERATED_BODY()
	
public:
	ACStageTriggerVolume();
	
protected:
	virtual void BeginPlay() override;
	
protected:
	UFUNCTION()
	void OnTriggerEnter(AActor* OverlappedActor, AActor* OtherActor);
	
protected:
	UPROPERTY(EditAnywhere, Category = "StageTrigger")
	FName TriggerTag;
	
	UPROPERTY(EditAnywhere, Category = "StageTrigger")
	TObjectPtr<ACStageManager> StageManager;
};

