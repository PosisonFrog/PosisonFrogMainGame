// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "COrbHUDWidget.generated.h"

class UTextBlock;

/**
 * 
 */
UCLASS()
class POSISONFROG_API UCOrbHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void UpdateCounters(int32 ActiveOrbs, int32 TotalPicked);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_ActiveOrbs;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_TotalPicked;
};
