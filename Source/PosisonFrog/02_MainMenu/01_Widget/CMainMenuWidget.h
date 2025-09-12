// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Templates/Function.h"
#include "CMainMenuWidget.generated.h"

class UButton;
class UNiagaraSystem;
/**
 * 
 */
UCLASS()
class POSISONFROG_API UCMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	
	// ==== UI 바인드 ====
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButton> MainMenu_StartButton;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButton> MainMenu_SettingButton;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButton> MainMenu_ExitButton;

	UFUNCTION()
	void MainMenu_StartButtonReleasedHandle();

	UFUNCTION()
	void MainMenu_SettingButtonReleasedHandle();

	UFUNCTION()
	void MainMenu_ExitButtonReleasedHandle();
	
	// ==== UI 애니메이션 ====
	// UMG에서 애니메이션 추가만 해도 코드가 자동으로 재생되게 설정 해봤습니다.
protected:
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	UWidgetAnimation* Anim_Focus;
	
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	UWidgetAnimation* Anim_Hover;
	
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	UWidgetAnimation* Anim_Click;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	UWidgetAnimation* Anim_FadeOut;

	UFUNCTION()
	void OnAnyButtonHovered();
	
	UFUNCTION()
	void OnAnyButtonPressed();

	// ==== UI SFX ====
protected:
	UPROPERTY(EditDefaultsOnly, Category = "SFX")
	USoundBase* SFX_Hover = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "SFX")
	USoundBase* SFX_Click = nullptr;

	void PlayUISound(USoundBase* SFX);

	// ==== 나이아가라(파티클) ====
	// 사용한다면 SpawnClickVFX에 따로 구현 예정
protected:
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	UNiagaraSystem* VFX_Click = nullptr;

	UFUNCTION()
	void SpawnClickVFX(FVector2D ScreenPos);

	// ==== 중복 클릭 방지 (뭔가 있어야할거 같은데....) ====
	// 이건 동인 주인님 도우ㅜ우우우우우움
protected:
	bool bInputLocked = false;
	
};
