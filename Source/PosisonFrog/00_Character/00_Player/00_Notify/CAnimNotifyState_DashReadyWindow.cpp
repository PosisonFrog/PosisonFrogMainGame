#include "CAnimNotifyState_DashReadyWindow.h"
#include "00_Character/00_Player/CPlayerCharacter.h"

void UCAnimNotifyState_DashReadyWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase*, float,
	const FAnimNotifyEventReference&)
{
	if (!MeshComp) return;
	if (ACPlayerCharacter* PC = Cast<ACPlayerCharacter>(MeshComp->GetOwner()))
	{
		// 윈도우 시작 시 버퍼 소비(같은 프레임 즉발)
		PC->OnAttackDashReady();
	}
}

void UCAnimNotifyState_DashReadyWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase*,
	const FAnimNotifyEventReference&)
{
	if (!MeshComp) return;
	if (ACPlayerCharacter* PC = Cast<ACPlayerCharacter>(MeshComp->GetOwner()))
	{
		// 프레임 드랍/블렌드 변동으로 Begin을 놓친 경우 대비
		PC->OnAttackDashReady();
	}
}

/*C++만으로 동작

위 2개 파일을 추가 → 빌드.

애님 몽타주의 Attack1/2 끝부분에 PF_DashReadyWindow를 0.06~0.12초 길이로 배치하는게 좋습니다(블루프린트 필요 없음).

CPlayerCharacter 쪽

이미 적용하신 DashStart()/TryCommitDash()/OnAttackStarted()/OnAttackEnded()/OnAttackDashReady()/버퍼·락·쿨다운 로직이 있으면 그대로 사용하시면 됩니다.

중요한 점은 어떤 경로(입력/노티/공격완료)로 오든 항상 TryCommitDash()를 통해 실행되도록 유지하는 것입니당.

UCWeaponComponent 콜백(권장)

공격 시작 시 PC->OnAttackStarted()

공격 종료 시 PC->OnAttackEnded()

(이미 구현하셨다면 그대로 유지)

틱 이슈가 있었다면

UCDashComponent::bUseLaunchMode = true로 설정하면 루트모션/프레임 드랍에 강하며 “같은 프레임 즉발” 체감이 좋아집니다.*/