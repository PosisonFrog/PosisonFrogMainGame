#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "TransparentCameraComponent.generated.h"

class UCameraComponent;
class USpringArmComponent;  

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UTransparentCameraComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UTransparentCameraComponent();

	/** 외부에서 카메라/스프링암 지정(선택) */
	UFUNCTION(BlueprintCallable, Category="Camera|Transparent")
	void SetCameraComponent(UCameraComponent* InCameraComponent);

	UFUNCTION(BlueprintCallable, Category="Camera|Transparent")
	void SetSpringArmComponent(USpringArmComponent* InSpringArm);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// ── Obstruction 처리 ──
	void UpdateObstructingActors();

	// ── Auto Reset View ──
	void UpdateAutoResetView(float DeltaTime);
	void CalibrateIdleView();                  // 현재 시점을 Idle로 등록
	FRotator GetCurrentViewRotation() const;   // 컨트롤러/스프링암/카메라 중 선택
	void     ApplyViewRotation(const FRotator& NewRot); // 같은 기준으로 적용
	bool     DetectUserViewMove(const FRotator& Curr, const FRotator& Prev) const;

private:
	// === References ===
	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter = nullptr;
	
	UPROPERTY(VisibleInstanceOnly, Category="Camera|Transparent")
	TObjectPtr<UCameraComponent>  CameraComponent = nullptr;

	UPROPERTY(VisibleInstanceOnly, Category="Camera|Transparent")
	TObjectPtr<USpringArmComponent> SpringArmComponent = nullptr;

	// === Trace ===
	UPROPERTY(EditAnywhere, Category="Camera|Transparent|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, Category="Camera|Transparent|Trace")
	bool bDebugTrace = false;

	TSet<TObjectPtr<UPrimitiveComponent>> ObstructingComponents_LastFrame;

	// === Auto Reset View ===
	/** 자동 복귀 사용 여부 */
	UPROPERTY(EditAnywhere, Category="Camera|AutoReset")
	bool bEnableAutoReset = true;
	
	/** 셰이더에 전달할 커스텀 스텐실 값 */
	UPROPERTY(EditAnywhere, Category="Camera|Transparent|Trace", meta=(ClampMin="0", ClampMax="255"))
	int32 StencilValue = 200;
	
	/** 무입력 후 복귀 대기 시간(초) */
	UPROPERTY(EditAnywhere, Category="Camera|AutoReset", meta=(ClampMin="0.0"))
	float ResetDelaySeconds = 5.0f;

	/** 복귀 보간 속도(값이 클수록 빠르게) */
	UPROPERTY(EditAnywhere, Category="Camera|AutoReset", meta=(ClampMin="0.1"))
	float ResetInterpSpeed = 3.0f;

	/** Pitch만 복귀(수직 시점)할지 여부. false면 Yaw까지 복귀 */
	UPROPERTY(EditAnywhere, Category="Camera|AutoReset")
	bool bResetPitchOnly = true;

	/** 어떤 회전을 기준으로 볼지 선택: true=Controller, false=Camera/Arm */
	UPROPERTY(EditAnywhere, Category="Camera|AutoReset")
	bool bUseControllerRotation = true;

	/** 유저가 움직였다고 감지할 프레임 당 최소 각도 변화(도) */
	UPROPERTY(EditAnywhere, Category="Camera|AutoReset", meta=(ClampMin="0.01"))
	float MovementDetectThresholdDeg = 0.15f;

	/** Idle과 현재 시점 차이가 이 값(도) 이상일 때만 복귀 수행 */
	UPROPERTY(EditAnywhere, Category="Camera|AutoReset", meta=(ClampMin="0.0"))
	float DeviationThresholdDeg = 0.75f;

	/** 자동 복귀 도중 유저 개입으로 간주할 추가 임계치(도/프레임) */
	UPROPERTY(EditAnywhere, Category="Camera|AutoReset", meta=(ClampMin="0.01"))
	float InterruptThresholdDeg = 0.35f;

	FRotator IdleViewRotation;      // 기준 시점
	FRotator PrevViewRotation;      // 전 프레임 시점
	float    LastUserViewMoveTime = -1000.f;
	bool     bAutoReturning = false;
};