// CMainGameModeBase.cpp

#include "CMainGameModeBase.h"
#include "00_Character/00_Player/CPlayerController.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "01_Item/CHealOrb.h"
#include "01_Item/CHealOrbPoolSubsystem.h"
#include "99_Util/CLog.h"

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
		Pool->SetOrbClass(HealOrbClass);

		Pool->Prewarm(GetWorld(), 20);
		
		CLog::Log(TEXT("ACMainGameModeBase - HealOrb Pool 초기화 완료"));
	}
}
