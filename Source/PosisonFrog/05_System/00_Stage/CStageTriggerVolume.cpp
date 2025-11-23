
#include "CStageTriggerVolume.h"

#include "CStageManager.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "02_MainMenu/01_Widget/CCutsceneWidget.h"
#include "05_System/CPauseSubsystem.h"
#include "Kismet/GameplayStatics.h"

ACStageTriggerVolume::ACStageTriggerVolume()
{
	OnActorBeginOverlap.AddDynamic(this, &ACStageTriggerVolume::OnTriggerEnter);
}

void ACStageTriggerVolume::BeginPlay()
{
	Super::BeginPlay();

	bHasTriggered = false;
	
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
	if (bHasTriggered)
		return;
	
	if (!IsValid(StageManager))
		return;
	
	if (!OtherActor || !OtherActor->IsA<ACPlayerCharacter>())
		return;

	if (StageManager->GetCurrentStage() == 7)
	{
		StartMiddleCutscene();
	}

	bHasTriggered = true;
	StageManager->HandleTrigger(TriggerTag);
	// SetActorEnableCollision(false);
}

void ACStageTriggerVolume::StartMiddleCutscene()
{
	UE_LOG(LogTemp, Log, TEXT("[StageTrigger] Starting Middle Cutscene..."));

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	UCPauseSubsystem* PauseSubsystem = GetGameInstance()->GetSubsystem<UCPauseSubsystem>();

	// 1. 위젯 생성
	if (ImageCutsceneWidgetClass && PC)
	{
		ImageCutsceneWidget = CreateWidget<UCCutsceneWidget>(PC, ImageCutsceneWidgetClass);
		if (ImageCutsceneWidget)
		{
			ImageCutsceneWidget->OnCutsceneFinished.AddDynamic(this, &ACStageTriggerVolume::OnImageCutsceneFinished);
			
			if (PauseSubsystem)
			{
				PauseSubsystem->RequestPause(PC);   // 게임 시간 정지
				PauseSubsystem->SetCanPause(false); // ESC 메뉴 호출 방지
			}
			
			FInputModeUIOnly InputMode;
			PC->SetInputMode(InputMode);
			InputMode.SetWidgetToFocus(ImageCutsceneWidget->TakeWidget()); 
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetShowMouseCursor(true);
			
			ImageCutsceneWidget->AddToViewport(200);
			ImageCutsceneWidget->InitializeAndStart();
		}
		else
		{
			// 위젯 생성 실패 시 바로 종료 처리
			OnImageCutsceneFinished();
			return; 
		}
	}
	else
	{
		OnImageCutsceneFinished();
		return;
	}
}

void ACStageTriggerVolume::OnImageCutsceneFinished()
{
	UE_LOG(LogTemp, Log, TEXT("[StageTrigger] Cutscene Finished -> "));
    
	// 1. 위젯 제거
	if (ImageCutsceneWidget)
	{
		ImageCutsceneWidget->RemoveFromParent();
		ImageCutsceneWidget = nullptr;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	UCPauseSubsystem* PauseSubsystem = GetGameInstance()->GetSubsystem<UCPauseSubsystem>();
	
		// [입력 모드 복구] 게임 전용 모드
	FInputModeGameOnly InputMode;
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(false); // 마우스 커서 숨김

		// 2. 시스템 재개
	if (PauseSubsystem)
	{
		PauseSubsystem->SetCanPause(true); // 퍼즈 기능 허용
		PauseSubsystem->RequestResume(PC); // 게임 시간 재개
	}
	
}



