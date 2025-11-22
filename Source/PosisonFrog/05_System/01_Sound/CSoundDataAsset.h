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

public:
    // 규칙 0: 허공에 휘두를 때 (Swing)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    USoundBase* WeaponSwingSound;

    // 규칙 1: 몬스터 타격 성공 시 (Hit)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    USoundBase* AttackHitSound;

    // 규칙 2: 대시 (Dash)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    USoundBase* DashSound;

    // 규칙 3: 커맨드 스킬 (1타: 띄우기, 2타: 내려찍기)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound|Skill")
    USoundBase* CommandLaunchSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound|Skill")
    USoundBase* CommandSlamSound;

    // 규칙 4: 스핀 스킬 (루프)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound|Skill")
    USoundBase* SpinLoopSound;

    // 사망
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    USoundBase* DeathSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    USoundBase* KnockdownSound;
    
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
    
    /** 보스 사운드 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Boss")
    FCharacterSoundCollection BossSounds;

    /** 게임 전체 사운드 (BGM, UI 등) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game")
    FGameSoundCollection GameSounds;

    /** 캐릭터 타입으로 사운드 컬렉션 가져오기  */
    const FCharacterSoundCollection* GetCharacterSounds(FName CharacterType) const;
};