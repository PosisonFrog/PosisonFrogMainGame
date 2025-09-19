// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "00_Character/00_Player/01_Widget/CSkillIconBaseWidget.h"
#include "CUltimateSkillIconWidget.generated.h"

/**
 * 
 */
UCLASS()
class POSISONFROG_API UCUltimateSkillIconWidget : public UCSkillIconBaseWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	
public:
	void SetRatio(float Ratio);
};
