#include "CPlayerWidget.h"

#include "CPlayerHpBarWidget.h"
#include "CSkillUIWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

void UCPlayerWidget::UpdateHpBar(float Current, float Max)
{
	// 프로젝트에 맞게 구현하세요 (예: ProgressBar/숫자 텍스트 갱신) -> 나중에 맞춰봅시다
	if (WBP_PlayerHpBar)
		WBP_PlayerHpBar->UpdateHp(Current, Max);
}

void UCPlayerWidget::UpdateDashCooldown(float RemainingSeconds, float TotalSeconds)
{
	if (DashCooldownText)
	{
		const double Sec = FMath::Max(0.0, (double)RemainingSeconds);
		FNumberFormattingOptions Opt;
		Opt.SetMinimumFractionalDigits(1);
		Opt.SetMaximumFractionalDigits(1);
		const FText SecText = FText::AsNumber(Sec, &Opt);
		DashCooldownText->SetText(FText::Format(FText::FromString(TEXT("{0}s")), SecText));
		DashCooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (WBP_DashSkillUI)
	{
		float CurrentTime = 0.f;
		if (TotalSeconds > 0.f)
		{
			const float Ratio = FMath::Clamp(RemainingSeconds / TotalSeconds, 0.f, 1.f);
			CurrentTime = 1.0f - Ratio;
		}

		WBP_DashSkillUI->UpdateCoolDownUI(CurrentTime, TotalSeconds);
		//WBP_DashSkillUI->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UCPlayerWidget::SetDashReady()
{
	if (DashCooldownText)
	{
		DashCooldownText->SetText(FText::FromString(TEXT("READY")));
		DashCooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	
	if (WBP_DashSkillUI)
	{
		WBP_DashSkillUI->FinishCoolDown();
		//WBP_DashSkillUI->SetVisibility(ESlateVisibility::Collapsed);
	}
}


/*빌드 설정
YourModule.Build.cs에 아래 모듈이 포함되어야 함
PublicDependencyModuleNames.AddRange(new string[] {
  "Core", "CoreUObject", "Engine", "InputCore",
  "UMG", "Slate", "SlateCore"
}); -> c# 코드임
*/
