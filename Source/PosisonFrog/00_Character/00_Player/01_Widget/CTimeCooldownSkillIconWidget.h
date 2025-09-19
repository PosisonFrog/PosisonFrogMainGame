// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "00_Character/00_Player/01_Widget/CSkillIconBaseWidget.h"
#include "CTimeCooldownSkillIconWidget.generated.h"

/**
 * 
 */
UCLASS()
class POSISONFROG_API UCTimeCooldownSkillIconWidget : public UCSkillIconBaseWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	
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
