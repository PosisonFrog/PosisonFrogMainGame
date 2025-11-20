// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/00_Notify/CAnimNotify_PlayEffectByUltState.h"

#include "00_Character/00_Player/CPlayerCharacter.h"
#include "00_Character/00_Player/02_Weapon/CHammer.h"
#include "99_Util/CLog.h"

UCAnimNotify_PlayEffectByUltState::UCAnimNotify_PlayEffectByUltState()
{
	// 기본값 초기화
	NormalEffect.ScaleOffset = FVector(1.0f, 1.0f, 1.0f);
	UltimateEffect.ScaleOffset = FVector(1.0f, 1.0f, 1.0f);
}

void UCAnimNotify_PlayEffectByUltState::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp)
	{
		return;
	}

	// 부모 클래스의 원래 설정 백업
	FParentSettings OriginalSettings = BackupParentSettings();

	// 궁극기 상태 확인 후 적절한 이펙트 설정 적용
	const FEffectSettings& SelectedEffect = IsPlayerUltimateActive(MeshComp) ? UltimateEffect : NormalEffect;
	
	// 선택된 이펙트가 유효한지 확인
	if (!SelectedEffect.EffectTemplate)
	{
		CLog::Log(TEXT("PlayEffectByUltState: 선택된 이펙트 템플릿이 없습니다."));
		return;
	}

	// 선택된 이펙트 설정을 부모 클래스에 적용
	ApplyEffectSettings(SelectedEffect);

	// 부모 클래스의 Notify 호출 (실제 이펙트 스폰)
	Super::Notify(MeshComp, Animation, EventReference);

	// 부모 클래스 설정 복원
	RestoreParentSettings(OriginalSettings);
}

bool UCAnimNotify_PlayEffectByUltState::IsPlayerUltimateActive(USkeletalMeshComponent* MeshComp) const
{
	if (!MeshComp)
	{
		return false;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return false;
	}

	// 플레이어 캐릭터 확인
	if (ACPlayerCharacter* Player = Cast<ACPlayerCharacter>(Owner))
	{
		return Player->IsBuffActive();
	}

	// Hammer 메시인 경우 플레이어를 찾아서 확인
	if (ACHammer* Hammer = Cast<ACHammer>(Owner))
	{
		if (ACPlayerCharacter* Player = Cast<ACPlayerCharacter>(Hammer->GetOwner()))
		{
			return Player->IsBuffActive();
		}
	}

	return false;
}

UCAnimNotify_PlayEffectByUltState::FParentSettings UCAnimNotify_PlayEffectByUltState::BackupParentSettings() const
{
	FParentSettings Settings;
	Settings.Template = Template;
	Settings.LocationOffset = LocationOffset;
	Settings.RotationOffset = RotationOffset;
	Settings.ScaleOffset = Scale;
	return Settings;
}

void UCAnimNotify_PlayEffectByUltState::RestoreParentSettings(const FParentSettings& Settings)
{
	Template = Settings.Template;
	LocationOffset = Settings.LocationOffset;
	RotationOffset = Settings.RotationOffset;
	Scale = Settings.ScaleOffset;
}

void UCAnimNotify_PlayEffectByUltState::ApplyEffectSettings(const FEffectSettings& Settings)
{
	Template = Settings.EffectTemplate;
	LocationOffset = Settings.LocationOffset;
	RotationOffset = Settings.RotationOffset;
	Scale = Settings.ScaleOffset;
}
