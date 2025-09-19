// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "00_Character/00_Player/01_Widget/CSkillIconBaseWidget.h"
#include "CUltimateSkillIconWidget.generated.h"

class UProgressBar;

/**
 * 
 */
UCLASS()
class POSISONFROG_API UCUltimateSkillIconWidget : public UCSkillIconBaseWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> Stack_1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> Stack_2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> Stack_3;
	
public:
	void SetUltimateUI(float Ratio, int32 UltimateStack);

private:
	TArray<UProgressBar*> StackBars;
	static constexpr int32 MaxStacks = 3;
};
