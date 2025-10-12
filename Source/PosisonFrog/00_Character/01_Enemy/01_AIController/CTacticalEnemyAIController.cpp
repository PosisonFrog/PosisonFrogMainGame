// Source/PosisonFrog/00_Character/01_Enemy/AI/CTacticalEnemyAIController.cpp
#include "CTacticalEnemyAIController.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

ACTacticalEnemyAIController::ACTacticalEnemyAIController(const FObjectInitializer& Obj)
    : Super(Obj)
{
    bAttachToPawn = true;
    bStartAILogicOnPossess = true;
    PrimaryActorTick.bCanEverTick = true;
}

void ACTacticalEnemyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    // Crowd 품질은 부모(ACrowdEnemyAIController)에서 설정됩니다.
    // 필요 시 여기에서 추가 보정 가능.
    ConsecutiveFails = 0;
    NextRepathTime   = 0.f;

    if (!HomePos.IsNearlyZero())
        bUseLeash = true;
}

void ACTacticalEnemyAIController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetPawn()) return;

    const float Now = GetWorld()->GetTimeSeconds();
    if (Now < NextRepathTime) return;
    NextRepathTime = Now + RepathInterval;

    switch (Mode)
    {
    case ETacticalMode::ChaseRing:
        if (ChaseRing.Target.IsValid())
        {
            const float Angle = FMath::IsFinite(ChaseRing.AngleDeg)
                              ? ChaseRing.AngleDeg
                              : ComputeAutoAngleDeg();

            FVector Goal = ComputeRingSlot(ChaseRing.Target.Get(), ChaseRing.Radius, Angle);
            if (!ProjectToNav(Goal, Goal)) { ++ConsecutiveFails; break; }

            const float Dist2D = FVector::Dist2D(Goal, LastIssuedGoal);
            if (Dist2D > MoveGoalTolerance)
            {
                IssueMoveTo(Goal, ChaseRing.AcceptanceRadius);
            }

            if (ChaseRing.bKeepFocus && bAutoFocusOnTarget)
                SetFocus(ChaseRing.Target.Get());
        }
        break;

    case ETacticalMode::Strafe:
        if (Strafe.Target.IsValid())
        {
            // 종료 타이밍
            if (Now >= Strafe.EndTime) { TacticalStop(); break; }

            Strafe.AccumAngleDeg = FMath::Fmod(Strafe.AccumAngleDeg + Strafe.AngularSpeedDegPerSec * RepathInterval, 360.f);

            const FVector TargetLoc = Strafe.Target->GetActorLocation();
            const float   BaseYaw   = Strafe.Target->GetActorRotation().Yaw;
            const float   Angle     = BaseYaw + Strafe.AccumAngleDeg;

            FVector Goal = ComputeRingSlot(Strafe.Target.Get(), Strafe.Radius, Angle);
            if (!ProjectToNav(Goal, Goal)) { ++ConsecutiveFails; break; }

            if (FVector::Dist2D(Goal, LastIssuedGoal) > MoveGoalTolerance)
                IssueMoveTo(Goal, Strafe.AcceptanceRadius);

            if (bAutoFocusOnTarget) SetFocus(Strafe.Target.Get());
        }
        break;

    case ETacticalMode::Flank:
        if (Flank.Target.IsValid())
        {
            const FVector TargetLoc = Flank.Target->GetActorLocation();
            const float   TargetYaw = Flank.Target->GetActorRotation().Yaw;

            // 좌: +90, 우: -90
            const float OffsetYaw = Flank.bLeft ? +90.f : -90.f;
            const float Angle     = TargetYaw + OffsetYaw;

            FVector Goal = ComputeRingSlot(Flank.Target.Get(), Flank.Radius, Angle);
            if (!ProjectToNav(Goal, Goal)) { ++ConsecutiveFails; break; }

            if (FVector::Dist2D(Goal, LastIssuedGoal) > MoveGoalTolerance)
                IssueMoveTo(Goal, Flank.AcceptanceRadius);

            if (bAutoFocusOnTarget) SetFocus(Flank.Target.Get());
        }
        break;

    case ETacticalMode::Retreat:
    {
        FVector Goal = Retreat.RetreatPoint;
        if (!ProjectToNav(Goal, Goal)) { ++ConsecutiveFails; break; }

        if (FVector::Dist2D(Goal, LastIssuedGoal) > MoveGoalTolerance)
            IssueMoveTo(Goal, Retreat.AcceptanceRadius);

        ClearFocus(EAIFocusPriority::Gameplay);
        break;
    }

    case ETacticalMode::Reposition:
        if (Repos.Target.IsValid())
        {
            const float BaseYaw = Repos.Target->GetActorRotation().Yaw;
            bool bMoved = false;
 
            const int32 NumTries = FMath::Max(Repos.MaxTries, 1);
            const float SweepDeg = Repos.AngleJitterDeg * 2.f;

            for (int32 i = 0; i < NumTries; ++i)
            {
                const float Progress = (NumTries > 1)
                                         ? static_cast<float>(i) / static_cast<float>(NumTries - 1)
                                         : 0.f;
                const float BaseOffset = (Progress * SweepDeg) - Repos.AngleJitterDeg;
                const float Jitter = FMath::FRandRange(-Repos.AngleJitterDeg, +Repos.AngleJitterDeg);
                const float Yaw    = BaseYaw + BaseOffset + Jitter;
                FVector Goal = ComputeRingSlot(Repos.Target.Get(), Repos.Radius, Yaw);
                if (!ProjectToNav(Goal, Goal)) continue;

                IssueMoveTo(Goal, Repos.AcceptanceRadius);
                bMoved = true;
                break;
            }

            if (!bMoved) ++ConsecutiveFails;
            if (bAutoFocusOnTarget) SetFocus(Repos.Target.Get());
        }
        break;

    default: break;
    }

    // 리스 체크(선택): 너무 멀어지면 후퇴 모드로
    if (bUseLeash && Mode != ETacticalMode::Retreat)
    {
        const float D = FVector::Dist2D(GetPawn()->GetActorLocation(), HomePos);
        if (D > LeashMaxDist)
        {
            TacticalRetreatTo(HomePos, 120.f);
        }
    }
}

void ACTacticalEnemyAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
    Super::OnMoveCompleted(RequestID, Result);

    if (Result.Code == EPathFollowingResult::Success)
    {
        ConsecutiveFails = 0;

        if (Mode == ETacticalMode::Reposition)
        {
            if (bResumeModeAfterReposition)
            {
                Mode = ModeBeforeReposition;
                ModeBeforeReposition = ETacticalMode::None;
                bResumeModeAfterReposition = false;
                LastIssuedGoal = FVector::ZeroVector;
                NextRepathTime = 0.f;
                RefreshFocus();
            }
            else
            {
                TacticalStop();
            }
        }
    }
    else
    {
        ++ConsecutiveFails;

        // 연속 실패 시 재배치 시도
        if (ConsecutiveFails >= 3)
        {
            ConsecutiveFails = 0;

            if (Mode == ETacticalMode::ChaseRing && ChaseRing.Target.IsValid())
            {
                TacticalRepositionAround(ChaseRing.Target.Get(), ChaseRing.Radius, RingAngleJitterDeg, 5, ChaseRing.AcceptanceRadius);
            }
            else if (Mode == ETacticalMode::Flank && Flank.Target.IsValid())
            {
                TacticalRepositionAround(Flank.Target.Get(), Flank.Radius, RingAngleJitterDeg, 5, Flank.AcceptanceRadius);
            }
        }
    }
}

// ───────── 전술 API 구현 ─────────
void ACTacticalEnemyAIController::TacticalStop()
{
    Mode = ETacticalMode::None;
    StopMovement();
    ClearFocus(EAIFocusPriority::Gameplay);
    LastIssuedGoal = FVector::ZeroVector;
}

void ACTacticalEnemyAIController::TacticalChaseRing(AActor* Target, float Radius, float AcceptanceRadius,
                                                    bool bKeepFocus, float FixedAngleDeg)
{
    if (!Target) { TacticalStop(); return; }

    Mode = ETacticalMode::ChaseRing;
    ChaseRing.Target = Target;
    ChaseRing.Radius = Radius;
    ChaseRing.AcceptanceRadius = AcceptanceRadius;
    ChaseRing.bKeepFocus = bKeepFocus;

    if (FMath::IsFinite(FixedAngleDeg))
        ChaseRing.AngleDeg = FixedAngleDeg;
    else
        ChaseRing.AngleDeg = NAN; // 자동 각도 시드 사용

    RefreshFocus();
}

void ACTacticalEnemyAIController::TacticalStrafe(AActor* Target, float Radius, float AngularSpeedDegPerSec,
                                                 float Duration, float AcceptanceRadius)
{
    if (!Target) { TacticalStop(); return; }

    Mode = ETacticalMode::Strafe;
    Strafe.Target = Target;
    Strafe.Radius = Radius;
    Strafe.AngularSpeedDegPerSec = AngularSpeedDegPerSec;
    Strafe.Duration = Duration;
    Strafe.AcceptanceRadius = AcceptanceRadius;
    Strafe.AccumAngleDeg = 0.f;
    Strafe.EndTime = GetWorld()->GetTimeSeconds() + Duration;

    RefreshFocus();
}

