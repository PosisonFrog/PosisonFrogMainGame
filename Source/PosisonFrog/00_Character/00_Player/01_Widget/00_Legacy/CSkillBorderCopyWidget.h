// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CSkillBorderCopyWidget.generated.h"

class UImage;
class UTexture2D;
class UTextureRenderTarget2D;

/**
 * 
 */
UCLASS()
class POSISONFROG_API UCSkillBorderCopyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SkillBorder")
	UTexture2D* SourceTexture = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	UImage* PreviewImage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SkillBorder")
	FIntPoint OutputSize = FIntPoint(0,0);

	UFUNCTION(BlueprintCallable, Category="SkillBorder")
	void SetSourceTexture(UTexture2D* InTex);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	UTextureRenderTarget2D* OutputRT = nullptr;

	bool bHasInitializedInGame = false;
	
	void EnsureRT();
	void DispatchOnce();
	void ApplyBrush();
};
