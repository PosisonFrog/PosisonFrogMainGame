// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/01_Widget/CSkillUIWidget.h"

#include "00_Character/00_Player/CPlayerCharacter.h"
#include "Components/ProgressBar.h"
#include "GameFramework/Character.h"

void UCSkillUIWidget::NativeConstruct()
{
	Super::NativeConstruct();

}

void UCSkillUIWidget::UpdateCoolDownUI(float CurrentTime, float MaxTime)
{
	if (CoolTimeBar)
	{
		CoolTimeBar->SetPercent(FMath::Clamp(CurrentTime, 0.f, 1.f));
	}
}

void UCSkillUIWidget::FinishCoolDown()
{
	if (!SkillICon || !CoolTimeBar)
		return;

	CoolTimeBar->SetPercent(0.0f);

	// 이미지 변화 및 이펙트, 애니메이션이 출력되는 코드가 필요함
	
	bIsCoolDown = false;
}
