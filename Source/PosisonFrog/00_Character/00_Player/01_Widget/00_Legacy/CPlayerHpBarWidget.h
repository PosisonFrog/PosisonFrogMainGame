// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CPlayerHpBarWidget.generated.h"

class UProgressBar;

/**
 * 
 */
UCLASS()
class POSISONFROG_API UCPlayerHpBarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UProgressBar> HpBar;
	
public:
	void UpdateHp(float CurrentHp, float MaxHp) const;
};
