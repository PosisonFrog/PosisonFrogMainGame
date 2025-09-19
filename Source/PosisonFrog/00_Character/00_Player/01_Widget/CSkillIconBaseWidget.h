// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CSkillIconBaseWidget.generated.h"

class UNiagaraSystem;
class USoundBase;
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

	// SFX(사운드), VFX(이펙트) 변수
	UPROPERTY(EditDefaultsOnly, Category = "Skill|SFX")
	USoundBase* SFX_SkillReady = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Skill|SFX")
	USoundBase* SFX_CoolTimeBlocked = nullptr;

	// 애니메이션 변수
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Anim")
	UWidgetAnimation* Anim_SkillReady = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Skill|Anim")
	UWidgetAnimation* Anim_CoolTimeBlocked = nullptr;

	// 쿨타임 완료되면 플레이어한테도 뭔가 이펙트 출력 괜찮다고 생각해서 한번 작성해봅니다.
	UPROPERTY(EditDefaultsOnly, Category = "Skill|VFX")
	UNiagaraSystem* VFX_SkillReady = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Skill|VFX")
	UNiagaraSystem* VFX_CoolTimeBlocked = nullptr;

	// 사운드는 같을거 같아서 따로 부모 클래스에 재생되도록 변경
	void PlaySkillReadySound();
	void PlayCoolTimeBlockedSound();

	// 스킬 이펙트가 공통적으로 같거나 다른 연출이 있다면 override해서 구현
	// 애니메이션도 마찬가지
	virtual void PlaySkillReadyAnim();
	virtual void PlayCoolTimeBlockAnim();
	
	virtual void PlaySkillReadyEffect();
	virtual void PlayCoolTimeBlockedEffect();
};
