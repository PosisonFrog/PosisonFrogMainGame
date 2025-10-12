// Source/PosisonFrog/00_Character/01_Enemy/AI/CTacticalEnemyAIController.h
#pragma once

#include "CoreMinimal.h"
#include "CrowdEnemyAIController.h"                 // 이전에 드린 기본/크라우드 컨트롤러 기반
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "CTacticalEnemyAIController.generated.h"

/** 전술 모드 */
UENUM()
enum class ETacticalMode : uint8
{
    None,
    ChaseRing,
    Strafe,
    Flank,
    Retreat,
    Reposition
};

/** 스트레이프 설정 */
USTRUCT()
struct FTacticalStrafe
{
    GENERATED_BODY()

    UPROPERTY() TWeakObjectPtr<AActor> Target = nullptr;
    UPROPERTY() float Radius = 300.f;
    UPROPERTY() float AngularSpeedDegPerSec = 90.f;  // +:좌/ -:우
    UPROPERTY() float Duration = 0.8f;
    UPROPERTY() float AcceptanceRadius = 120.f;
    UPROPERTY() float AccumAngleDeg = 0.f;           // 내부 누적각
    UPROPERTY() float EndTime = 0.f;
};

/** 링 추적 설정 */
USTRUCT()
struct FTacticalChaseRing
{
    GENERATED_BODY()

    UPROPERTY() TWeakObjectPtr<AActor> Target = nullptr;
    UPROPERTY() float Radius = 300.f;
    UPROPERTY() float AngleDeg = 0.f;                // 슬롯 각
    UPROPERTY() float AcceptanceRadius = 120.f;
    UPROPERTY() bool  bKeepFocus = true;
};

/** 플랭크 설정 */
USTRUCT()
struct FTacticalFlank
{
    GENERATED_BODY()

    UPROPERTY() TWeakObjectPtr<AActor> Target = nullptr;
    UPROPERTY() float Radius = 300.f;
    UPROPERTY() bool  bLeft = true;                  // true: 좌, false: 우
    UPROPERTY() float AcceptanceRadius = 120.f;
};

/** 후퇴 설정 */
USTRUCT()
struct FTacticalRetreat
{
    GENERATED_BODY()

    UPROPERTY() FVector RetreatPoint = FVector::ZeroVector;
    UPROPERTY() float AcceptanceRadius = 120.f;
};

/** 재배치 설정 */
USTRUCT()
struct FTacticalReposition
{
    GENERATED_BODY()

    UPROPERTY() TWeakObjectPtr<AActor> Target = nullptr;
    UPROPERTY() float Radius = 300.f;
    UPROPERTY() float AngleJitterDeg = 30.f;
    UPROPERTY() int32 MaxTries = 5;
    UPROPERTY() float AcceptanceRadius = 120.f;
};

UCLASS()
class POSISONFROG_API ACTacticalEnemyAIController : public ACrowdEnemyAIController
{
    GENERATED_BODY()
public:
    ACTacticalEnemyAIController(const FObjectInitializer& Obj = FObjectInitializer::Get());

    // ───────── 외부에서 호출하는 전술 API ─────────
    void TacticalStop();

    /** 타깃을 둘러싸는 링 목표점으로 추적(줄서기 완화) */
    void TacticalChaseRing(AActor* Target, float Radius, float AcceptanceRadius = 120.f,
                           bool bKeepFocus = true, float FixedAngleDeg = NAN);

    /** 타깃 기준 원운동(좌/우) */
    void TacticalStrafe(AActor* Target, float Radius, float AngularSpeedDegPerSec, float Duration,
                        float AcceptanceRadius = 120.f);

    /** 좌/우 측면 진입 */
    void TacticalFlank(AActor* Target, float Radius, bool bLeft, float AcceptanceRadius = 120.f);

    /** 지정 지점으로 후퇴 */
    void TacticalRetreatTo(const FVector& RetreatPoint, float AcceptanceRadius = 120.f);

    /** 혼잡/막힘 시 인근에 재배치 후 재시도 */
    void TacticalRepositionAround(AActor* Target, float Radius, float AngleJitterDeg, int32 MaxTries,
                                  float AcceptanceRadius = 120.f);

    /** 전술 중 타깃 포커스 유지/해제 */
    void SetTacticalFocus(AActor* Target, bool bEnable);

    /** 리스(Leash) 설정: 홈 위치/최대거리(넘으면 후퇴 권장) */
    void SetLeash(const FVector& Home, float MaxDistance);

protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

    // ───────── 내부 구현 ─────────
    FVector ComputeRingSlot(AActor* Target, float Radius, float AngleDeg) const;
    float   ComputeAutoAngleDeg() const;                 // 개체별 고유 각도 시드
    bool    ProjectToNav(const FVector& In, FVector& Out) const;
    void    IssueMoveTo(const FVector& Goal, float AcceptanceRadius);
    void    RefreshFocus();

private:
    // 모드/설정
    UPROPERTY() ETacticalMode Mode = ETacticalMode::None;
    UPROPERTY() FTacticalStrafe     Strafe;
    UPROPERTY() FTacticalChaseRing  ChaseRing;
    UPROPERTY() FTacticalFlank      Flank;
    UPROPERTY() FTacticalRetreat    Retreat;
    UPROPERTY() FTacticalReposition Repos;

    /** 재배치 후 복귀해야 할 이전 모드(없으면 None) */
    UPROPERTY() ETacticalMode ModeBeforeReposition = ETacticalMode::None;

    /** 재배치 완료 시 이전 모드로 복귀할지 여부 */
    UPROPERTY() bool bResumeModeAfterReposition = false;
    
    // 공통 파라미터
    UPROPERTY(EditAnywhere, Category="PF|AI|Tactical")
    float RepathInterval = 0.25f;                 // 목표 재평가 주기

    UPROPERTY(EditAnywhere, Category="PF|AI|Tactical")
    float MoveGoalTolerance = 35.f;               // 리퀘스트 재발행 임계치

    UPROPERTY(EditAnywhere, Category="PF|AI|Tactical")
    float RingAngleJitterDeg = 20.f;              // 자동 각도 지터

    UPROPERTY(EditAnywhere, Category="PF|AI|Tactical")
    bool bAutoFocusOnTarget = true;

    // 내부 상태
    FVector LastIssuedGoal = FVector::ZeroVector;
    float   NextRepathTime = 0.f;
    int32   ConsecutiveFails = 0;

    // 리스
    bool    bUseLeash = false;
    FVector HomePos = FVector::ZeroVector;
    float   LeashMaxDist = 0.f;
};
