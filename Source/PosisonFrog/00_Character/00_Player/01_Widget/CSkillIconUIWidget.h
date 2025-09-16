// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CSkillIconUIWidget.generated.h"

class UNiagaraSystem;
class UImage;
class UProgressBar;

/**
 * 
 */
UCLASS()
class POSISONFROG_API UCSkillIconUIWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	
private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> SkillICon;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> CoolTimeBar;

	// 애니메이션 변수 / 흐으으음..... 필요하다면?????
	
	// SFX(사운드), VFX(이펙트) 변수
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	USoundBase* SFX_CoolTimeFinished = nullptr;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	USoundBase* SFX_CoolTimeBlocked = nullptr;

	// 쿨타임 완료되면 플레이어한테도 뭔가 이펙트 출력 괜찮다고 생각해서 한번 작성해봅니다.
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	UNiagaraSystem* VFX_CoolTimeFinishedOnPlayer = nullptr;

	// 생각해보니 UI 위에 이펙트 출력 관련으로 동인님한테 물어보고 싶은게 있어요!
	
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
