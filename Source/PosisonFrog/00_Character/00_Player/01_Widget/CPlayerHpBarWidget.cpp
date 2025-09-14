// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/01_Widget/CPlayerHpBarWidget.h"

#include "Components/ProgressBar.h"

void UCPlayerHpBarWidget::UpdateHp(float CurrentHp, float MaxHp) const
{
	const float Percent = (MaxHp > 0.0f) ? (CurrentHp / MaxHp) : 0.0f;

	if (HpBar)
		HpBar->SetPercent(Percent);
}
