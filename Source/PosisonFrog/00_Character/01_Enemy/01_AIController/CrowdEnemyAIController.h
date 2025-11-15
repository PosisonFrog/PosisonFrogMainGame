// CrowdEnemyAIController.h
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Navigation/CrowdManager.h"
#include "CrowdEnemyAIController.generated.h"

/**
 * CEnemyCharacterBase의 FSM을 지원하는 군중(Crowd) AI 컨트롤러입니다.
 * 생성자에서 CrowdFollowingComponent를 사용하고, OnPossess에서 군중 이동 품질을 설정합니다.
 * ACTacticalEnemyAIController의 부모 클래스로 사용됩니다.(기존 AIController 참고해서 만들었긴했음.)
 * 
 * 주요 기능:
 * - Detour Crowd Manager를 통한 다중 에이전트 경로찾기
 * - 상호 회피 및 충돌 방지
 * - 군집 이동 시 자연스러운 분산
 */
UCLASS()
class POSISONFROG_API ACrowdEnemyAIController : public AAIController
{
    GENERATED_BODY()

public:
    ACrowdEnemyAIController(const FObjectInitializer& ObjectInitializer);

    /** 컨트롤러가 다시 Possess할 때 CrowdFollowingComponent 설정을 다시 적용합니다. */
    void ReinitializeCrowdComponent();

protected:
    /**
     * Pawn에 빙의될 때 CrowdFollowingComponent의 품질 설정을 적용합니다.
     */
    virtual void OnPossess(APawn* InPawn) override;

protected:
    // ───────── Crowd 기본 설정 ─────────
    

     // 군중 회피 품질 (0=Off, 1=Low, 2=Medium, 3=Good, 4=High)
     //ECrowdAvoidanceQuality 대신 int32 사용
     
    UPROPERTY(EditDefaultsOnly, Category = "Crowd Settings", meta = (ClampMin = "0", ClampMax = "4"))
    int32 CrowdAvoidanceQuality = 2; // Medium
    
    /** 다른 에이전트와 충돌을 피하려는 강도 (0: 무시, 높을수록 강하게 회피) */
    UPROPERTY(EditDefaultsOnly, Category = "Crowd Settings", meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float CrowdSeparationWeight = 2.0f;
    
    /** 경로 최적화를 시도하는 범위. 에이전트가 경로에서 얼마나 멀리 벗어날 수 있는지 결정 */
    UPROPERTY(EditDefaultsOnly, Category = "Crowd Settings", meta = (ClampMin = "0.0"))
    float CrowdPathOptimizationRange = 1000.0f;
    
    /** 다른 에이전트와의 충돌을 감지하기 위해 검사하는 범위 */
    UPROPERTY(EditDefaultsOnly, Category = "Crowd Settings", meta = (ClampMin = "0.0"))
    float CrowdCollisionQueryRange = 600.0f;
    
    /** 회피 범위 배율 (기본값의 배수) */
    UPROPERTY(EditDefaultsOnly, Category = "Crowd Settings", meta = (ClampMin = "0.1", ClampMax = "2.0"))
    float CrowdAvoidanceRangeMultiplier = 1.0f;
    
    // ───────── Crowd 고급 설정 ─────────
    
    /** 코너를 예측하여 미리 회전 시작 */
    UPROPERTY(EditDefaultsOnly, Category = "Crowd Settings|Advanced")
    bool bCrowdAnticipateTurns = true;
    
    /** 장애물 회피 활성화 */
    UPROPERTY(EditDefaultsOnly, Category = "Crowd Settings|Advanced")
    bool bCrowdObstacleAvoidance = true;
    
    /** 다른 에이전트와의 분리 활성화 */
    UPROPERTY(EditDefaultsOnly, Category = "Crowd Settings|Advanced")
    bool bCrowdSeparation = true;
    
    /** 목표 지점 도달 시 감속 */
    UPROPERTY(EditDefaultsOnly, Category = "Crowd Settings|Advanced")
    bool bCrowdSlowdownAtGoal = true;
    
    // ───────── 회피 그룹 설정 ─────────
    
    /** 이 에이전트가 속한 회피 그룹 (0-31) */
    UPROPERTY(EditDefaultsOnly, Category = "Crowd Settings|Avoidance Groups", meta = (ClampMin = "0", ClampMax = "31"))
    int32 AvoidanceGroup = 0;
    
    /** 회피해야 할 그룹들 (비트마스크) */
    UPROPERTY(EditDefaultsOnly, Category = "Crowd Settings|Avoidance Groups")
    int32 GroupsToAvoid = -1;  // -1 = 모든 그룹 회피
    
    /** 무시할 그룹들 (비트마스크) */
    UPROPERTY(EditDefaultsOnly, Category = "Crowd Settings|Avoidance Groups")
    int32 GroupsToIgnore = 0;  // 0 = 무시할 그룹 없음
};