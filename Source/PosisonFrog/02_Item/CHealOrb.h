// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CHealOrb.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UNiagaraSystem;
class USoundBase;

UCLASS()
class POSISONFROG_API ACHealOrb : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACHealOrb();

private:
	// 추적 대상 액터(플레이어)
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<AActor> TargetActor = nullptr;

	// === 구슬 궤도 및 스피드 관련 변수 ===
	// - Components -
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> DetectSphere;

	// - Movement/Detection -
	UPROPERTY(EditAnywhere, Category="Chase", meta=(ClampMin="0", ForceUnits="cm"))
	float SphereRadius = 60.0f;
	
	UPROPERTY(EditAnywhere, Category="Chase", meta=(ClampMin="0", ForceUnits="cm"))
	float DetectRadius = 800.0f;

	// 현재 속도 벡터
	UPROPERTY(VisibleInstanceOnly, Category="State")
	FVector Velocity = FVector::ZeroVector;

	// 곡선 궤적용 내부 위상
	UPROPERTY(VisibleInstanceOnly, Category="State")
	float CurvePhase = 0.f;
	
	// 기본 목표 속도
	UPROPERTY(EditAnywhere, Category="Chase", meta=(ClampMin="0", ForceUnits="cm/s"))
	float Speed = 500.f;

	// 거리가 가까울수록 속도 부스트
	UPROPERTY(EditAnywhere, Category="Chase|Homing", meta=(ClampMin="0", ForceUnits="cm/s"))
	float HomingBoost = 400.0f;

	// 부스트가 최대치가 되는 반경 (안으로 들어오면 선형으로 부스트 증가)
	UPROPERTY(EditAnywhere, Category="Chase|Homing", meta=(ClampMin="0", ForceUnits="cm"))
	float HomingBoostRadius = 600.f;

	// 조향 민감도(클수록 날카롭게 방향 전환)
	UPROPERTY(EditAnywhere, Category="Chase|Homing", meta=(ClampMin="0"))
	float HomingTurnRate = 6.0f;

	// 오버슈트 방지(스텝을 현재 거리로 클램프)
	UPROPERTY(EditAnywhere, Category="Chase")
	bool bClampStepToDistance = true;

	// 벽 차단/슬라이드
	UPROPERTY(EditAnywhere, Category="Chase|Collision")
	bool bSlideOnBlock = false;

	UPROPERTY(EditAnywhere, Category="Chase|Collision", meta=(ClampMin="0", ClampMax="1"))
	float SlideFactor = 0.6f;
	
	// ! 전용 채널 : 에디터에서 만든 PlayerBody 채널 할당 !
	UPROPERTY(EditDefaultsOnly, Category="Chase|Collision")
	TEnumAsByte<ECollisionChannel> PlayerBodyChannel = ECollisionChannel::ECC_GameTraceChannel1;

	// ─ Curve Trajectory ─
	// 곡선(지그재그) 궤적 활성화
	UPROPERTY(EditAnywhere, Category="Chase|Curve")
	bool bUseSineCurve = false;

	// 횡방향 진폭 (Dir의 법선 방향), 주파수(Hz)
	UPROPERTY(EditAnywhere, Category="Chase|Curve", meta=(ClampMin="0", ForceUnits="cm"))
	float CurveAmplitude = 60.f;

	UPROPERTY(EditAnywhere, Category="Chase|Curve", meta=(ClampMin="0"))
	float CurveFrequency = 2.f;

	// 목표에 가까울수록 곡선 효과를 줄이기
	UPROPERTY(EditAnywhere, Category="Chase|Curve")
	bool bCurveDampWithDistance = true;
	
	// === Heal ===
	// 힐값 확인하기 위해서 큰 값으로 지정
	UPROPERTY(EditAnywhere, Category = "Heal")
	float HealAmount = 30.0f;

	// === VFX / SFX ===
	// 현재 이펙트 및 사운드가 없어서 임시로 nullptr 나중에 적용할 때 BP에서 변경 OR 코드에서 변경
	UPROPERTY(EditDefaultsOnly, Category="FX")
	TObjectPtr<UNiagaraSystem> VFX_Spawn = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="FX")
	TObjectPtr<UNiagaraSystem> VFX_Pickup = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="FX")
	TObjectPtr<UNiagaraSystem> VFX_Bounce = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="SFX")
	TObjectPtr<USoundBase> SFX_Spawn = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="SFX")
	TObjectPtr<USoundBase> SFX_Pickup = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="SFX")
	TObjectPtr<USoundBase> SFX_Bounce = nullptr;
	
protected:
	// 액터가 생성될 때 호출
	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;
	
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION()
	void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
						 UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
						 bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnDetectOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
						 UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
						 bool bFromSweep, const FHitResult& SweepResult);
};