void ACTacticalEnemyAIController::TacticalFlank(AActor* Target, float Radius, bool bLeft, float AcceptanceRadius)
{
    if (!Target) { TacticalStop(); return; }

    Mode = ETacticalMode::Flank;
    Flank.Target = Target;
    Flank.Radius = Radius;
    Flank.bLeft  = bLeft;
    Flank.AcceptanceRadius = AcceptanceRadius;

    RefreshFocus();
}

void ACTacticalEnemyAIController::TacticalRetreatTo(const FVector& RetreatPoint, float AcceptanceRadius)
{
    Mode = ETacticalMode::Retreat;
    Retreat.RetreatPoint = RetreatPoint;
    Retreat.AcceptanceRadius = AcceptanceRadius;

    ClearFocus(EAIFocusPriority::Gameplay);
}

void ACTacticalEnemyAIController::TacticalRepositionAround(AActor* Target, float Radius, float AngleJitterDeg,
                                                           int32 MaxTries, float AcceptanceRadius)
{
    if (!Target) { TacticalStop(); return; }

    if (Mode != ETacticalMode::Reposition)
    {
        ModeBeforeReposition = Mode;
        bResumeModeAfterReposition = (ModeBeforeReposition != ETacticalMode::None);
    }

    Mode = ETacticalMode::Reposition;
    Repos.Target = Target;
    Repos.Radius = Radius;
    Repos.AngleJitterDeg = AngleJitterDeg;
    Repos.MaxTries = FMath::Max(MaxTries, 1);
    Repos.AcceptanceRadius = AcceptanceRadius;

    LastIssuedGoal = FVector::ZeroVector;
    NextRepathTime = 0.f;

    RefreshFocus();
}

void ACTacticalEnemyAIController::SetTacticalFocus(AActor* Target, bool bEnable)
{
    if (bEnable && Target) SetFocus(Target);
    else ClearFocus(EAIFocusPriority::Gameplay);
}

void ACTacticalEnemyAIController::SetLeash(const FVector& Home, float MaxDistance)
{
    HomePos = Home; LeashMaxDist = MaxDistance; bUseLeash = (MaxDistance > 0.f);
}

// ───────── 내부 유틸 ─────────
FVector ACTacticalEnemyAIController::ComputeRingSlot(AActor* Target, float Radius, float AngleDeg) const
{
    const FVector Center = Target->GetActorLocation();
    const FRotator YawRot(0.f, AngleDeg, 0.f);
    const FVector Dir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
    return Center - Dir * Radius;  // 타깃을 바라보도록 반대방향
}

float ACTacticalEnemyAIController::ComputeAutoAngleDeg() const
{
    // Pawn 주소 해시 기반 고유 시드 + 지터
    const APawn* P = GetPawn();
    const uint32 H = ::GetTypeHash(P);
    const float Base = (float)(H % 360u);
    return FMath::Fmod(Base + FMath::FRandRange(-RingAngleJitterDeg, +RingAngleJitterDeg), 360.f);
}

bool ACTacticalEnemyAIController::ProjectToNav(const FVector& In, FVector& Out) const
{
    if (const UNavigationSystemV1* NS = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
    {
        FNavLocation NL;
        if (NS->ProjectPointToNavigation(In, NL, FVector(100.f,100.f,200.f)))
        {
            Out = NL.Location;
            return true;
        }
    }
    Out = In;
    return false;
}

void ACTacticalEnemyAIController::IssueMoveTo(const FVector& Goal, float AcceptanceRadius)
{
    LastIssuedGoal = Goal;

    FAIMoveRequest Req(Goal);
    Req.SetAcceptanceRadius(AcceptanceRadius);
    Req.SetUsePathfinding(true);
    Req.SetAllowPartialPath(true);

    MoveTo(Req);
}

void ACTacticalEnemyAIController::RefreshFocus()
{
    switch (Mode)
    {
    case ETacticalMode::ChaseRing: if (ChaseRing.bKeepFocus && bAutoFocusOnTarget && ChaseRing.Target.IsValid()) SetFocus(ChaseRing.Target.Get()); break;
    case ETacticalMode::Strafe:    if (bAutoFocusOnTarget && Strafe.Target.IsValid()) SetFocus(Strafe.Target.Get()); break;
    case ETacticalMode::Flank:     if (bAutoFocusOnTarget && Flank.Target.IsValid())  SetFocus(Flank.Target.Get()); break;
    default: break;
    }
}
