#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CBossBattleStartTrigger.generated.h"

class UBoxComponent;
class ACEnemyBossCharacter;
class ACBossStageBarrier;

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

private:
    /** 트리거 박스 컴포넌트 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBoxComponent> TriggerBox;

    /** 시작할 보스 캐릭터 레퍼런스 (레벨에서 설정) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Battle", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<ACEnemyBossCharacter> TargetBoss;
    
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

public :
    /** 리스폰 시 트리거 상태 초기화 */
    UFUNCTION(BlueprintCallable, Category = "Boss Battle")
    void ResetTrigger();
};