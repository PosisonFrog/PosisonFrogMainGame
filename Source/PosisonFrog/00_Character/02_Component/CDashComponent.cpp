// Fill out your copyright notice in the Description page of Project Settings.


#include "00_Character/02_Component/CDashComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Global.h"


UCDashComponent::UCDashComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}


void UCDashComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // 소유자 캐릭터 찾기
    OwnerCharacter = Cast<ACharacter>(GetOwner());
    
    if (OwnerCharacter)
    {
        MovementComponent = OwnerCharacter->GetCharacterMovement();
        if (MovementComponent)
        {
            OriginalMaxSpeed = MovementComponent->MaxWalkSpeed;
        }
    }
    
    CurrentDashCount = MaxDashCount;
    
    // 입력 방향 초기화
    CurrentInputDirection = FVector::ZeroVector;
}


void UCDashComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 대시 중이 아니면 Tick 비활성화
    if (!bIsDashing)
    {
        SetComponentTickEnabled(false);
        return;
    }

    if (OwnerCharacter && MovementComponent)
    {
        // 대시 시간 업데이트
        DashElapsedTime += DeltaTime;
        float NormalizedTime = FMath::Min(DashElapsedTime / DashDuration, 1.0f);
        
        // 현재 위치와 목표 위치 사이의 거리 계산
        FVector CurrentLocation = OwnerCharacter->GetActorLocation();
        float DistanceToTarget = FVector::Dist2D(CurrentLocation, DashTargetLocation);

        // 목표 위치에 도달했거나 가까워지면 대시 종료
        if (DistanceToTarget < 50.0f || DashElapsedTime >= DashDuration)
        {
            StopDash();
            return;
        }
        
        // 벡터 투영을 통해 목표를 지나쳤는지 확인
        FVector DashVector = DashTargetLocation - CurrentLocation;
        FVector VelocityDirection = MovementComponent->Velocity.GetSafeNormal();
        
        float DotProduct = FVector::DotProduct(DashVector.GetSafeNormal(), VelocityDirection);
        if (DotProduct < 0) // 목표를 지나침
        {
            StopDash();
            return;
        }
        
        // 속도 조절 - 커브 기반 또는 부드러운 감속 사용
        float SpeedMultiplier;
        if (DashSpeedCurve)
        {
            // 커브 기반 속도 조절
            SpeedMultiplier = DashSpeedCurve->GetFloatValue(NormalizedTime);
        }
        else
        {
            // 기본 속도 조절 (시작은 빠르고 끝에는 느림)
            // easeOutExpo 공식: y = 1 - pow(2, -10x)
            SpeedMultiplier = 1.0f - FMath::Pow(2.0f, -10.0f * NormalizedTime);
        }
        
        // 현재 방향은 유지하면서 속도만 조절
        FVector CurrentVelocityDirection = MovementComponent->Velocity.GetSafeNormal();
        float CurrentSpeed = InitialDashSpeed * (1.0f - 0.3f * SpeedMultiplier); // 마지막에는 70%까지 감속
        
        MovementComponent->Velocity = CurrentVelocityDirection * CurrentSpeed;
    }
}

