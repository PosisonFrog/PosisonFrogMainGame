// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CTutorialManager.generated.h"

class UWorld;
class UCTutorialPopupWidget;

UENUM(BlueprintType)
enum class ETutorialActionType : uint8
{
	None,
	BasicCombo_3Hit,
	Dash_Used,
	Command_Used,
	Spin_Used,
	Ult_Used
};

USTRUCT(BlueprintType)
struct FTutorialStep : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FName StepId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	ETutorialActionType RequiredAction = ETutorialActionType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial", meta = (ClampMin = "0"))
	float OptionalTimeLimit = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FName TutorialWidgetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FName StageTriggerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	bool bLockOtherActions = false;
};

USTRUCT(BlueprintType)
struct FTutorialStepSequence
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	TArray<FTutorialStep> Steps;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTutorialStepEvent, FName, StepId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTutorialSequenceEvent, FName, SequenceId);

/**
 * 행동 카운터 + UI 브로드캐스트를 담당하는 튜토리얼 전용 매니저
 */
UCLASS()
class POSISONFROG_API UCTutorialManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UCTutorialManager();

	virtual void Deinitialize() override;
	void SetTutorialPopupClass(TSubclassOf<UCTutorialPopupWidget> InPopupClass);

public:
	// 튜토리얼 시퀀스 시작 (DataTable 또는 맵에서 로드)
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void StartSequence(FName SequenceId, bool bStartFirstStep = true);

	// 지정된 스텝 배열로 직접 시작
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void StartSequenceWithSteps(const TArray<FTutorialStep>& Steps, bool bStartFirstStep = true);

	// 현재 스텝을 강제 시작 (트리거나 StageManager 요청용)
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	bool RequestStartStepById(FName StepId);

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	bool RequestStartStepByIndex(int32 StepIndex);

	// 입력/전투 시스템에서 호출되는 카운터 이벤트
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void NotifyAction(ETutorialActionType ActionType);

	// 스킵/강제 완료
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void SkipCurrentStep();

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	bool IsInStep(FName StepId) const;

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	FName GetCurrentStepId() const;

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	bool IsActionAllowed(ETutorialActionType ActionType) const;

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void SetUnlockLevel(int32 NewLevel);
	
public: // Delegate
	UPROPERTY(BlueprintAssignable, Category = "Tutorial") FTutorialStepEvent OnTutorialStepStarted;
	UPROPERTY(BlueprintAssignable, Category = "Tutorial") FTutorialStepEvent OnTutorialStepCompleted;
	UPROPERTY(BlueprintAssignable, Category = "Tutorial") FTutorialSequenceEvent OnTutorialSequenceStarted;
	UPROPERTY(BlueprintAssignable, Category = "Tutorial") FTutorialSequenceEvent OnTutorialSequenceFinished;

protected:
	void BeginStep(int32 StepIndex);
	void CompleteCurrentStep();
	bool LoadSequence(FName SequenceId);
	bool IsValidStepIndex(int32 StepIndex) const;

private:
	void ShowTutorialPopup(FName StepId);

	UPROPERTY(EditAnywhere, Category = "Tutorial|UI")
	TSubclassOf<UCTutorialPopupWidget> TutorialPopupClass;

	UPROPERTY()
	TObjectPtr<UCTutorialPopupWidget> CurrentPopupWidget;
	
	UPROPERTY(EditAnywhere, Category = "Tutorial|Data")
	TArray<FTutorialStep> DefaultSequence;

	UPROPERTY(EditAnywhere, Category = "Tutorial|Data")
	TMap<FName, FTutorialStepSequence> TutorialSequences;

	// DataTable 기반 로딩도 지원 (테이블이 설정되어 있으면 우선)
	UPROPERTY(EditAnywhere, Category = "Tutorial|Data")
	TObjectPtr<UDataTable> TutorialDataTable;

	
	UPROPERTY()
	TArray<FTutorialStep> ActiveSteps;

	UPROPERTY()
	FName ActiveSequenceId;

	int32 CurrentStepIndex = INDEX_NONE;
	int32 CurrentCount = 0;
	int32 CurrentUnlockLevel = 1;
	bool bSequenceActive = false;
	bool bStepInProgress = false;
};