// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/01_Widget/CTimeCooldownSkillIconWidget.h"

#include "NiagaraFunctionLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Kismet/GameplayStatics.h"

void UCTimeCooldownSkillIconWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UCTimeCooldownSkillIconWidget::UpdateCoolDownUI(float CurrentTime, float MaxTime)
{
	if (!SkillIcon || !SkillBar)
		return;
	
	SkillBar->SetPercent(FMath::Clamp(CurrentTime, 0.f, MaxTime));

	// 이미지 색 변화를 최적화 하기 위해
	if (CurrentTime < 1.0f - KINDA_SMALL_NUMBER && !bIsCoolDown)
	{
		bIsCoolDown = true;
		SkillIcon->SetColorAndOpacity(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f));
	}
}

void UCTimeCooldownSkillIconWidget::FinishCoolDown()
{
	if (!SkillIcon || !SkillBar)
		return;
 	
	SkillBar->SetPercent(0.0f);
	bIsCoolDown = false;

	SkillIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
}
