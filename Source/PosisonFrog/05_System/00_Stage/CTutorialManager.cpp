#include "CTutorialManager.h"

#include "CTutorialPopupWidget.h"
#include "99_Util/CLog.h"
#include "Blueprint/UserWidget.h"

UCTutorialManager::UCTutorialManager()
{
}

void UCTutorialManager::Deinitialize()
{
	ActiveSteps.Empty();
	CurrentStepIndex = INDEX_NONE;
	CurrentCount = 0;
	bSequenceActive = false;
	bStepInProgress = false;

	Super::Deinitialize();
}

void UCTutorialManager::StartSequence(FName SequenceId, bool bStartFirstStep)
{
	if (!LoadSequence(SequenceId))
	{
		CLog::Log(FString::Printf(TEXT("[TutorialManager] Sequence '%s' 로드 실패 - DefaultSequence 사용"), *SequenceId.ToString()));
		ActiveSteps = DefaultSequence;
	}

	ActiveSequenceId = SequenceId;
	CurrentStepIndex = INDEX_NONE;
	CurrentCount = 0;
	bSequenceActive = ActiveSteps.Num() > 0;
	bStepInProgress = false;

	if (!bSequenceActive)
	{
		CLog::Log(TEXT("[TutorialManager] 시작할 스텝이 없습니다."));
		return;
	}

	OnTutorialSequenceStarted.Broadcast(SequenceId);

	if (bStartFirstStep)
	{
		BeginStep(0);
	}
}

void UCTutorialManager::StartSequenceWithSteps(const TArray<FTutorialStep>& Steps, bool bStartFirstStep)
{
	CLog::Log(TEXT("========================================"));
	CLog::Log(FString::Printf(TEXT("[TutorialManager] StartSequenceWithSteps: %d개 스텝"), Steps.Num()));
	CLog::Log(TEXT("========================================"));

	ActiveSteps = Steps;
	ActiveSequenceId = NAME_None;
	CurrentStepIndex = INDEX_NONE;
	CurrentCount = 0;
	bSequenceActive = ActiveSteps.Num() > 0;
	bStepInProgress = false;

	if (!bSequenceActive)
	{
		CLog::Log(TEXT("[TutorialManager] 빈 스텝 배열로 시퀀스 시작 시도"));
		return;
	}

	// ActiveSteps 출력
	for (int32 i = 0; i < ActiveSteps.Num(); ++i)
	{
		CLog::Log(FString::Printf(TEXT("[TutorialManager] Step[%d]: %s"), i, *ActiveSteps[i].StepId.ToString()));
	}

	OnTutorialSequenceStarted.Broadcast(NAME_None);

	if (bStartFirstStep)
	{
		BeginStep(0);
	}
	else
	{
		CLog::Log(TEXT("[TutorialManager] bStartFirstStep=false, 대기 중"));
	}
}

bool UCTutorialManager::RequestStartStepById(FName StepId)
{
	if (!bSequenceActive)
		return false;

	for (int32 Index = 0; Index < ActiveSteps.Num(); ++Index)
	{
		if (ActiveSteps[Index].StepId == StepId)
		{
			return RequestStartStepByIndex(Index);
		}
	}

	return false;
}

bool UCTutorialManager::RequestStartStepByIndex(int32 StepIndex)
{
	if (!bSequenceActive || !IsValidStepIndex(StepIndex))
		return false;

	if (bStepInProgress && CurrentStepIndex == StepIndex)
		return true;

	BeginStep(StepIndex);
	return true;
}

void UCTutorialManager::NotifyAction(ETutorialActionType ActionType)
{
	if (!bStepInProgress || !IsValidStepIndex(CurrentStepIndex))
		return;

	const FTutorialStep& CurrentStep = ActiveSteps[CurrentStepIndex];
	if (CurrentStep.RequiredAction != ActionType)
		return;

	CurrentCount++;
	if (CurrentCount >= CurrentStep.RequiredCount)
	{
		CompleteCurrentStep();
	}
}

void UCTutorialManager::SkipCurrentStep()
{
	if (!bStepInProgress)
		return;

	CompleteCurrentStep();
}

bool UCTutorialManager::IsInStep(FName StepId) const
{
	return bStepInProgress && IsValidStepIndex(CurrentStepIndex) && ActiveSteps[CurrentStepIndex].StepId == StepId;
}

FName UCTutorialManager::GetCurrentStepId() const
{
	if (IsValidStepIndex(CurrentStepIndex))
	{
		return ActiveSteps[CurrentStepIndex].StepId;
	}

	return NAME_None;
}

bool UCTutorialManager::IsActionAllowed(ETutorialActionType ActionType) const
{
	int32 RequiredLevel = 0;

	switch (ActionType)
	{
	case ETutorialActionType::None:             return true;
	case ETutorialActionType::BasicCombo_3Hit:  RequiredLevel = 1; break; 
	case ETutorialActionType::Dash_Used:        RequiredLevel = 2; break; 
	case ETutorialActionType::Command_Used:     RequiredLevel = 3; break; 
	case ETutorialActionType::Spin_Used:        RequiredLevel = 4; break; 
	case ETutorialActionType::Ult_Used:         RequiredLevel = 5; break; 
	default: return true;
	}

	// 현재 레벨이 요구 레벨보다 높거나 같으면 사용 가능
	return CurrentUnlockLevel >= RequiredLevel;
}

