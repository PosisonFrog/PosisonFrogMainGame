// CMainGameModeBase.h
// gamePlay용

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CMainGameModeBase.generated.h"

class ACPlayerCharacter;
class ACPlayerController;
class ACHealOrb;
class ACCheckPoint;

// 플레이어 상태 스냅샷 구조체
// 스테이지 클리어 당시 스텟을 저장하기 위함
USTRUCT(BlueprintType)
struct FPlayerStateSnapshot
{
	GENERATED_BODY()

	// 체력
	UPROPERTY() float CurrentHealth = 100.0f;
	UPROPERTY() float MaxHealth = 100.0f;

	// 궁극기 게이지
	UPROPERTY() float UltimateGauge = 100.0f;

	// Fury 게이지
	UPROPERTY() float FuryGauge = 100.0f;

	// 체크포인트 위치
	UPROPERTY() FVector CheckPointLocation = FVector::ZeroVector;
	UPROPERTY() FRotator CheckPointRotation = FRotator::ZeroRotator;

	FPlayerStateSnapshot() {}
};

// 스테이지 스냅샷 구조체
USTRUCT(BlueprintType)
struct FStageSnapshot
{
	GENERATED_BODY()

	// 현재 활성 스테이지
	int32 ActivateStage = 1;

	// 클리어한 스테이지들
	UPROPERTY() TSet<int32> ClearedStages;

	FStageSnapshot() {}
};

/**
 * 게임플레이 기본 GameMode
 * - DefaultPawn: ACPlayerCharacter
 * - PlayerController: ACPlayerController
 */
UCLASS(config = Game)
class POSISONFROG_API ACMainGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACMainGameModeBase();

	// ──────────── 체크포인트 관리 ────────────
	// 현재 체크포인트 설정 + 플레이어 상태 저장
	UFUNCTION()
	void OnCheckPointActivateEvent(ACCheckPoint* CheckPoint, ACPlayerCharacter* Player);

	UFUNCTION()
	ACCheckPoint* GetCurrentCheckPoint() const { return CurrentCheckPoint; }

	UFUNCTION(BlueprintCallable, Category = "Game|Pause")
	void RestartFromLastCheckpoint(ACPlayerController* PlayerController);
	
	UFUNCTION(BlueprintCallable, Category = "Game|Pause")
	void ReturnToTitleScreen();
	
	UFUNCTION(BlueprintPure, Category = "Game|Pause")
	FName GetMainMenuLevelName() const { return MainMenuLevelName; }

	// ──────────── 플레이어 사망/리스폰 ────────────
	UFUNCTION()
	void OnPlayerDeath(ACPlayerController* PlayerController);

	// 체크포인트에서 플레이어 리스폰
	UFUNCTION()
	void RespawnPlayerAtCheckPoint(ACPlayerController* PlayerController);

	// ──────────── BGM 관리 ────────────
	/** 게임플레이 BGM 시작 */
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void StartGameplayBGM();

	/** 보스 BGM으로 전환 */
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void PlayBossBGM();

	// ──────────── Getter ────────────
	/** SoundDataAsset 가져오기 (캐릭터들이 사운드 로드할 때 사용) */
	UFUNCTION(BlueprintPure, Category = "Audio")
	UCSoundDataAsset* GetSoundDataAsset() const { return SoundDataAsset; }


protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION() void ReturnToMenu();

	// 플레이어 상태 저장/복원
	void SavePlayerState(ACPlayerCharacter* Player);
	void RestorePlayerState(ACPlayerCharacter* Player);
 
	// Stage 상태 저장/복원
	void RequestStageRespawn();
	void ResetBossBattleState();
	
private:

	// ──────────── 사운드 데이터 ────────────
	/** 게임 전체 사운드를 담은 데이터 에셋 */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<UCSoundDataAsset> SoundDataAsset = nullptr;


	/** 세팅 UI넣기 전까지 임시 마스터 믹스 */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundMix> MasterSoundMix = nullptr;
	
	// ──────────── 체크포인트 ────────────
	UPROPERTY(VisibleInstanceOnly, Category = "Game|CheckPoint")
	TObjectPtr<ACCheckPoint> CurrentCheckPoint = nullptr;

	UPROPERTY(VisibleInstanceOnly, Category = "Game|CheckPoint")
	FPlayerStateSnapshot PlayerStateSnapshot;

	UPROPERTY(VisibleAnywhere, Category = "Game|CheckPoint")
	bool bHasValidSnapshot = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Game|CheckPoint")
	FStageSnapshot StageSnapshot;
	
	// ──────────── 힐 오브 ────────────
	UPROPERTY(EditAnywhere, Category = "Item")
	TSubclassOf<ACHealOrb> HealOrbClass;

	// ──────────── 사망 설정 ────────────
	UPROPERTY(EditDefaultsOnly, Category = "Game|Respawn", meta = (AllowPrivateAccess = "true"))
	FName MainMenuLevelName = TEXT("MainMenu");

	// 메인 메뉴로 돌아가는 딜레이
	UPROPERTY(EditDefaultsOnly, Category = "Game|Respawn", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float DeathReturnDelay = 5.0f;

	// 리스폰 딜레이
	UPROPERTY(EditDefaultsOnly, Category = "Game|Respawn")
	float RespawnDelay = 3.0f;

	// 체크포인트 없을 때 메뉴로 돌아갈지 여부
	UPROPERTY(EditDefaultsOnly, Category = "Game|Respawn")
	bool bReturnToMenuIfNoCheckPoint = true;
	
	FTimerHandle TimerHandle_ReturnToMenu;
	FTimerHandle TimerHandle_Respawn;
};
