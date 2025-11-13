// CMainGameModeBase.cpp

#include "CMainGameModeBase.h"
#include "00_Character/00_Player/CPlayerController.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "01_Enemy/CEnemyBossCharacter.h"
#include "01_Item/CHealOrb.h"
#include "01_Item/CHealOrbPoolSubsystem.h"
#include "02_Component/00_PlayerComponent/CFuryGaugeComponent.h"
#include "02_Component/00_PlayerComponent/CPlayerHealthComponent.h"
#include "05_System/CBossBattleStartTrigger.h"
#include "05_System/00_Stage/CCheckPoint.h"
#include "05_System/00_Stage/CStageManager.h"
#include "99_Util/CLog.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ACMainGameModeBase::ACMainGameModeBase()
{
	PlayerControllerClass = ACPlayerController::StaticClass();   // 올바른 컨트롤러 지정
	DefaultPawnClass = ACPlayerCharacter::StaticClass();    //플레이어 폰 지정

	// (선택) HUDClass = AYourHUD::StaticClass();
	// (선택) bUseSeamlessTravel = false;
}

void ACMainGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	if (UCHealOrbPoolSubsystem* Pool = GetGameInstance()->GetSubsystem<UCHealOrbPoolSubsystem>())
	{
		if (HealOrbClass)
		{
			Pool->SetOrbClass(HealOrbClass);
			CLog::Log(TEXT("ACMainGameModeBase - HealOrb Pool 초기화 완료"));
		}
		else
		{
			CLog::Log(TEXT("ACMainGameModeBase - ERROR: HealOrbClass is not set!"));
		}
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACStageManager::StaticClass(), FoundActors);
	if (FoundActors.Num() > 0)
	{
		if (ACStageManager* StageManager = Cast<ACStageManager>(FoundActors[0]))
		{
			StageManager->OnCheckPointActivated.AddDynamic(this, &ACMainGameModeBase::OnCheckPointActivateEvent);
		}
	}
}

void ACMainGameModeBase::OnCheckPointActivateEvent(ACCheckPoint* CheckPoint, ACPlayerCharacter* Player)
{
	if (!IsValid(CheckPoint) || !IsValid(Player))
	{
		CLog::Log(TEXT("[ACMainGameModeBase::OnCheckPointActivateEvent] Invalid Parameters"));
		return;
	}

	CurrentCheckPoint = CheckPoint;

	SavePlayerState(Player);
	StageSnapshot.ActivateStage = CheckPoint->SectionID;

	CLog::Log(FString::Printf(TEXT("[ACMainGameModeBase::OnCheckPointActivateEvent] 체크포인트 이벤트 받음 : %s (Section %d)"), *CheckPoint->GetName(), CheckPoint->SectionID));
}

void ACMainGameModeBase::RestartFromLastCheckpoint(ACPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		CLog::Log(TEXT("[ACMainGameModeBase::RestartFromLastCheckpoint] Invalid PlayerController"));
		return;
	}
	
	GetWorldTimerManager().ClearTimer(TimerHandle_Respawn);
	GetWorldTimerManager().ClearTimer(TimerHandle_ReturnToMenu);
	
	if (UCHealOrbPoolSubsystem* Pool = GetGameInstance()->GetSubsystem<UCHealOrbPoolSubsystem>())
	{
		Pool->ClearPool();
	}
	
	RespawnPlayerAtCheckPoint(PlayerController);
}

void ACMainGameModeBase::ReturnToTitleScreen()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_Respawn);
	GetWorldTimerManager().ClearTimer(TimerHandle_ReturnToMenu);
	
	if (MainMenuLevelName != NAME_None)
	{
		UGameplayStatics::OpenLevel(this, MainMenuLevelName);
	}
	else
	{
		CLog::Log(TEXT("[ACMainGameModeBase::ReturnToTitleScreen] MainMenuLevelName is not set."));
	}
}

