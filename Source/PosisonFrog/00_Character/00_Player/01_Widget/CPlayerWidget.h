#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CPlayerWidget.generated.h"

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

protected:
	// UMG 자산에 동일한 이름의 위젯이 있으면 자동 바인딩(옵션)
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* DashCooldownText = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UProgressBar* DashCooldownBar = nullptr;

	// 필요하면 HP 바도 BindWidgetOptional 추가
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta=(BindWidget))
	TObjectPtr<UCPlayerHpBarWidget> WBP_PlayerHpBar;
};

