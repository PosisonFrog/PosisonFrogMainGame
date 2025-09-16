// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CSkillUIWidget.generated.h"

class UImage;
class UProgressBar;

/**
 * 
 */
UCLASS()
class POSISONFROG_API UCSkillUIWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	
private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> SkillICon;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> CoolTimeBar;

	// 애니메이션 및 이펙트, 사운드 변수 필요

public:
	// 쿨타임이 진행중 Bar가 업데이트 되는 코드가 필요
	UFUNCTION()
	void UpdateCoolDownUI(float CurrentTime, float MaxTime);

	// 쿨타임이 완료 되었을 때 Bar 및 이미지 옵션 변경
	void FinishCoolDown();
	
protected:
	// 지금 쿨타임 상태인지 아닌지 확인용 bool 타입 선언
	bool bIsCoolDown = false;
};