void ACMainGameModeBase::OnPlayerDeath(ACPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		CLog::Log(TEXT("[GameMode] OnPlayerDeath - Invalid PlayerController"));
		return;
	}

	CLog::Log(TEXT("[ACMainGameModeBase::OnPlayerDeath] 플레이어 사망"));

	// HealOrb 풀 정리
	if (UCHealOrbPoolSubsystem* Pool = GetGameInstance()->GetSubsystem<UCHealOrbPoolSubsystem>())
	{
		Pool->ClearPool();
		CLog::Log(TEXT("[GameMode] HealOrb Pool cleared"));
	}

	// 체크포인트가 있다면 리스폰, 없으면 메인 매뉴로
	if (IsValid(CurrentCheckPoint) && CurrentCheckPoint->IsActivated())
	{
		CLog::Log(FString::Printf(TEXT("[ACMainGameModeBase::OnPlayerDeath] %.1f초 후 체크포인트에서 리스폰"), RespawnDelay));

		// 페이드아웃 효과
		if (APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
		{
			CameraManager->StartCameraFade(0.0f, 1.0f, RespawnDelay, FLinearColor::Black);
		}

		GetWorldTimerManager().SetTimer(
		TimerHandle_Respawn,
		[this, PlayerController]()
		{
			RespawnPlayerAtCheckPoint(PlayerController);
		},
		RespawnDelay,
		false);
	}
	else
	{
		// 체크포인트 없음 - 메인 메뉴로
		if (bReturnToMenuIfNoCheckPoint)
		{
			CLog::Log(FString::Printf(TEXT("[ACMainGameModeBase::OnPlayerDeath] 체크 포인트 없음. %.1f초 후 메인 메뉴로 이동"), DeathReturnDelay));

			// 페이드아웃 효과
			if (APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
			{
				CameraManager->StartCameraFade(0.0f, 1.0f, DeathReturnDelay, FLinearColor::Black);
			}

			GetWorldTimerManager().SetTimer(
			TimerHandle_Respawn,
			this,
			&ACMainGameModeBase::ReturnToMenu,
			DeathReturnDelay,
			false);
		}
		else
		{
			CLog::Log(TEXT("[ACMainGameModeBase::OnPlayerDeath] 체크포인트 없지만 리스폰 시도"));
			RespawnPlayerAtCheckPoint(PlayerController);
		}
	}
}

void ACMainGameModeBase::RespawnPlayerAtCheckPoint(ACPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		CLog::Log(TEXT("[ACMainGameModeBase::RespawnPlayerAtCheckPoint] InValid PlayerController"));
		return;
	}

	FTransform SpawnTransform = FTransform::Identity;

	if (IsValid(CurrentCheckPoint))
	{
		SpawnTransform.SetLocation(CurrentCheckPoint->GetRespawnLocation());
		SpawnTransform.SetRotation(CurrentCheckPoint->GetRespawnRotation().Quaternion());
	}
	else
	{
		if (AActor* PlayerStart = FindPlayerStart(PlayerController))
			SpawnTransform = PlayerStart->GetActorTransform();
	}

	if (APawn* OldPawn = PlayerController->GetPawn())
	{
		OldPawn->Destroy();
	}

	RestartPlayerAtTransform(PlayerController, SpawnTransform);

	if (ACPlayerCharacter* NewPlayer = Cast<ACPlayerCharacter>(PlayerController->GetPawn()))
	{
		NewPlayer->SetCanBeDamaged(true);

		if (UCapsuleComponent* Cap = NewPlayer->FindComponentByClass<UCapsuleComponent>())
		{
			Cap->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Cap->SetCollisionProfileName(TEXT("Pawn"));
		}

		if (UCharacterMovementComponent* Move = NewPlayer->FindComponentByClass<UCharacterMovementComponent>())
		{
			Move->StopMovementImmediately();
			Move->SetMovementMode(MOVE_Walking);
		}

		RestorePlayerState(NewPlayer);
		NewPlayer->EnableInput(PlayerController);
		RequestStageRespawn();
		ResetBossBattleState();

		if (APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
		{
			CameraManager->StartCameraFade(1.0f, 0.0f, 1.0f, FLinearColor::Black);
		}
	}
}

void ACMainGameModeBase::ReturnToMenu()
{
	CLog::Log(TEXT("[GameMode] Returning to Main Menu"));

	GetWorldTimerManager().ClearAllTimersForObject(this);
	
	UGameplayStatics::OpenLevel(this, MainMenuLevelName);
}

void ACMainGameModeBase::SavePlayerState(ACPlayerCharacter* Player)
{
	if (!IsValid(Player))
		return;

	// 체력 저장
	if (UCPlayerHealthComponent* HealthComp = Player->FindComponentByClass<UCPlayerHealthComponent>())
	{
		PlayerStateSnapshot.CurrentHealth = HealthComp->GetHealth();
		PlayerStateSnapshot.MaxHealth = HealthComp->GetMaxHealth();
	}

	// Fury 게이지 저장
	if (UCFuryGaugeComponent* FuryComp = Player->FindComponentByClass<UCFuryGaugeComponent>())
		PlayerStateSnapshot.FuryGauge = FuryComp->GetCurrentFury();
	
	// 궁극기 게이지 저장 
	PlayerStateSnapshot.UltimateGauge = Player->GetUltimateGauge();

	if (IsValid(CurrentCheckPoint))
	{
		PlayerStateSnapshot.CheckPointLocation = CurrentCheckPoint->GetRespawnLocation();
		PlayerStateSnapshot.CheckPointRotation = CurrentCheckPoint->GetRespawnRotation();
	}

	bHasValidSnapshot = true;
}

void ACMainGameModeBase::RestorePlayerState(ACPlayerCharacter* Player)
{
	if (!IsValid(Player) || !bHasValidSnapshot)
		return;

	if (UCPlayerHealthComponent* HealthComp = Player->FindComponentByClass<UCPlayerHealthComponent>())
	{
		HealthComp->SetMaxHealth(PlayerStateSnapshot.MaxHealth, true, false);
		HealthComp->SetHealth(PlayerStateSnapshot.CurrentHealth);
	}

	if (UCFuryGaugeComponent* FuryComp = Player->FindComponentByClass<UCFuryGaugeComponent>())
	{
		FuryComp->SetFury(PlayerStateSnapshot.FuryGauge);
	}
	
	Player->SetUltimateGauge(PlayerStateSnapshot.UltimateGauge);
}

void ACMainGameModeBase::RequestStageRespawn()
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACStageManager::StaticClass(), FoundActors);
	
	if (FoundActors.Num() > 0)
	{
		if (ACStageManager* StageManager = Cast<ACStageManager>(FoundActors[0]))
		{
			const int32 TargetStageID = StageManager->GetCurrentStage();
			StageManager->PrepareForRespawn(TargetStageID);
			CLog::Log(FString::Printf(TEXT("[ACMainGameModeBase::RequestStageRespawn] Stage %d 리스폰 요청"), StageSnapshot.ActivateStage));
		}
	}
}

