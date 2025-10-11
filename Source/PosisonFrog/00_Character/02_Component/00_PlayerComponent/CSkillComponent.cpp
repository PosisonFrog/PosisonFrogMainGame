// Fill out your copyright notice in the Description page of Project Settings.


#include "CSkillComponent.h"
#include "00_Character/02_Component/00_PlayerComponent/CFuryGaugeComponent.h"

UCSkillComponent::UCSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCSkillComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		FuryRef = Owner->FindComponentByClass<UCFuryGaugeComponent>();
		if (FuryRef)
		{
			FuryRef->OnEffectStarted     .AddDynamic(this, &UCSkillComponent::OnFuryStarted);
			FuryRef->OnEffectTick        .AddDynamic(this, &UCSkillComponent::OnFuryTick);
			FuryRef->OnEffectEnded       .AddDynamic(this, &UCSkillComponent::OnFuryEnded);
			FuryRef->OnFinisherTriggered .AddDynamic(this, &UCSkillComponent::OnFuryFinisher);
		}
	}
}

bool UCSkillComponent::ActivateSkill()
{
	if (State == ESkillState::Active || State == ESkillState::Cooldown) return false;
	if (DoActivate())
	{
		State = ESkillState::Active;
		return true;
	}
	return false;
}

bool UCSkillComponent::CancelSkill()
{
	if (State != ESkillState::Active) return false;
	if (DoCancel())
	{
		State = ESkillState::Inactive;
		return true;
	}
	return false;
}

// Fury 기본 구현은 비워두고, 파생 스킬에서 필요한 것만 override
void UCSkillComponent::OnFuryStarted (int32, float, float, int32) {}
void UCSkillComponent::OnFuryTick    (float, float) {}
void UCSkillComponent::OnFuryEnded   (bool, float) {}
void UCSkillComponent::OnFuryFinisher(float) {}