void UCDashComponent::StartDash()
{
    if (!CanDash())
        return;

    // 이미 대시 중이면 타이머 초기화
    if (bIsDashing)
    {
        GetWorld()->GetTimerManager().ClearTimer(DashTimerHandle);
    }

    // 대시 시작
    bIsDashing = true;
    CurrentDashCount--;
    DashStartTime = GetWorld()->GetTimeSeconds();
    DashElapsedTime = 0.0f;

    if (!OwnerCharacter || !MovementComponent)
        return;

    // 현재 지상/공중 상태 저장
    bWasInAir = MovementComponent->IsFalling();

    // 원래 속도 저장
    OriginalMaxSpeed = MovementComponent->MaxWalkSpeed;
    OriginalGravityScale = MovementComponent->GravityScale;

    // 대시 방향 결정
    FVector DashDirection;

    // 입력 방향이 있으면 그 방향으로 대시
    if (CurrentInputDirection.SizeSquared() > 0.1f)
    {
        DashDirection = CurrentInputDirection.GetSafeNormal2D();
    }
    // 입력이 없지만 이동 중이면 현재 속도 방향으로 대시
    else if (MovementComponent->Velocity.SizeSquared() > 1.0f)
    {
        DashDirection = MovementComponent->Velocity.GetSafeNormal2D();
    }
    // 아무 입력도 없으면 캐릭터가 바라보는 방향으로 대시
    else
    {
        DashDirection = OwnerCharacter->GetActorForwardVector();
    }
    
    // 공중 상태에 따라 다른 값 적용
    float ActualDashDistance = DashDistance;
    float ActualDashSpeed = DashSpeed;

    if (bWasInAir)
    {
        // 공중 대시 설정
        ActualDashDistance *= AirDashDistanceModifier;
        ActualDashSpeed *= AirDashSpeedModifier;
        MovementComponent->GravityScale = 0.0f; // 공중에서는 중력 제거
        
        // 공중 대시 애니메이션 재생
        if (AirDashMontage)
        {
            OwnerCharacter->PlayAnimMontage(AirDashMontage);
        }
        else if (DashMontage)
        {
            OwnerCharacter->PlayAnimMontage(DashMontage);
        }
    }
    else
    {
        // 지상 대시 설정
        MovementComponent->SetMovementMode(MOVE_Flying); // 지상에서는 Flying 모드로 변경하여 마찰 제거
        
        // 지상 대시 애니메이션 재생
        if (GroundDashMontage)
        {
            OwnerCharacter->PlayAnimMontage(GroundDashMontage);
        }
        else if (DashMontage)
        {
            OwnerCharacter->PlayAnimMontage(DashMontage);
        }
    }

    // 대시 초기 속도 설정 (커브의 시작 값이나 최대값)
    InitialDashSpeed = ActualDashSpeed;
    
    // LineTrace로 대시 목표 위치 결정
    FVector StartLocation = OwnerCharacter->GetActorLocation();
    FVector EndLocation = StartLocation + DashDirection * ActualDashDistance;

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter);

    // LineTrace 실행
    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        StartLocation,
        EndLocation,
        ECC_Visibility,
        QueryParams
    );

    // 충돌 지점이 있으면 그 지점까지만 이동
    if (bHit)
    {
        DashTargetLocation = HitResult.Location - DashDirection * 50.0f; // 충돌 지점보다 약간 앞에서 멈춤
    }
    else
    {
        DashTargetLocation = EndLocation;
    }

    // 디버그 라인 그리기
    DrawDebugLine(
        GetWorld(),
        StartLocation,
        DashTargetLocation,
        FColor::Green,
        false,
        2.0f,
        0,
        1.0f
    );

    // 대시 이동 설정 - 초기 속도는 최대치로
    MovementComponent->Velocity = DashDirection * InitialDashSpeed;

    // 대시 시작 이벤트 발생
    OnDashStarted.Broadcast();

    // 대시 종료 체크를 위해 Tick 사용
    SetComponentTickEnabled(true);
}



void UCDashComponent::StopDash()
{
    if (!bIsDashing)
        return;

    bIsDashing = false;
    SetComponentTickEnabled(false);

    if (MovementComponent)
    {
        // 원래 이동 모드로 복원
        if (!bWasInAir)
        {
            MovementComponent->SetMovementMode(MOVE_Walking);
            
            // 지상에서는 일정 속도를 유지 (현재 속도의 X, Y 값 보존)
            FVector CurrentVelocity = MovementComponent->Velocity;
            // 너무 빠른 속도는 제한 (OriginalMaxSpeed의 80%까지만 유지)
            float MaxHorizontalSpeed = OriginalMaxSpeed * 0.8f;
            float CurrentHorizontalSpeed = FMath::Min(CurrentVelocity.Size2D(), MaxHorizontalSpeed);
            
            if (CurrentHorizontalSpeed > 0.1f)
            {
                FVector Direction = CurrentVelocity.GetSafeNormal2D();
                MovementComponent->Velocity = FVector(
                    Direction.X * CurrentHorizontalSpeed, 
                    Direction.Y * CurrentHorizontalSpeed, 
                    MovementComponent->Velocity.Z
                );
            }
        }
        else
        {
            // 공중에서는 X, Y 속도를 0으로 리셋
            MovementComponent->Velocity = FVector(0, 0, MovementComponent->Velocity.Z);
        }

        // 원래 최대 속도와 중력 복원
        MovementComponent->MaxWalkSpeed = OriginalMaxSpeed;
        MovementComponent->GravityScale = OriginalGravityScale;
    }

    // 몽타주 중지 (블렌드 아웃으로 부드럽게)
    if (OwnerCharacter)
    {
        OwnerCharacter->StopAnimMontage(DashMontage);
        OwnerCharacter->StopAnimMontage(GroundDashMontage);
        OwnerCharacter->StopAnimMontage(AirDashMontage);
    }

    // 쿨다운 시작
    bInCooldown = true;

    // 대시 종료 이벤트 발생
    OnDashEnded.Broadcast();

    // 쿨다운 타이머 설정
    GetWorld()->GetTimerManager().SetTimer(CooldownTimerHandle, [this]()
    {
        bInCooldown = false;
        CurrentDashCount = FMath::Min(CurrentDashCount + 1, MaxDashCount);
    }, DashCooldown, false);
}

bool UCDashComponent::CanDash() const
{
	if (!OwnerCharacter || !MovementComponent)
		return false;

	// 대시 가능 조건 확인 - 달리기 중에도 가능하도록 조건 제거
	bool bHasCharges = CurrentDashCount > 0;
	bool bNotInCooldown = !bInCooldown;
	bool bCanAirDashCheck = bCanAirDash || !MovementComponent->IsFalling();

	return bHasCharges && bNotInCooldown && bCanAirDashCheck;
}