// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/01_Widget/CUltimateSkillIconWidget.h"

#include "Components/ProgressBar.h"

void UCUltimateSkillIconWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Stack_1)
	{
		Stack_1->SetPercent(0.0f);
		StackBars.Add(Stack_1);
	}
	if (Stack_2)
	{
		Stack_2->SetPercent(0.0f);
		StackBars.Add(Stack_2);
	}
	if (Stack_3)
	{
		Stack_3->SetPercent(0.0f);
		StackBars.Add(Stack_3);
	}
}

void UCUltimateSkillIconWidget::SetUltimateUI(float Ratio, int32 UltimateStack)
{
	if (StackBars.Num() == 0)
		return;

	for (int32 i = 0; i < StackBars.Num(); i++)
	{
		if (!StackBars[i])
			return;

		if (i < UltimateStack)
		{
			StackBars[i]->SetPercent(1.0f);
		}
		else
		{
			StackBars[i]->SetPercent(0.0f);
		}
	}
	
	if (SkillBar)
		SkillBar->SetPercent(FMath::Clamp(Ratio, 0.0f, 1.0f));
}
