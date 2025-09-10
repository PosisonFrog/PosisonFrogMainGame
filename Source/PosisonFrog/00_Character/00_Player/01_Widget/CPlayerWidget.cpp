// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/01_Widget/CPlayerWidget.h"

#include "CPlayerHpBarWidget.h"

void UCPlayerWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UCPlayerWidget::UpdateHpBar(float CurrentHp, float MaxHp)
{
	if (WBP_PlayerHpBar)
		WBP_PlayerHpBar->UpdateHp(CurrentHp, MaxHp);
}
