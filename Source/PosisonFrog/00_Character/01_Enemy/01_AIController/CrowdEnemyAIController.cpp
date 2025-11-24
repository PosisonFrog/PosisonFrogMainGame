
#include "CrowdEnemyAIController.h"
#include "00_Character/01_Enemy/CEnemyCharacterBase.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Navigation/CrowdManager.h"
#include "NavigationSystem.h"
#include "Global.h"

ACrowdEnemyAIController::ACrowdEnemyAIController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{
    PrimaryActorTick.bCanEverTick = false; // Tick은 TacticalController에서 필요하므로 여기서는 꺼도 됩니다.
}

void ACrowdEnemyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    ReinitializeCrowdComponent(); // Possess 시 초기화 함수 호출

    // 컨트롤러가 올바른 Pawn에 빙의되었는지 확인합니다. (디버깅용 로그)
    if (Cast<ACEnemyCharacterBase>(InPawn))
    {
        //CLog::Print(*InPawn->GetName(), 1, 2.f, FColor::Yellow);
        //CLog::Print(TEXT("ACrowdEnemyAIController가 위 폰에 빙의함."), 1, 2.f, FColor::Yellow);
    }
}

void ACrowdEnemyAIController::ReinitializeCrowdComponent()
{
    // CrowdFollowingComponent 설정 적용
    if (UCrowdFollowingComponent* CrowdComp = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
    {
        // 이 컴포넌트가 유효한지 먼저 확인
        if (!IsValid(CrowdComp)) return;

        // 회피 품질 설정 (int32를 ECrowdAvoidanceQuality로 캐스팅)
        CrowdComp->SetCrowdAvoidanceQuality(static_cast<ECrowdAvoidanceQuality::Type>(CrowdAvoidanceQuality));
        
        // ... (OnPossess에 있던 모든 CrowdComp 설정 코드를 그대로 여기로 옮깁니다) ...
        CrowdComp->SetCrowdAvoidanceRangeMultiplier(CrowdAvoidanceRangeMultiplier);
        CrowdComp->SetCrowdCollisionQueryRange(CrowdCollisionQueryRange);
        CrowdComp->SetCrowdSeparationWeight(CrowdSeparationWeight);
        CrowdComp->SetCrowdPathOptimizationRange(CrowdPathOptimizationRange);
        CrowdComp->SetCrowdAnticipateTurns(bCrowdAnticipateTurns);
        CrowdComp->SetCrowdObstacleAvoidance(bCrowdObstacleAvoidance);
        CrowdComp->SetCrowdSeparation(bCrowdSeparation);
        CrowdComp->SetCrowdSlowdownAtGoal(bCrowdSlowdownAtGoal);
        CrowdComp->SetAvoidanceGroup(AvoidanceGroup);
        CrowdComp->SetGroupsToAvoid(GroupsToAvoid);
        CrowdComp->SetGroupsToIgnore(GroupsToIgnore);

        UE_LOG(LogTemp, Log, TEXT("[%s]의 CrowdFollowingComponent가 재초기화되었습니다."), *GetNameSafe(GetPawn()));
    }
}