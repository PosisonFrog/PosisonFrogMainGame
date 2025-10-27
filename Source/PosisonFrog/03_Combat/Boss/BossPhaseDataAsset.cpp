#include "BossPhaseDataAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UBossPhaseDataAsset::UBossPhaseDataAsset()
{
}

#if WITH_EDITOR
EDataValidationResult UBossPhaseDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	if (Phases.Num() == 0)
	{
		Context.AddError(NSLOCTEXT("BossPhaseDataAsset", "NoPhases", "페이즈 데이터가 비어 있습니다."));
		return EDataValidationResult::Invalid;
	}

	for (int32 PhaseIdx = 0; PhaseIdx < Phases.Num(); ++PhaseIdx)
	{
		const FBossPhaseDefinition& Phase = Phases[PhaseIdx];
		if (Phase.Patterns.Num() == 0)
		{
			Context.AddWarning(FText::Format(NSLOCTEXT("BossPhaseDataAsset", "PhaseNoPattern", "{0} : 패턴이 없습니다."), FText::FromName(Phase.PhaseName)));
		}

		for (int32 PatternIdx = 0; PatternIdx < Phase.Patterns.Num(); ++PatternIdx)
		{
			const FBossPatternDefinition& Pattern = Phase.Patterns[PatternIdx];
			if (Pattern.PatternId.IsNone())
			{
				Context.AddError(FText::Format(NSLOCTEXT("BossPhaseDataAsset", "PatternIdMissing", "Phase {0} 의 패턴 {1} : PatternId가 설정되지 않았습니다."), FText::FromName(Phase.PhaseName), FText::AsNumber(PatternIdx)));
				return EDataValidationResult::Invalid;
			}
		}
	}

	return EDataValidationResult::Valid;
}
#endif
