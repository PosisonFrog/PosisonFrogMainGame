#include "00_Character/00_Player/00_Notify/CAnimNotifyState_ComboWindow.h"
#include "00_Character/02_Component/00_PlayerComponent/CPlayerWeaponComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "99_Util/CLog.h"

void UCAnimNotifyState_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!MeshComp) return;

	ACharacter* Char = MeshComp->GetOwner<ACharacter>();
	if (!Char) return;

	if (UCPlayerWeaponComponent* Weapon = Char->FindComponentByClass<UCPlayerWeaponComponent>())
	{
		Weapon->EnableComboInput();
		if (bDebugLog) CLog::Log(TEXT("[ComboWindow] Begin"));
	}
}

void UCAnimNotifyState_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (!MeshComp) return;

	ACharacter* Char = MeshComp->GetOwner<ACharacter>();
	if (!Char) return;

	if (UCPlayerWeaponComponent* Weapon = Char->FindComponentByClass<UCPlayerWeaponComponent>())
	{
		Weapon->DisableComboInput();
		if (bDebugLog) CLog::Log(TEXT("[ComboWindow] End"));
	}
}
/*애님 배치 “규약”(정확히 물리는 법)

각 콤보 단계의 애님 몽타주(또는 섹션)에

HitWindow(공격 판정) → UCAnimNotifyState_PlayerAttack(저희가 만든것)

ComboWindow(입력 유효 구간) → UAnimNotifyState_ComboWindow
를 구간으로 배치합니다.

플레이어가 공격 키를 누르면:

창이 닫혀 있으면 bQueuedNextInput=true로 저장

창 Begin 시 EnableComboInput()에서 큐가 있으면 즉시 다음 콤보로 Step

창이 닫히면 DisableComboInput()로 유효 입력을 막습니다.

몽타주가 끝나거나 인터럽트되면 OnMontageEnded에서 완전 리셋(히트창도 안전 OFF).

이 방식으로 “애님 타임라인 상의 정확한 구간”에만 다음 콤보 입력이 먹고,
창 밖에서 미리 눌렀더라도 자동 큐 처리로 타이밍 처리가 좋아짐니당*/