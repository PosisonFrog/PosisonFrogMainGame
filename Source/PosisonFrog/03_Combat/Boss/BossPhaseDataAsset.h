#pragma once

#include "CoreMinimal.h"

#include "Engine/EngineTypes.h"
#include "Engine/DataAsset.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "BossPhaseDataAsset.generated.h"


UENUM(BlueprintType)
enum class EBossPatternSpawnAnchor : uint8
{
    /** 보스의 현재 월드 위치 기준 */
    BossRoot,

    /** 보스 메시의 소켓 기준 */
    BossSocket,

    /** 현재 타겟(플레이어) 위치 기준 */
    PlayerLocation,

    /** 특정 액터(스폰 존 등)를 기준 */
    CustomActor
};

/** 패턴 스폰 위치/회전 정보를 정의 */
USTRUCT(BlueprintType)
struct FBossPatternSpawnTransform
{
    GENERATED_BODY();

    /** 기준 위치 타입 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn")
    EBossPatternSpawnAnchor Anchor = EBossPatternSpawnAnchor::BossRoot;

    /** BossSocket 기준일 때 사용할 소켓 이름 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(EditCondition="Anchor==EBossPatternSpawnAnchor::BossSocket"))
    FName SocketName = NAME_None;

    /** 커스텀 액터 기준일 때 사용할 참조 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(EditCondition="Anchor==EBossPatternSpawnAnchor::CustomActor", AllowedClasses= "/Script/Engine.Actor"))
    TSoftObjectPtr<AActor> SpawnAnchor;

    /** 기준 위치에서 적용할 추가 위치 오프셋 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn")
    FVector LocationOffset = FVector::ZeroVector;

    /** 기준 회전에서 더해질 회전 오프셋 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn")
    FRotator RotationOffset = FRotator::ZeroRotator;

    /** 기준 액터의 회전을 그대로 사용할지 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn")
    bool bUseAnchorRotation = true;

    /** 지면으로 투영할지 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn")
    bool bProjectToGround = false;

    /** 지면 투영 시 사용될 최대 거리 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(EditCondition="bProjectToGround", ClampMin="0"))
    float GroundTraceDistance = 2000.f;

    /** 지면에 붙일 때 충돌 채널 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(EditCondition="bProjectToGround"))
    TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_WorldStatic;

    /** 지면에 붙인 후 노멀 방향으로 회전 정렬 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(EditCondition="bProjectToGround"))
    bool bAlignToGroundNormal = false;
};

/** 패턴 실행 시 생성될 웨폰(투사체 등) 정보 */
USTRUCT(BlueprintType)
struct FBossPatternWeaponSpawnDefinition
{
    GENERATED_BODY();

    /** 스폰할 액터 클래스 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
    TSubclassOf<AActor> WeaponClass;

    /** 스폰 위치/회전 지정 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
    FBossPatternSpawnTransform SpawnTransform;
    
    /** 소켓 기준일 때 부모에 붙일지 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
    bool bAttachToSpawnAnchor = true;

    /** 패턴 종료 시 자동 제거 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
    bool bDestroyOnPatternEnd = true;
    
    /** 생성 직후 부여할 초기 속도 (ProjectileMovement가 있을 경우 적용) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
    FVector InitialVelocity = FVector::ZeroVector;
};

/** 패턴 실행 중 생성되는 보조 액터 정보 */
USTRUCT(BlueprintType)
struct FBossPatternUtilitySpawnDefinition
{
    GENERATED_BODY();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Utility")
    TSubclassOf<AActor> ActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Utility")
    FBossPatternSpawnTransform SpawnTransform;

    /** 생성 수량 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Utility", meta=(ClampMin="1"))
    int32 SpawnCount = 1;

    /** 생성 시 반경 랜덤 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Utility", meta=(ClampMin="0"))
    float SpawnRadius = 0.f;

    /** 반복 생성 시 간격(0이면 즉시 모두 생성) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Utility", meta=(ClampMin="0"))
    float SpawnInterval = 0.f;

    /** 패턴 종료 시 제거 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Utility")
    bool bDestroyOnPatternEnd = true;
};

/** 패턴에서 소환되는 일반 몬스터 정보 */
USTRUCT(BlueprintType)
struct FBossPatternMinionSpawnDefinition
{
    GENERATED_BODY();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion")
    TSubclassOf<APawn> MinionClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion")
    FBossPatternSpawnTransform SpawnTransform;

    /** 생성 수량 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion", meta=(ClampMin="1"))
    int32 SpawnCount = 1;

    /** 반경 내 랜덤 스폰 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion", meta=(ClampMin="0"))
    float SpawnRadius = 0.f;

    /** 생성 지연 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion", meta=(ClampMin="0"))
    float SpawnDelay = 0.f;

    /** 기본 AI 컨트롤러 스폰 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion")
    bool bSpawnDefaultController = true;

    /** 패턴 종료 시 제거 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion")
    bool bDestroyOnPatternEnd = false;
};

/** 하늘에서 투사체를 떨어뜨리는 패턴 전용 설정 */
USTRUCT(BlueprintType)
struct FBossPatternProjectileRainSettings
{
    GENERATED_BODY();

    /** 활성화 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ProjectileRain")
    bool bEnableRain = false;

    /** 사용될 투사체 클래스 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ProjectileRain")
    TSubclassOf<AActor> ProjectileClass;

    /** 스폰 위치 기준 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ProjectileRain")
    FBossPatternSpawnTransform SpawnTransform;

    /** 총 웨이브 수 (0이면 무한) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ProjectileRain", meta=(ClampMin="0"))
    int32 Waves = 3;

    /** 웨이브당 생성 수 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ProjectileRain", meta=(ClampMin="1"))
    int32 ProjectilesPerWave = 5;

    /** 웨이브 간 간격 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ProjectileRain", meta=(ClampMin="0"))
    float SpawnInterval = 0.5f;

    /** 랜덤 산포 반경 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ProjectileRain", meta=(ClampMin="0"))
    float SpawnRadius = 500.f;

    /** 기준 위치에서 추가로 올릴 높이 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ProjectileRain")
    float SpawnHeight = 800.f;

    /** 초기 속도 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ProjectileRain")
    FVector InitialVelocity = FVector(0.f, 0.f, -1500.f);

    /** 패턴 종료 시 투사체 제거 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ProjectileRain")
    bool bDestroyOnPatternEnd = true;
};

class AActor;
class APawn;

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

    /** 패턴 예고(준비) 시간 - Telegraph, Windup 등 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pattern|Timing", meta=(ClampMin="0"))
    float TelegraphTime = 0.5f;

    /** 패턴 메인 실행 시간 (실제 공격/돌진 시간) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pattern|Timing", meta=(ClampMin="0"))
    float ExecutionTime = 2.0f;

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

    /** 패턴 시작 시 생성될 웨폰들 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pattern|Spawns")
    TArray<FBossPatternWeaponSpawnDefinition> WeaponSpawns;
    
    /** 패턴 중 사용할 기타 액터 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pattern|Spawns")
    TArray<FBossPatternUtilitySpawnDefinition> UtilitySpawns;
    
    /** 일반 몬스터 소환 정보 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pattern|Spawns")
    TArray<FBossPatternMinionSpawnDefinition> MinionSpawns;
    
    /** 투사체 빗방울 패턴 설정 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pattern|Spawns")
    FBossPatternProjectileRainSettings ProjectileRain;
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