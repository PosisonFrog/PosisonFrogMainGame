#include "CTutorialManager.h"

#include "99_Util/CLog.h"

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

	OnTutorialSequenceStarted.Broadcast(NAME_None);

	if (bStartFirstStep)
	{
		BeginStep(0);
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

	OnTutorialStepStarted.Broadcast(ActiveSteps[CurrentStepIndex].StepId);
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