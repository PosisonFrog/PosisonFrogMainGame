// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/01_Widget/CUltimateSkillIconWidget.h"

#include "Components/ProgressBar.h"

void UCUltimateSkillIconWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UCUltimateSkillIconWidget::SetRatio(float Ratio)
{
	if (SkillBar)
		SkillBar->SetPercent(FMath::Clamp(Ratio, 0.0f, 1.0f));
}
