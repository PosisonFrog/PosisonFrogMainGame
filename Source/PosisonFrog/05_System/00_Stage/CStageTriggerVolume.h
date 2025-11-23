#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "CStageTriggerVolume.generated.h"

class ACStageManager;
class ACPlayerCharacter;
class UCCutsceneWidget;
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

	UFUNCTION()
	void OnImageCutsceneFinished();
	void StartMiddleCutscene();
	
protected:
	UPROPERTY(EditAnywhere, Category = "StageTrigger")
	FName TriggerTag;
	
	UPROPERTY(EditAnywhere, Category = "StageTrigger")
	TObjectPtr<ACStageManager> StageManager;

	UPROPERTY(EditAnywhere, Category = "StageTrigger|ClearWidget")
	TSubclassOf<UCCutsceneWidget> ImageCutsceneWidgetClass;
    
	// 이미지 컷신 위젯 인스턴스
	UPROPERTY()
	TObjectPtr<UCCutsceneWidget> ImageCutsceneWidget;

	FTimerHandle TimerHandle_CutsceneStart;

private:
	bool bHasTriggered = false;
};

