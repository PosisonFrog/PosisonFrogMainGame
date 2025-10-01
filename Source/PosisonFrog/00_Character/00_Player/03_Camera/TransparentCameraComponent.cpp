#include "TransparentCameraComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"

#include "Global.h"


UTransparentCameraComponent::UTransparentCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTransparentCameraComponent::SetCameraComponent(UCameraComponent* InCameraComponent)
{
	CameraComponent = InCameraComponent;

}

void UTransparentCameraComponent::SetSpringArmComponent(USpringArmComponent* InSpringArm)
{
	SpringArmComponent = InSpringArm;
	// 스프링암 갱신 시 Idle을 재보정해두면 편함
	CalibrateIdleView();
}

void UTransparentCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacter>(GetOwner());

	// 초기 시점을 Idle로 등록
	CalibrateIdleView();
	PrevViewRotation   = IdleViewRotation;
	LastUserViewMoveTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	bAutoReturning = false;
}

void UTransparentCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	// World가 종료 중이거나 유효하지 않으면 CustomDepth 복원 스킵
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		// 참조를 정리하고 함수를 종료합니다.
		ObstructingComponents_LastFrame.Reset();
		CameraComponent = nullptr;
		SpringArmComponent = nullptr;
		OwnerCharacter = nullptr;
		
		return;
	}
	
	// IsValid() 체크를 통해 유효한 컴포넌트에만 접근
	for (const TObjectPtr<UPrimitiveComponent>& Comp : ObstructingComponents_LastFrame)
	{
		if (IsValid(Comp))
		{
			Comp->SetRenderCustomDepth(true);
		}
	}
	
	// 모든 참조를 정리합니다.
	ObstructingComponents_LastFrame.Reset();
	CameraComponent = nullptr;
	SpringArmComponent = nullptr;
	OwnerCharacter = nullptr;
}

void UTransparentCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateObstructingActors();

	if (bEnableAutoReset)
		UpdateAutoResetView(DeltaTime);
}




// ───────────────────────────────────────────────
// Obstruction 처리
// ───────────────────────────────────────────────
void UTransparentCameraComponent::UpdateObstructingActors()
{
	if (!IsValid(OwnerCharacter) || !IsValid(CameraComponent))
		return;

	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
		return;
	
	TSet<TObjectPtr<UPrimitiveComponent>> ObstructingComponents_CurrentFrame;

	const FVector StartLocation = CameraComponent->GetComponentLocation();
	const FVector EndLocation = OwnerCharacter->GetActorLocation();

	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TransparentCam), false, OwnerCharacter);
	Params.AddIgnoredActor(OwnerCharacter);

	GetWorld()->LineTraceMultiByChannel(Hits, StartLocation, EndLocation, TraceChannel, Params);

	for (const FHitResult& Hit : Hits)
	{
		UPrimitiveComponent* HitComp = Hit.GetComponent();
		if (IsValid(HitComp)&& HitComp->IsVisible())
		{
			ObstructingComponents_CurrentFrame.Add(HitComp);
			HitComp->SetRenderCustomDepth(false);

			if (bDebugTrace)
			{
				DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 8.f, FColor::Green, false, 0.06f);
				DrawDebugLine(GetWorld(), StartLocation, EndLocation, FColor::Cyan, false, 0.06f, 0, 0.5f);
			}
		}
	}
	
	// 이전 프레임에만 있던 것들 복원
	TSet<TObjectPtr<UPrimitiveComponent>> ToRestore = ObstructingComponents_LastFrame.Difference(ObstructingComponents_CurrentFrame);
	for (UPrimitiveComponent* Comp : ToRestore)
	{
		if (IsValid(Comp))
			Comp->SetRenderCustomDepth(true);
			Comp->SetCustomDepthStencilValue(StencilValue); 
	}
	ObstructingComponents_LastFrame = MoveTemp(ObstructingComponents_CurrentFrame);
}



// ───────────────────────────────────────────────
// Auto Reset View

void UTransparentCameraComponent::CalibrateIdleView()
{
	//처음 Idle값 : 시작 시 또는 필요 시점에 현재 뷰 각도를 저장함.
	if (bUseControllerRotation)
	{
		if (OwnerCharacter && OwnerCharacter->GetController())
		{
			IdleViewRotation = OwnerCharacter->GetController()->GetControlRotation();
		}
		else
		{
			IdleViewRotation = FRotator::ZeroRotator;
		}
	}

	else
	{
		if (SpringArmComponent)
		{
			IdleViewRotation = SpringArmComponent->GetTargetRotation();
		}
		else if (CameraComponent)
		{
			IdleViewRotation = CameraComponent -> GetRelativeRotation();
		}
		else
		{
			IdleViewRotation = FRotator::ZeroRotator;
		}
		
	}
}

FRotator UTransparentCameraComponent::GetCurrentViewRotation() const
{
	if (bUseControllerRotation)
	{
		if (OwnerCharacter && OwnerCharacter -> GetController())
		{
			return OwnerCharacter->GetController()->GetControlRotation();
		}
	}
	else
	{
		if (SpringArmComponent)
		{
			return SpringArmComponent->GetTargetRotation();
		}
		if (CameraComponent)
		{
			return CameraComponent->GetRelativeRotation();
		}
	}
	return FRotator::ZeroRotator;
}

