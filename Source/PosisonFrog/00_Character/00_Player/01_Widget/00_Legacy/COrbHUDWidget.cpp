// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/01_Widget/00_Legacy/COrbHUDWidget.h"
#include "Components/TextBlock.h"


void UCOrbHUDWidget::UpdateCounters(int32 ActiveOrbs, int32 TotalPicked)
{
	if (Text_ActiveOrbs)
		Text_ActiveOrbs->SetText(FText::FromString(FString::Printf(TEXT("Active Orbs : %d"), ActiveOrbs)));

	if (Text_TotalPicked)
		Text_TotalPicked->SetText(FText::FromString(FString::Printf(TEXT("Picked : %d"), TotalPicked)));
}

void UCOrbHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateCounters(0,0);
}
