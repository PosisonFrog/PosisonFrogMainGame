// CMainGameModeBase.cpp

#include "CMainGameModeBase.h"
#include "00_Character/00_Player/CPlayerController.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "01_Item/CHealOrb.h"
#include "01_Item/CHealOrbPoolSubsystem.h"
#include "99_Util/CLog.h"
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
			CLog::Log(TEXT("ACMainGameModeBase - ERROR: HealOrbClass is not set!"));  // ✅ 에러 로그
		}
	}
}

void ACMainGameModeBase::OnPlayerDeath(ACPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		CLog::Log(TEXT("[GameMode] OnPlayerDeath - Invalid PlayerController"));
		return;
	}

	CLog::Log(FString::Printf(TEXT("[GameMode] PlayerDeath - Returning to main menu in %.1f seconds"), DeathReturnDelay));

	if (UCHealOrbPoolSubsystem* Pool = GetGameInstance()->GetSubsystem<UCHealOrbPoolSubsystem>())
	{
		Pool->ClearPool();
		CLog::Log(TEXT("[GameMode] HealOrb Pool cleared"));
	}
	
	// 페이드아웃 효과
	if (APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
	{
		CameraManager->StartCameraFade(0.0f, 1.0f, DeathReturnDelay, FLinearColor::Black);
	}

	GetWorldTimerManager().SetTimer(
		TimerHandle_ReturnToMenu,
		this,
		&ACMainGameModeBase::ReturnToMenu,
		DeathReturnDelay,
		false);
}

void ACMainGameModeBase::ReturnToMenu()
{
	CLog::Log(TEXT("[GameMode] Returning to Main Menu"));

	GetWorldTimerManager().ClearAllTimersForObject(this);
	
	UGameplayStatics::OpenLevel(this, MainMenuLevelName);
}
