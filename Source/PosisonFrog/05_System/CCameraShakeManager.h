// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CCameraShakeManager.generated.h"

class UMatineeCameraShake;
class UCameraShakeBase;
class UWorld;

/**
 * 카메라 셰이크 매니저
 * - 플레이어/적 카메라 셰이크를 분리 관리
 * - 쿨다운으로 과도한 셰이크 방지
 * - 우선순위 시스템으로 중요한 셰이크 보존
 */
UCLASS()
class POSISONFROG_API UCCameraShakeManager : public UObject
{
    GENERATED_BODY()

public:
    /** 싱글톤 인스턴스 가져오기 */
    static UCCameraShakeManager* GetInstance(UWorld* World);

    /** 플레이어 공격 카메라 셰이크 재생 */
    void PlayPlayerAttackShake(
        UWorld* World,
        TSubclassOf<UCameraShakeBase> ShakeClass,
        FVector Location,
        float Scale = 1.0f
    );

    /** 적 공격 카메라 셰이크 재생 */
    void PlayEnemyAttackShake(
        UWorld* World,
        TSubclassOf<UMatineeCameraShake> ShakeClass,
        FVector Location,
        float InnerRadius = 0.0f,
        float OuterRadius = 1000.0f,
        float Scale = 1.0f
    );

    /** 강제로 모든 셰이크 중지 */
    void StopAllShakes(UWorld* World);

    /** 플레이어 셰이크만 중지 */
    void StopPlayerShakes(UWorld* World);

    /** 적 셰이크만 중지 */
    void StopEnemyShakes(UWorld* World);

    /** 쿨다운 설정 */
    void SetPlayerShakeCooldown(float Cooldown) { PlayerShakeCooldown = Cooldown; }
    void SetEnemyShakeCooldown(float Cooldown) { EnemyShakeCooldown = Cooldown; }

    /** 셰이크 활성화/비활성화 */
    void SetPlayerShakesEnabled(bool bEnabled) { bPlayerShakesEnabled = bEnabled; }
    void SetEnemyShakesEnabled(bool bEnabled) { bEnemyShakesEnabled = bEnabled; }

    /** 디버그 정보 출력 */
    void LogDebugInfo() const;

private:
    /** 쿨다운 체크 */
    bool CanPlayPlayerShake(TSubclassOf<UCameraShakeBase> ShakeClass);
    bool CanPlayEnemyShake(TSubclassOf<UMatineeCameraShake> ShakeClass);

    /** 마지막 재생 시간 업데이트 */
    void UpdatePlayerShakeTime(TSubclassOf<UCameraShakeBase> ShakeClass, float CurrentTime);
    void UpdateEnemyShakeTime(TSubclassOf<UMatineeCameraShake> ShakeClass, float CurrentTime);

private:
    /** 플레이어 셰이크 쿨다운 (같은 셰이크 반복 방지) */
    UPROPERTY()
    TMap<TSubclassOf<UCameraShakeBase>, float> PlayerShakeLastTimes;

    /** 적 셰이크 쿨다운 */
    UPROPERTY()
    TMap<TSubclassOf<UMatineeCameraShake>, float> EnemyShakeLastTimes;

    /** 쿨다운 설정 */
    float PlayerShakeCooldown = 0.05f;  // 플레이어 공격은 짧은 쿨다운 (빠른 공격 대응)
    float EnemyShakeCooldown = 0.1f;    // 적 공격은 조금 긴 쿨다운 (여러 적 동시 공격)

    /** 활성화 플래그 */
    bool bPlayerShakesEnabled = true;
    bool bEnemyShakesEnabled = true;

    /** 싱글톤 인스턴스 */
    static UCCameraShakeManager* Instance;
};