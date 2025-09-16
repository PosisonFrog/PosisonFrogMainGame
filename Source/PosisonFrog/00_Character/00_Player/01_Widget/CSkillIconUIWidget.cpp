// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/00_Player/01_Widget/CSkillIconUIWidget.h"

#include "NiagaraFunctionLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Kismet/GameplayStatics.h"

void UCSkillIconUIWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UCSkillIconUIWidget::UpdateCoolDownUI(float CurrentTime, float MaxTime)
{
	if (!SkillICon || !CoolTimeBar)
		return;
	
	CoolTimeBar->SetPercent(FMath::Clamp(CurrentTime, 0.f, MaxTime));

	// 이미지 색 변화를 최적화 하기 위해
	if (CurrentTime < 1.0f - KINDA_SMALL_NUMBER && !bIsCoolDown)
	{
		bIsCoolDown = true;
		SkillICon->SetColorAndOpacity(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f));
	}
}

void UCSkillIconUIWidget::FinishCoolDown()
{
	if (!SkillICon || !CoolTimeBar)
		return;

	CoolTimeBar->SetPercent(0.0f);
	bIsCoolDown = false;

	// 이미지 변화 및 이펙트, 애니메이션이 출력되는 코드가 필요함
	SkillICon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
	
	// 쿨타임 끝 사운드 출력
	if (SFX_CoolTimeFinished)
		UGameplayStatics::PlaySound2D(this, SFX_CoolTimeFinished);

	// 간단하게 플레이어 발밑에 한번 스폰하게....?
	if (VFX_CoolTimeFinishedOnPlayer)
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
	}
}
