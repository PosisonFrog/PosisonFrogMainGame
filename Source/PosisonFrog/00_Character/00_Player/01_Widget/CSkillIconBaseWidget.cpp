// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/01_Widget/CSkillIconBaseWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Kismet/GameplayStatics.h"

void UCSkillIconBaseWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SkillBar)
	{
		SkillBar->SetPercent(0.0f);
	}
}

void UCSkillIconBaseWidget::PlaySkillReadySound()
{
	if (SFX_SkillReady)
	{
		UGameplayStatics::PlaySound2D(this, SFX_SkillReady);
	}
}

void UCSkillIconBaseWidget::PlayCoolTimeBlockedSound()
{
	if (SFX_CoolTimeBlocked)
	{
		UGameplayStatics::PlaySound2D(this, SFX_CoolTimeBlocked);
	}
}

void UCSkillIconBaseWidget::PlaySkillReadyAnim()
{
	
}

void UCSkillIconBaseWidget::PlayCoolTimeBlockAnim()
{
	
}

void UCSkillIconBaseWidget::PlaySkillReadyEffect()
{
	// 공통 연출 추가
	// 간단하게 플레이어 발밑에 한번 스폰하게....?
	/*if (VFX_CoolTimeFinishedOnPlayer)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(
					GetWorld(),
					VFX_CoolTimeFinishedOnPlayer,
					Pawn->GetActorLocation(),
					Pawn->GetActorRotation());
			}
		}
	}*/
}

void UCSkillIconBaseWidget::PlayCoolTimeBlockedEffect()
{
	// 공통 연출 추가
}
