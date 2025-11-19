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

			// 1. PatternId 검사
			if (Pattern.PatternId.IsNone())
			{
				Context.AddError(FText::Format(NSLOCTEXT("BossPhaseDataAsset", "PatternIdMissing", "Phase {0} 의 패턴 {1} : PatternId가 설정되지 않았습니다."), FText::FromName(Phase.PhaseName), FText::AsNumber(PatternIdx)));
				return EDataValidationResult::Invalid;
			}

			// 2. ExecutionTime 검사
			if (Pattern.ExecutionTime <= 0.f)
			{
				Context.AddError(FText::Format(
					NSLOCTEXT("BossPhaseDataAsset", "ExecutionTimeZero", 
					"Phase {0} 패턴 {1} : ExecutionTime이 0 이하입니다. 최소 0.1초 이상 필요합니다."), 
					FText::FromName(Phase.PhaseName), 
					FText::FromName(Pattern.PatternId)));
				return EDataValidationResult::Invalid;
			}

			// ✅ [추가] Cooldown 검사
			if (Pattern.Cooldown <= 0.f)
			{
				Context.AddError(FText::Format(
					NSLOCTEXT("BossPhaseDataAsset", "CooldownZero", 
					"Phase {0} 패턴 {1} : Cooldown이 0 이하입니다. 최소 0.1초 이상 필요합니다."), 
					FText::FromName(Phase.PhaseName), 
					FText::FromName(Pattern.PatternId)));
				return EDataValidationResult::Invalid;
			}

			// 3. WeaponSpawns 검사
			for (int32 WeaponIdx = 0; WeaponIdx < Pattern.WeaponSpawns.Num(); ++WeaponIdx)
			{
				if (!Pattern.WeaponSpawns[WeaponIdx].WeaponClass)
				{
					Context.AddError(FText::Format(NSLOCTEXT("BossPhaseDataAsset", "WeaponClassMissing", "Phase {0} 패턴 {1} : WeaponSpawns[{2}] 의 WeaponClass가 비어 있습니다."), FText::FromName(Phase.PhaseName), FText::FromName(Pattern.PatternId), FText::AsNumber(WeaponIdx)));
					return EDataValidationResult::Invalid;
				}
			}

			// 4. UtilitySpawns 검사
			for (int32 UtilityIdx = 0; UtilityIdx < Pattern.UtilitySpawns.Num(); ++UtilityIdx)
			{
				if (!Pattern.UtilitySpawns[UtilityIdx].ActorClass)
				{
					Context.AddError(FText::Format(NSLOCTEXT("BossPhaseDataAsset", "UtilityClassMissing", "Phase {0} 패턴 {1} : UtilitySpawns[{2}] 의 ActorClass가 비어 있습니다."), FText::FromName(Phase.PhaseName), FText::FromName(Pattern.PatternId), FText::AsNumber(UtilityIdx)));
					return EDataValidationResult::Invalid;
				}
			}

			// 5. MinionSpawns 검사
			for (int32 MinionIdx = 0; MinionIdx < Pattern.MinionSpawns.Num(); ++MinionIdx)
			{
				if (!Pattern.MinionSpawns[MinionIdx].MinionClass)
				{
					Context.AddError(FText::Format(NSLOCTEXT("BossPhaseDataAsset", "MinionClassMissing", "Phase {0} 패턴 {1} : MinionSpawns[{2}] 의 MinionClass가 비어 있습니다."), FText::FromName(Phase.PhaseName), FText::FromName(Pattern.PatternId), FText::AsNumber(MinionIdx)));
					return EDataValidationResult::Invalid;
				}
			}

			// 6. ProjectileRain 검사
			if (Pattern.ProjectileRain.bEnableRain && !Pattern.ProjectileRain.ProjectileClass)
			{
				Context.AddError(FText::Format(NSLOCTEXT("BossPhaseDataAsset", "ProjectileRainMissing", "Phase {0} 패턴 {1} : ProjectileRain 사용 시 ProjectileClass가 필요합니다."), FText::FromName(Phase.PhaseName), FText::FromName(Pattern.PatternId)));
				return EDataValidationResult::Invalid;
			}
		}
	}

	return EDataValidationResult::Valid;
}
#endif