void ACMainGameModeBase::ResetBossBattleState()
{
	UWorld* World = GetWorld();
	if (!World)
		return;

	TArray<AActor*> BossCharacters;
	UGameplayStatics::GetAllActorsOfClass(World, ACEnemyBossCharacter::StaticClass(), BossCharacters);
	for (AActor* Actor : BossCharacters)
	{
		if (ACEnemyBossCharacter* BossCharacter = Cast<ACEnemyBossCharacter>(Actor))
		{
			BossCharacter->ResetBossBattleState();
		}
	}
	
	TArray<AActor*> StageManagers;
	UGameplayStatics::GetAllActorsOfClass(World, ACStageManager::StaticClass(), StageManagers);
	for (AActor* Actor : StageManagers)
	{
		if (ACStageManager* StageManager = Cast<ACStageManager>(Actor))
		{
			StageManager->ResetBossBattleForRespawn();
		}
	}
	
	TArray<AActor*> BossTriggers;
	UGameplayStatics::GetAllActorsOfClass(World, ACBossBattleStartTrigger::StaticClass(), BossTriggers);
	for (AActor* Actor : BossTriggers)
	{
		if (ACBossBattleStartTrigger* Trigger = Cast<ACBossBattleStartTrigger>(Actor))
		{
			Trigger->ResetTrigger();
		}
	}
}
