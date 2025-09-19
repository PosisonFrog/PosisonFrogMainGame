// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CSkillIconBaseWidget.generated.h"

class UProgressBar;
class UImage;
/**
 * 
 */
UCLASS()
class POSISONFROG_API UCSkillIconBaseWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> SkillIcon = nullptr;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UProgressBar> SkillBar = nullptr;
};
