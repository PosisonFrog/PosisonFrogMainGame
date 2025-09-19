#pragma once

#include "CoreMinimal.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "Blueprint/UserWidget.h"
#include "CPlayerWidget.generated.h"

class UCUltimateSkillIconWidget;
class UCTimeCooldownSkillIconWidget;
class UTextBlock;
class UProgressBar;
class UCPlayerHpBarWidget;

UCLASS()
class POSISONFROG_API UCPlayerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// HP UI (프로젝트 로직대로 구현)
	UFUNCTION(BlueprintCallable) void UpdateHpBar(float Current, float Max);

	// Dash 쿨타임 UI
	UFUNCTION(BlueprintCallable) void UpdateDashCooldown(float RemainingSeconds, float TotalSeconds);
	UFUNCTION(BlueprintCallable) void SetDashReady();

	// 궁극기 UI
	UFUNCTION(BlueprintCallable)
	void SetUltimatePoints(float UltimateCurrentPoints, float UltimateMaxPoints, int32 UltimateStack);

protected:
	// UMG 자산에 동일한 이름의 위젯이 있으면 자동 바인딩(옵션)
	// === Skill UI 관련 ===
	UPROPERTY(EditDefaultsOnly, meta = (BindWidgetOptional))
	UTextBlock* DashCooldownText = nullptr;
	UPROPERTY(EditDefaultsOnly, meta = (BindWidgetOptional))
	UCTimeCooldownSkillIconWidget* WBP_DashSkillIconWidget = nullptr;

	UPROPERTY(EditDefaultsOnly, meta = (BindWidgetOptional))
	UCUltimateSkillIconWidget* WBP_UltimateSkillIconWidget = nullptr;

	// 필요하면 HP 바도 BindWidgetOptional 추가
	UPROPERTY(EditDefaultsOnly, meta=(BindWidget))
	TObjectPtr<UCPlayerHpBarWidget> WBP_PlayerHpBar;
};

