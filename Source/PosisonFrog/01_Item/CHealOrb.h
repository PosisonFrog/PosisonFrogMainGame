// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CHealOrb.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UCurveFloat;
class UNiagaraSystem;
class USoundBase;

UCLASS()
class POSISONFROG_API ACHealOrb : public AActor
{
	GENERATED_BODY()
	
public:	
	ACHealOrb();

protected:
	// 액터가 생성될 때 호출
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	
private:
	// ==== Target ====
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<AActor> TargetPlayer = nullptr;
	
	// ==== Components ====
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> DetectSphere;

	UPROPERTY(EditAnywhere, Category="Chase", meta=(ClampMin="0", ForceUnits="cm"))
	float SphereRadius = 60.0f;
	
	UPROPERTY(EditAnywhere, Category="Chase", meta=(ClampMin="0", ForceUnits="cm"))
	float DetectRadius = 800.0f;

	// ! 전용 채널 : 에디터에서 만든 PlayerBody 채널 할당 !
	UPROPERTY(EditDefaultsOnly, Category="Chase|Collision")
	TEnumAsByte<ECollisionChannel> PlayerBodyChannel = ECollisionChannel::ECC_GameTraceChannel1;

	// ==== RunTime State ====
	UPROPERTY(VisibleInstanceOnly, Category="State")
	FVector Velocity = FVector::ZeroVector;
	
	// ==== Steering (자연스러운 추적) ====
	UPROPERTY(EditAnywhere, Category="Chase|Steering", meta=(ClampMin="0"))
	float MaxSpeed = 800.0f;

	UPROPERTY(EditAnywhere, Category="Chase|Steering", meta=(ClampMin="0"))
	float Accel = 3000.0f;

	// 근접 시 감속 반경
	UPROPERTY(EditAnywhere, Category="Chase|Steering", meta=(ClampMin="0"))
	float ArriveRadius = 120.0f;

	// 방향 전환 보정(0~1): 높을수록 빠르게 보정
	UPROPERTY(EditAnywhere, Category="Chase|Steering", meta=(ClampMin="0", ClampMax="1"))
	float TurnAssist = 0.25f;

	// **속도 보간 커브 (선택)** - X: Player까지의 거리(cm), Y: 속도 스케일
	// 예) 멀리선 1.2, 가까이선 0.2 등으로 설정
	UPROPERTY(EditAnywhere, Category="Chase|Steering")
	TObjectPtr<UCurveFloat> SpeedByDistanceCurve = nullptr;

	// ==== Obstacle Avoidance (간단 벽 회피) ====
	UPROPERTY(EditAnywhere, Category="Chase|Avoid", meta=(ClampMin="0"))
	float ProbeLength = 300.0f;

	UPROPERTY(EditAnywhere, Category="Chase|Avoid", meta=(ClampMin="0"))
	float SideProbeOffset = 110.0f;

	// ==== NavMesh Path Follow (시야 끊길 때만) ====
	UPROPERTY(EditAnywhere, Category="Chase|Nav")
	bool bUseNavMesh = true;

	UPROPERTY(EditAnywhere, Category="Chase|Nav", meta=(ClampMin="0.05"))
	float RepathInterval = 0.30f;

	UPROPERTY(EditAnywhere, Category="Chase|Nav", meta=(ClampMin="10"))
	float WaypointReachRadius = 80.0f;

	// ==== Target Reacquire Policy (재획득 정책) ====
	// DetectSphere를 벗어난 뒤 계속 추적을 허용하는 유예 시간
	UPROPERTY(EditAnywhere, Category="Chase|Target", meta=(ClampMin="0"))
	float LoseTargetGraceTime = 2.0f;

	// DetectSphere 밖 + LOS도 끊겼을 때, 기존 Nav 경로를 붙잡고 쫓아가는 최대 유지 시간
	UPROPERTY(EditAnywhere, Category="Chase|Target", meta=(ClampMin="0"))
	float PathHoldTime = 3.0f;

	// ==== Timers / Accumulators
	float TimeSinceRepath = 0.0f;
	bool  bTargetInDetect = false;      // DetectSphere 안에 있는가
	float TimeSinceDetectLost = 0.0f;   // DetectSphere 밖에 있었던 누적 시간
	float TimeSinceNoLOS = 0.0f;        // LOS가 없었던 누적 시간

	// ==== Path ====
	TArray<FVector> PathPoints;
	int32  PathIndex = 0;
	
	// ==== Heal ====
	UPROPERTY(EditAnywhere, Category = "Heal")
	float HealAmount = 30.0f;

	// ==== VFX / SFX ====
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

	// ===== Debug =====
	UPROPERTY(EditAnywhere, Category="Debug")
	bool bDebugDraw = false;
	
private:
	// ==== Overlaps ====
	UFUNCTION()
	void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
						 UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
						 bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnDetectBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
							  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
							  bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnDetectEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
							  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// ==== Steering / Avoidance ====
	bool HasLineOfSightToTarget() const;
	void FollowSteering(float DeltaTime);
	float GetDistToTarget2D() const; // Helper
	FVector ComputeDesiredDir() const;
	FVector AvoidObstacles(const FVector& DesiredDir) const;

	// ==== NavMesh Path ====
	void RebuildPath();
	void FollowPath(float DeltaTime);
};
