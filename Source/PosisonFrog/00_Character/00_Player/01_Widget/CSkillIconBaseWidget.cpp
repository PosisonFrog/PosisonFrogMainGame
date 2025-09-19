// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/01_Widget/CSkillIconBaseWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"

void UCSkillIconBaseWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SkillBar)
	{
		SkillBar->SetPercent(0.0f);
	}
}
