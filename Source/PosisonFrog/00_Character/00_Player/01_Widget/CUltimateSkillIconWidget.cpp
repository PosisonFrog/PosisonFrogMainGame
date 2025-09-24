#include "00_Character/00_Player/01_Widget/CUltimateSkillIconWidget.h"

#include "Components/ProgressBar.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/KismetMathLibrary.h"
#include "Internationalization/Text.h"

void UCUltimateSkillIconWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bConstructed = true;

	// UMG 변수명이 다르거나 BindWidgetOptional이 실패해도 런타임 탐색
	TryResolveWidgetRefs();

	// 생성 전에 들어온 값이 있으면 반영
	ApplyVisuals();
}

void UCUltimateSkillIconWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyVisuals();
}

void UCUltimateSkillIconWidget::SetRatio(float InRatio)
{
	Ratio = FMath::Clamp(InRatio, 0.f, 1.f);

	// 위젯 생성 전이면 캐시만 해두고, 생성 후엔 즉시 반영
	if (bConstructed)
	{
		ApplyVisuals();
	}
}

void UCUltimateSkillIconWidget::SetUsable(bool bInUsable)
{
	bUsable = bInUsable;
	if (bConstructed)
	{
		ApplyVisuals();
	}
}

void UCUltimateSkillIconWidget::ApplyVisuals()
{
	if (!SkillBar)
	{
		TryResolveWidgetRefs();
	}
	if (!SkillBar) return;

	// 1) 퍼센트 갱신
	SkillBar->SetPercent(Ratio);

	// 2) 색상 자동 보간
	if (bAutoColorByRatio)
	{
		// 0~0.5: Empty→Mid, 0.5~1.0: Mid→Full 로 2단계 보간
		const float Half = 0.5f;
		FLinearColor FillColor;
		if (Ratio <= Half)
		{
			const float T = (Half > 0.f) ? (Ratio / Half) : 0.f;
			FillColor = UKismetMathLibrary::LinearColorLerp(Color_Empty, Color_Mid, T);
		}
		else
		{
			const float T = (Ratio - Half) / (1.f - Half);
			FillColor = UKismetMathLibrary::LinearColorLerp(Color_Mid, Color_Full, T);
		}
		// 사용 불가면 살짝 회색/감산 틴트
		if (!bUsable)
		{
			FillColor = UKismetMathLibrary::LinearColorLerp(FillColor, Tint_Unusable, 0.65f);
		}
		SkillBar->SetFillColorAndOpacity(FillColor);
	}


	// 3) 풀충전 펄스 (선택)
	if (bPulseWhenFull && Anim_Pulse)
	{
		if (Ratio >= FullThreshold)
		{
			if (!IsAnimationPlaying(Anim_Pulse))
			{
				// 루프 가볍게 1~2회 정도만 돌리고 끊고 싶다면 PlayAnimation 설정 바꿔도 됩니다.
				PlayAnimation(Anim_Pulse, 0.f, 1, EUMGSequencePlayMode::Forward, 1.f);
			}
		}
		else
		{
			// 가득 찼다가 내려와도 펄스 정지
			if (IsAnimationPlaying(Anim_Pulse))
			{
				StopAnimation(Anim_Pulse);
			}
		}
	}

	// 4) 툴팁(접근성: 대략 % 표기)
	const int32 PercentInt = FMath::RoundToInt(Ratio * 100.f);
	SetToolTipText(FText::FromString(FString::Printf(TEXT("Ultimate: %d%%"), PercentInt)));
}

void UCUltimateSkillIconWidget::TryResolveWidgetRefs()
{
	// BindWidgetOptional이 실패했거나 디자인에서 이름이 달라졌을 경우 런타임으로 찾아봄
	if (!SkillBar)
	{
		if (UWidget* Found = (WidgetTree ? WidgetTree->FindWidget(TEXT("SkillBar")) : nullptr))
		{
			SkillBar = Cast<UProgressBar>(Found);
		}
	}
}

