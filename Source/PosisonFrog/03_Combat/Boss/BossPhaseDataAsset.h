#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "BossPhaseDataAsset.generated.h"

/** 개별 보스 패턴(스킬) 정의 */
USTRUCT(BlueprintType)
struct FBossPatternDefinition
{
    GENERATED_BODY();

    /** 내부 식별용 이름(블루프린트/애님 노티파이 연동용) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pattern")
    FName PatternId = NAME_None;

    /** 패턴을 발동하기 위해 필요한 최소 파워. 부족하면 대기. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pattern", meta=(ClampMin="0"))
    float RequiredPower = 0.f;

    /** 발동 시 즉시 차감될 파워 값 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pattern", meta=(ClampMin="0"))
    float PowerCost = 0.f;

    /** 패턴 수행 시간(이 동안 상태가 Pattern으로 유지) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pattern", meta=(ClampMin="0"))
    float ExecutionTime = 3.f;

    /** 패턴 종료 후 후딜레이 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pattern", meta=(ClampMin="0"))
    float RecoveryTime = 1.f;

    /** 재사용 대기시간 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pattern", meta=(ClampMin="0"))
    float Cooldown = 6.f;

    /** 선택 가중치. 값이 높을수록 패턴 선택 확률 증가 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pattern", meta=(ClampMin="0"))
    float Weight = 1.f;

    /** 패턴 성공 시 회복되는 파워 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pattern", meta=(ClampMin="0"))
    float PowerReward = 0.f;

    /** 강력한 패턴인지 여부(예: 클라이맥스) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pattern")
    bool bIsClimax = false;
};

/** 페이즈 단위 설정 */
USTRUCT(BlueprintType)
struct FBossPhaseDefinition
{
    GENERATED_BODY();

    /** 페이즈 이름(디버그 용도) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Phase")
    FName PhaseName = TEXT("Phase");

    /** 체력 퍼센트 기준 진입 시점 (0~1). 체력이 이하로 내려가면 해당 페이즈 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Phase", meta=(ClampMin="0", ClampMax="1"))
    float EnterHealthRatio = 1.f;

    /** 페이즈 진입 컷신/모션 시간 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Phase", meta=(ClampMin="0"))
    float IntroDuration = 2.f;

    /** 페이즈 공백/회복 기본 시간 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Phase", meta=(ClampMin="0"))
    float BaseRecoveryDuration = 2.f;

    /** 페이즈별 파워 획득 배율 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Phase")
    float PowerGainMultiplier = 1.f;

    /** 페이즈별 파워 소비 배율 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Phase")
    float PowerDrainMultiplier = 1.f;

    /** 파워가 최대치 도달 시 발동되는 메타 스킬 이름 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Phase")
    FName ShoutMetaId = NAME_None;

    /** 메타 스킬 연출 시간 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Phase", meta=(ClampMin="0"))
    float ShoutDuration = 4.f;

    /** 페이즈에서 사용 가능한 패턴 목록 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Phase")
    TArray<FBossPatternDefinition> Patterns;
};

/** 보스 페이즈/파워 데이터 에셋 */
UCLASS(BlueprintType)
class POSISONFROG_API UBossPhaseDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UBossPhaseDataAsset();

    /** 최대 파워 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss")
    float MaxPower = 200.f;

    /** 초당 자연 회복 파워 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss")
    float PassivePowerPerSecond = 1.5f;

    /** 플레이어에게 피격 시 추가 획득하는 파워 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss")
    float PowerGainOnHit = 4.f;

    /** 패턴 실패 등으로 감소하는 기본 파워 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss")
    float PowerLossOnInterrupt = 10.f;

    /** 파워가 이 값 이상 쌓이면 메타 샤우트 발동 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss")
    float ShoutTriggerRatio = 0.95f;

    /** 페이즈 데이터 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss")
    TArray<FBossPhaseDefinition> Phases;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};