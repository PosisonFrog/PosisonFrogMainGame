// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CPlayerWidget.generated.h"

class UCPlayerHpBarWidget;
/**
 * 
 */
UCLASS()
class POSISONFROG_API UCPlayerWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta=(BindWidget))
	TObjectPtr<UCPlayerHpBarWidget> WBP_PlayerHpBar;

public:
	UFUNCTION(BlueprintCallable)
	void UpdateHpBar(float CurrentHp, float MaxHp);
};