void UTransparentCameraComponent::ApplyViewRotation(const FRotator& NewRot)
{
	if (bUseControllerRotation)
	{
		if (APlayerController* PC = OwnerCharacter ? Cast<APlayerController>(OwnerCharacter-> GetController()) : nullptr)
		{
			//피치만 복귀하는 옵션
			if (bResetPitchOnly)
			{
				FRotator Cur = PC->GetControlRotation();
				Cur.Pitch = NewRot.Pitch;
				PC->SetControlRotation(Cur);
			}
			else
			{
				PC->SetControlRotation(NewRot);
			}
		}
	}

	else
	{
		if (SpringArmComponent)
		{
			FRotator Cur = SpringArmComponent->GetTargetRotation();
			if (bResetPitchOnly) Cur.Pitch = NewRot.Pitch;
			else                 Cur       = NewRot;
			SpringArmComponent->SetWorldRotation(Cur);
		}
		else if (CameraComponent)
		{
			FRotator Cur = CameraComponent->GetRelativeRotation();
			if (bResetPitchOnly) Cur.Pitch = NewRot.Pitch;
			else                 Cur       = NewRot;
			CameraComponent->SetRelativeRotation(Cur);
		}
	}
}



void UTransparentCameraComponent::UpdateAutoResetView(float DeltaTime)
{
	if (!GetWorld()) return;

	const float Now = GetWorld()->GetTimeSeconds();
	const FRotator Curr = GetCurrentViewRotation();

	// 1) 유저 입력 감지(최근 프레임 대비 변화)
	const bool bUserMoved = DetectUserViewMove(Curr, PrevViewRotation);

	// 자동 복귀 중인데 유저가 강하게 움직이면 복귀 중단
	if (bAutoReturning && bUserMoved)
	{
		const float dPitch = FMath::Abs(FRotator::NormalizeAxis(Curr.Pitch - PrevViewRotation.Pitch));
		const float dYaw   = FMath::Abs(FRotator::NormalizeAxis(Curr.Yaw   - PrevViewRotation.Yaw));
		const float Delta  = bResetPitchOnly ? dPitch : FMath::Max(dPitch, dYaw);

		if (Delta >= InterruptThresholdDeg)
		{
			bAutoReturning = false;
			LastUserViewMoveTime = Now;
		}
	}

	if (bUserMoved && !bAutoReturning)
	{
		LastUserViewMoveTime = Now;
	}

	PrevViewRotation = Curr;

	// 2) 복귀를 시작/진행할지 판단
	const float DevPitch = FMath::Abs(FRotator::NormalizeAxis(Curr.Pitch - IdleViewRotation.Pitch));
	const float DevYaw   = FMath::Abs(FRotator::NormalizeAxis(Curr.Yaw   - IdleViewRotation.Yaw));
	const float Dev      = bResetPitchOnly ? DevPitch : FMath::Max(DevPitch, DevYaw);

	// 무입력 시간이 ResetDelaySeconds 이상이고, Idle과 차이가 임계 이상이면 복귀
	const bool bShouldReturn = ((Now - LastUserViewMoveTime) >= ResetDelaySeconds) && (Dev >= DeviationThresholdDeg);

	if (bShouldReturn || bAutoReturning)
	{
		bAutoReturning = true;

		FRotator Target = Curr;
		if (bResetPitchOnly) Target.Pitch = IdleViewRotation.Pitch;
		else                 Target       = IdleViewRotation;

		FRotator NewRot;
		NewRot.Pitch = FMath::FInterpTo(Curr.Pitch, Target.Pitch, DeltaTime, ResetInterpSpeed);
		NewRot.Yaw   = bResetPitchOnly ? Curr.Yaw
		                               : FMath::FixedTurn(Curr.Yaw, Target.Yaw, ResetInterpSpeed * DeltaTime * 180.f); // Yaw는 랩어라운드 보호
		NewRot.Roll  = bResetPitchOnly ? Curr.Roll : IdleViewRotation.Roll;

		ApplyViewRotation(NewRot);

		// 목표치 근접 시 종료
		const float RemPitch = FMath::Abs(FRotator::NormalizeAxis(NewRot.Pitch - (bResetPitchOnly ? IdleViewRotation.Pitch : IdleViewRotation.Pitch)));
		const float RemYaw   = bResetPitchOnly ? 0.f : FMath::Abs(FRotator::NormalizeAxis(NewRot.Yaw - IdleViewRotation.Yaw));
		if (RemPitch < 0.1f && RemYaw < 0.25f)
		{
			bAutoReturning = false;
			ApplyViewRotation(bResetPitchOnly ? FRotator(NewRot.Pitch, Curr.Yaw, Curr.Roll) : IdleViewRotation);
		}
	}
}





bool UTransparentCameraComponent::DetectUserViewMove(const FRotator& Curr, const FRotator& Prev) const
{
	// 유저가 화면을 위/아래로 옮겼는지 간단 감지(프레임 당 각도 변화)
	const float dPitch = FMath::Abs(FRotator::NormalizeAxis(Curr.Pitch - Prev.Pitch));
	const float dYaw   = FMath::Abs(FRotator::NormalizeAxis(Curr.Yaw   - Prev.Yaw));
	const float Delta  = bResetPitchOnly ? dPitch : FMath::Max(dPitch, dYaw);
	return (Delta >= MovementDetectThresholdDeg);
}
