
#include "CrowdEnemyAIController.h"
#include "00_Character/01_Enemy/CEnemyCharacterBase.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Navigation/CrowdManager.h"
#include "NavigationSystem.h"
#include "Global.h"

ACrowdEnemyAIController::ACrowdEnemyAIController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{
    PrimaryActorTick.bCanEverTick = false;
}

void ACrowdEnemyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    // CrowdFollowingComponent 설정 적용
    if (UCrowdFollowingComponent* CrowdComp = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
    {
        // 회피 품질 설정 (int32를 ECrowdAvoidanceQuality로 캐스팅)
        // 0=Off, 1=Low, 2=Medium, 3=Good, 4=High
        CrowdComp->SetCrowdAvoidanceQuality(static_cast<ECrowdAvoidanceQuality::Type>(CrowdAvoidanceQuality));
        
        // 회피 범위 설정
        CrowdComp->SetCrowdAvoidanceRangeMultiplier(CrowdAvoidanceRangeMultiplier);
        
        // 충돌 쿼리 범위 설정
        CrowdComp->SetCrowdCollisionQueryRange(CrowdCollisionQueryRange);
        
        // 분리 가중치 설정
        CrowdComp->SetCrowdSeparationWeight(CrowdSeparationWeight);
        
        // 경로 최적화 범위 설정
        CrowdComp->SetCrowdPathOptimizationRange(CrowdPathOptimizationRange);
        
        // 회피 활성화
        CrowdComp->SetCrowdAnticipateTurns(bCrowdAnticipateTurns);
        CrowdComp->SetCrowdObstacleAvoidance(bCrowdObstacleAvoidance);
        CrowdComp->SetCrowdSeparation(bCrowdSeparation);
        
        // 속도 조정 활성화 (혼잡 시 속도 감소)
        CrowdComp->SetCrowdSlowdownAtGoal(bCrowdSlowdownAtGoal);
        
        // 추가 회피 그룹 설정 (필요한 경우)
        CrowdComp->SetAvoidanceGroup(AvoidanceGroup);
        CrowdComp->SetGroupsToAvoid(GroupsToAvoid);
        CrowdComp->SetGroupsToIgnore(GroupsToIgnore);
    }

    // 컨트롤러가 올바른 Pawn에 빙의되었는지 확인합니다. (디버깅용 로그)
    if (Cast<ACEnemyCharacterBase>(InPawn))
    {
        CLog::Print(*InPawn->GetName(), 1, 2.f, FColor::Yellow);
        CLog::Print(TEXT("ACrowdEnemyAIController가 위 폰에 빙의함."), 1, 2.f, FColor::Yellow);

    }
}