void UCTutorialManager::SetUnlockLevel(int32 NewLevel)
{
	if (NewLevel > CurrentUnlockLevel)
	{
		CurrentUnlockLevel = NewLevel;
		CLog::Log(FString::Printf(TEXT("[TutorialManager] 튜토리얼 레벨 강제 변경: Lv %d"), CurrentUnlockLevel));
	}
}


void UCTutorialManager::BeginStep(int32 StepIndex)
{
	if (!IsValidStepIndex(StepIndex))
	{
		CLog::Log(TEXT("[TutorialManager] BeginStep 실패 - 인덱스가 범위를 벗어났습니다."));
		return;
	}

	CurrentStepIndex = StepIndex;
	CurrentCount = 0;
	bStepInProgress = true;

	const FName StepId = ActiveSteps[CurrentStepIndex].StepId;

	// ===== 디버깅 로그 추가 =====
	CLog::Log(FString::Printf(TEXT("[TutorialManager] BeginStep: StepId=%s"), *StepId.ToString()));

	// 튜토리얼 팝업 표시
	ShowTutorialPopup(StepId);

	OnTutorialStepStarted.Broadcast(StepId);
}


void UCTutorialManager::SetTutorialPopupClass(TSubclassOf<UCTutorialPopupWidget> InPopupClass)
{
	TutorialPopupClass = InPopupClass;
}

void UCTutorialManager::ShowTutorialPopup(FName StepId)
{
	CLog::Log(FString::Printf(TEXT("[TutorialManager] ShowTutorialPopup 시작: StepId=%s"), *StepId.ToString()));

	if (!TutorialPopupClass)
	{
		CLog::Log(TEXT("[TutorialManager] ✗ TutorialPopupClass가 설정되지 않음!"));
		return;
	}
	
	CLog::Log(FString::Printf(TEXT("[TutorialManager] ✓ TutorialPopupClass: %s"), *TutorialPopupClass->GetName()));

	UWorld* World = GetWorld();
	if (!World)
	{
		CLog::Log(TEXT("[TutorialManager] ✗ World가 nullptr!"));
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		CLog::Log(TEXT("[TutorialManager] ✗ PlayerController를 찾을 수 없음!"));
		return;
	}

	CLog::Log(TEXT("[TutorialManager] ✓ PlayerController 발견"));

	// 기존 팝업이 있으면 제거
	if (CurrentPopupWidget)
	{
		CLog::Log(TEXT("[TutorialManager] 기존 팝업 제거"));
		CurrentPopupWidget->RemoveFromParent();
		CurrentPopupWidget = nullptr;
	}

	// 새 팝업 생성
	CLog::Log(TEXT("[TutorialManager] CreateWidget 시도..."));
	CurrentPopupWidget = CreateWidget<UCTutorialPopupWidget>(PC, TutorialPopupClass);
	
	if (CurrentPopupWidget)
	{
		CLog::Log(TEXT("[TutorialManager] ✓ Widget 생성 성공!"));
		CurrentPopupWidget->SetupTutorial(StepId);
		CurrentPopupWidget->AddToViewport(100);
		CLog::Log(TEXT("[TutorialManager] ✓ AddToViewport 완료!"));
	}
	else
	{
		CLog::Log(TEXT("[TutorialManager] ✗ CreateWidget 실패!"));
	}
}

void UCTutorialManager::CompleteCurrentStep()
{
	if (!IsValidStepIndex(CurrentStepIndex))
		return;

	const FName CompletedId = ActiveSteps[CurrentStepIndex].StepId;
	bStepInProgress = false;
	OnTutorialStepCompleted.Broadcast(CompletedId);

	const int32 NextIndex = CurrentStepIndex + 1;
	if (IsValidStepIndex(NextIndex))
	{
		const bool bAutoStartNext = ActiveSteps[NextIndex].StageTriggerTag.IsNone();
		CurrentStepIndex = NextIndex;
		CurrentCount = 0;
		if (bAutoStartNext)
		{
			BeginStep(NextIndex);
		}
	}
	else
	{
		bSequenceActive = false;
		CurrentStepIndex = INDEX_NONE;
		CurrentCount = 0;
		OnTutorialSequenceFinished.Broadcast(ActiveSequenceId);
	}
}

bool UCTutorialManager::LoadSequence(FName SequenceId)
{
	// DataTable 우선
	if (IsValid(TutorialDataTable))
	{
		static const FString Context = TEXT("TutorialSequence");
		TArray<FTutorialStep*> Rows;
		TutorialDataTable->GetAllRows(Context, Rows);

		ActiveSteps.Reset();
		for (FTutorialStep* Row : Rows)
		{
			if (Row)
			{
				ActiveSteps.Add(*Row);
			}
		}

		if (ActiveSteps.Num() > 0)
		{
			return true;
		}
	}

	// TMap에서 찾기 (FTutorialStepSequence 사용)
	if (const FTutorialStepSequence* SequencePtr = TutorialSequences.Find(SequenceId))
	{
		ActiveSteps = SequencePtr->Steps;
		return ActiveSteps.Num() > 0;
	}

	return false;
}

bool UCTutorialManager::IsValidStepIndex(int32 StepIndex) const
{
	return ActiveSteps.IsValidIndex(StepIndex);
}