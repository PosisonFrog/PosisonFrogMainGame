#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CBossBattleStartTrigger.generated.h"

class ACPlayerCharacter;
class UBoxComponent;
class ACEnemyBossCharacter;
class ACBossStageBarrier;
class ULevelSequence;
class ALevelSequenceActor;
class ULevelSequencePlayer;        
class UUserWidget;
class ACStageManager;

/**
 * 플레이어가 진입하면 보스 전투를 시작하는 트리거 볼륨
 */
UCLASS()
class POSISONFROG_API ACBossBattleStartTrigger : public AActor
{
    GENERATED_BODY()
    
public:    
    ACBossBattleStartTrigger();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** 오버랩 이벤트 핸들러 */
    UFUNCTION()
    void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
                               UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
                               bool bFromSweep, const FHitResult& SweepResult);

    /** 플레이어가 트리거에 진입했을 때 보스 전투를 시도 */
    void AttemptStartBossBattle(ACPlayerCharacter* PlayerCharacter);


    /** 경고 UI 표시 */
    void ShowWarningUI();
    
    /** 레벨 시퀀스 재생 */
    void PlayIntroSequence();
    
    /** 플레이어를 보스 앞으로 재배치 */
    void RepositionPlayer(ACPlayerCharacter* PlayerCharacter);
    
    /** 시퀀스 종료 시 호출 */
    UFUNCTION()
    void OnSequenceFinished();
    
private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBoxComponent> TriggerBox;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Battle", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<ACEnemyBossCharacter> TargetBoss;

    UPROPERTY(EditAnywhere, Category = "Boss Battle|Sequence")
    TObjectPtr<ULevelSequence> IntroSequence;
    
    UPROPERTY(Transient)
    TObjectPtr<ALevelSequenceActor> SequenceActor;

    UPROPERTY(Transient)
    TWeakObjectPtr<ACPlayerCharacter> CurrentPlayer;
    
    UPROPERTY(EditAnywhere, Category = "Boss Battle|UI")
    TSubclassOf<UUserWidget> WarningWidgetClass;
    
    UPROPERTY(Transient)
    TWeakObjectPtr<APlayerController> PlayerController;

    UPROPERTY(Transient)
    TWeakObjectPtr<ACStageManager> StageManagerCache;

private:
    
    UPROPERTY(EditAnywhere, Category = "Boss Battle|UI", meta = (ClampMin = "0.0"))
    float WarningDisplayDuration = 2.0f;
    
    UPROPERTY(EditAnywhere, Category = "Boss Battle|Positioning", meta = (ClampMin = "0.0"))
    float PlayerDistanceFromBoss = 1500.0f;
    
    UPROPERTY(EditAnywhere, Category = "Boss Battle|Timing", meta = (ClampMin = "0.0"))
    float DelayBeforeSequence = 1.0f;
    
    /** 인트로를 스킵할지 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Battle", meta = (AllowPrivateAccess = "true"))
    bool bSkipIntro = false;
    
    /** 에디터/디버그용: 수동으로 트리거 발동 */
    UFUNCTION(BlueprintCallable, Category = "Boss Battle", meta = (DevelopmentOnly))
    void ManualTrigger();

    /** 트리거 활성화/비활성화 */
    UFUNCTION(BlueprintCallable, Category = "Boss Battle")
    void SetTriggerEnabled(bool bEnabled);
    
    /** 한 번만 트리거되도록 설정 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Battle", meta = (AllowPrivateAccess = "true"))
    bool bTriggerOnce = true;

    /** 트리거가 이미 발동되었는지 여부 */
    UPROPERTY(VisibleInstanceOnly, Category = "Boss Battle")
    bool bHasTriggered = false;

    /** 트리거 활성화 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Battle", meta = (AllowPrivateAccess = "true"))
    bool bIsEnabled = true;

    bool bOriginalUsePawnControlRotation = true;
    bool bOriginalUseControllerRotationYaw = false;
    bool bUsedTrigger = false;

    FString InitialBossName;
    
    FTimerHandle WarningTimerHandle;
    FTimerHandle SequenceTimerHandle;
    
public :
    UFUNCTION(BlueprintCallable, Category = "Boss Battle")
    void ResetTrigger();
};
