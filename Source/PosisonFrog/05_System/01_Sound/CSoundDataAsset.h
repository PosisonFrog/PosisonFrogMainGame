#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CSoundDataAsset.generated.h"

class USoundBase;
class USoundCue;

/**
 * 캐릭터별 사운드 컬렉션
 * - 공격, 피격, 상태, 이동, 음성 등
 */

USTRUCT(BlueprintType)
struct FCharacterSoundCollection
{
    GENERATED_BODY()

    /** 기본 공격 사운드 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    TObjectPtr<USoundBase> AttackSound = nullptr;

    /** 스킬 사운드 (배열로 여러 스킬 지원) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    TArray<TObjectPtr<USoundBase>> SkillSounds;

    /** 돌진 사운드 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    TObjectPtr<USoundBase> ChargeSound = nullptr;

    /** 스윙 사운드 (휘두르는 소리) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    TObjectPtr<USoundBase> SwingSound = nullptr;

    /** 피격 사운드 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit")
    TObjectPtr<USoundBase> HitSound = nullptr;

    /** 경직 사운드 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit")
    TObjectPtr<USoundBase> StunSound = nullptr;

    /** 넉백 사운드 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit")
    TObjectPtr<USoundBase> KnockbackSound = nullptr;

    /** 사망 사운드 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State")
    TObjectPtr<USoundBase> DeathSound = nullptr;

    /** 부활 사운드 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State")
    TObjectPtr<USoundBase> RespawnSound = nullptr;

    /** 발소리 (걷기) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
    TObjectPtr<USoundBase> FootstepWalk = nullptr;

    /** 발소리 (달리기) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
    TObjectPtr<USoundBase> FootstepRun = nullptr;

    /** 점프 사운드 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
    TObjectPtr<USoundBase> JumpSound = nullptr;

    /** 착지 사운드 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
    TObjectPtr<USoundBase> LandSound = nullptr;

    /** 대시 사운드 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
    TObjectPtr<USoundBase> DashSound = nullptr;

    /** 전투 외침 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice")
    TArray<TObjectPtr<USoundBase>> BattleCries;

    /** 승리 외침 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice")
    TObjectPtr<USoundBase> VictorySound = nullptr;
};

/**
 * 게임 사운드 컬렉션
 * - BGM, UI 사운드 등
 */
USTRUCT(BlueprintType)
struct FGameSoundCollection
{
    GENERATED_BODY()

    /** 메인 메뉴 BGM */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM")
    TObjectPtr<USoundBase> MainMenuBGM = nullptr;

    /** 게임플레이 BGM (일반 전투) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM")
    TObjectPtr<USoundBase> GameplayBGM = nullptr;

    /** 보스 전투 BGM */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM")
    TObjectPtr<USoundBase> BossBattleBGM = nullptr;

    /** 승리 BGM */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM")
    TObjectPtr<USoundBase> VictoryBGM = nullptr;

    /** 게임 오버 BGM */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM")
    TObjectPtr<USoundBase> GameOverBGM = nullptr;

    /** 버튼 클릭 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TObjectPtr<USoundBase> ButtonClickSound = nullptr;

    /** 버튼 호버 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TObjectPtr<USoundBase> ButtonHoverSound = nullptr;

    /** 메뉴 열기 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TObjectPtr<USoundBase> MenuOpenSound = nullptr;

    /** 메뉴 닫기 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TObjectPtr<USoundBase> MenuCloseSound = nullptr;

    /** 아이템 획득 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TObjectPtr<USoundBase> ItemPickupSound = nullptr;

    /** 체크포인트 활성화 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TObjectPtr<USoundBase> CheckpointSound = nullptr;

    /** 레벨 클리어 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "System")
    TObjectPtr<USoundBase> LevelClearSound = nullptr;

    /** 경고음 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "System")
    TObjectPtr<USoundBase> WarningSound = nullptr;
};

/**
 * 사운드 데이터 에셋
 * - 게임 전체의 사운드를 한 곳에서 관리
 */
UCLASS(BlueprintType)
class POSISONFROG_API UCSoundDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** 플레이어 사운드 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
    FCharacterSoundCollection PlayerSounds;

    /** 데드베어 사운드 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Enemy")
    FCharacterSoundCollection RiotRobotSounds;

    /** 벌룬버니 사운드 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Enemy")
    FCharacterSoundCollection RangedSkirmisherSounds;

    /** 엄지늘보 사운드 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Enemy")
    FCharacterSoundCollection TankerBruteSounds;

    /** 보스 사운드 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Boss")
    FCharacterSoundCollection BossSounds;

    /** 게임 전체 사운드 (BGM, UI 등) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game")
    FGameSoundCollection GameSounds;

    /** 캐릭터 타입으로 사운드 컬렉션 가져오기  */
    const FCharacterSoundCollection* GetCharacterSounds(FName CharacterType) const;
};