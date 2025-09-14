// CMainGameModeBase.cpp

#include "CMainGameModeBase.h"
#include "00_Character/00_Player/CPlayerController.h"
#include "00_Character/00_Player/CPlayerCharacter.h"

ACMainGameModeBase::ACMainGameModeBase()
{
	PlayerControllerClass = ACPlayerController::StaticClass();   // 올바른 컨트롤러 지정
	DefaultPawnClass = ACPlayerCharacter::StaticClass();    //플레이어 폰 지정

	// (선택) HUDClass = AYourHUD::StaticClass();
	// (선택) bUseSeamlessTravel = false;
}
