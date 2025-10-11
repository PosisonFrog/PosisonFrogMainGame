// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CSkillComponent.generated.h"

class UCFuryGaugeComponent;

UENUM(BlueprintType)
enum class ESkillState : uint8
{
	Inactive,
	Casting,
	Active,
	Cooldown
};

UCLASS(Abstract, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UCSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCSkillComponent();

	UFUNCTION(BlueprintCallable) virtual bool ActivateSkill();
	UFUNCTION(BlueprintCallable) virtual bool CancelSkill();
	UFUNCTION(BlueprintPure)     bool IsSkillActive() const { return State == ESkillState::Active; }

protected:
	virtual void BeginPlay() override;

	// Fury 이벤트 훅 (Dynamic Multicast에 AddDynamic 하기 위해 UFUNCTION 필요)
	UFUNCTION() virtual void OnFuryStarted (int32 TierIdx, float Duration, float TotalDamage, int32 InitialStacks);
	UFUNCTION() virtual void OnFuryTick    (float TimeRemaining, float TotalDuration);
	UFUNCTION() virtual void OnFuryEnded   (bool bCanceled, float TimeRemainingAtEnd);
	UFUNCTION() virtual void OnFuryFinisher(float FinisherDamage);

	// 파생 스킬 구현 포인트
	virtual bool DoActivate() PURE_VIRTUAL(UCSkillComponent::DoActivate, return false;);
	virtual bool DoCancel()   { return false; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Skill")
	ESkillState State = ESkillState::Inactive;

	UPROPERTY() UCFuryGaugeComponent* FuryRef = nullptr;
};

