// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Camera/CameraShakeBase.h"
#include "MatineeCameraShake.h"
#include "CPlayerAttack1Shake.generated.h"

/**
 * 
 */
UCLASS()
class POSISONFROG_API UCPlayerAttack1Shake : public UMatineeCameraShake
{
    GENERATED_BODY()

public:
    UCPlayerAttack1Shake()
    {
        OscillationDuration = 0.15f;
        OscillationBlendInTime = 0.02f;
        OscillationBlendOutTime = 0.05f;

        // 회전 - 가볍게
        RotOscillation.Pitch.Amplitude = 0.3f;
        RotOscillation.Pitch.Frequency = 20.0f;
        
        RotOscillation.Yaw.Amplitude = 0.3f;
        RotOscillation.Yaw.Frequency = 20.0f;

        // 위치 - Z축만 약간
        LocOscillation.Z.Amplitude = 1.0f;
        LocOscillation.Z.Frequency = 20.0f;

        // FOV 변화 없음
        FOVOscillation.Amplitude = 0.0f;
    }
};

/**
 * 플레이어 2타 공격 - 중간 셰이크
 * 무게감 있는 타격
 */
UCLASS()
class POSISONFROG_API UCPlayerAttack2Shake : public UMatineeCameraShake
{
    GENERATED_BODY()

public:
    UCPlayerAttack2Shake()
    {
        OscillationDuration = 0.25f;
        OscillationBlendInTime = 0.03f;
        OscillationBlendOutTime = 0.08f;

        // 회전 - 중간
        RotOscillation.Pitch.Amplitude = 0.6f;
        RotOscillation.Pitch.Frequency = 15.0f;
        
        RotOscillation.Yaw.Amplitude = 0.6f;
        RotOscillation.Yaw.Frequency = 15.0f;

        RotOscillation.Roll.Amplitude = 0.4f;
        RotOscillation.Roll.Frequency = 15.0f;

        // 위치 - 전체 축
        LocOscillation.X.Amplitude = 2.0f;
        LocOscillation.X.Frequency = 15.0f;
        
        LocOscillation.Y.Amplitude = 2.0f;
        LocOscillation.Y.Frequency = 15.0f;
        
        LocOscillation.Z.Amplitude = 2.0f;
        LocOscillation.Z.Frequency = 15.0f;

        // FOV - 약간
        FOVOscillation.Amplitude = 0.5f;
        FOVOscillation.Frequency = 15.0f;
    }
};

/**
 * 플레이어 3타 공격 - 강한 셰이크
 * 피니셔 느낌의 강력한 충격
 */
UCLASS()
class POSISONFROG_API UCPlayerAttack3Shake : public UMatineeCameraShake
{
    GENERATED_BODY()

public:
    UCPlayerAttack3Shake()
    {
        OscillationDuration = 0.4f;
        OscillationBlendInTime = 0.05f;
        OscillationBlendOutTime = 0.15f;

        // 회전 - 강력
        RotOscillation.Pitch.Amplitude = 1.2f;
        RotOscillation.Pitch.Frequency = 12.0f;
        
        RotOscillation.Yaw.Amplitude = 1.2f;
        RotOscillation.Yaw.Frequency = 12.0f;

        RotOscillation.Roll.Amplitude = 1.0f;
        RotOscillation.Roll.Frequency = 12.0f;

        // 위치 - 전체 축 강력
        LocOscillation.X.Amplitude = 4.0f;
        LocOscillation.X.Frequency = 12.0f;
        
        LocOscillation.Y.Amplitude = 4.0f;
        LocOscillation.Y.Frequency = 12.0f;
        
        LocOscillation.Z.Amplitude = 4.0f;
        LocOscillation.Z.Frequency = 12.0f;

        // FOV - 충격 강조
        FOVOscillation.Amplitude = 1.5f;
        FOVOscillation.Frequency = 12.0f;
    }
};

/**
 * 플레이어 대시 시작 - 미세한 셰이크
 * 빠른 움직임 시작을 강조
 */
UCLASS()
class POSISONFROG_API UCPlayerDashShake : public UMatineeCameraShake
{
    GENERATED_BODY()

public:
    UCPlayerDashShake()
    {
        OscillationDuration = 0.1f;
        OscillationBlendInTime = 0.01f;
        OscillationBlendOutTime = 0.03f;

        // 회전 - 매우 약함
        RotOscillation.Pitch.Amplitude = 0.2f;
        RotOscillation.Pitch.Frequency = 25.0f;

        // FOV - 빠른 움직임 강조
        FOVOscillation.Amplitude = 1.0f;
        FOVOscillation.Frequency = 25.0f;
    }
};

/**
 * 플레이어 피격 - 강한 충격
 * 맞았을 때의 충격감
 */
UCLASS()
class POSISONFROG_API UCPlayerHitShake : public UMatineeCameraShake
{
    GENERATED_BODY()

public:
    UCPlayerHitShake()
    {
        OscillationDuration = 0.3f;
        OscillationBlendInTime = 0.02f;
        OscillationBlendOutTime = 0.1f;

        // 회전 - 충격감
        RotOscillation.Pitch.Amplitude = 1.0f;
        RotOscillation.Pitch.Frequency = 18.0f;
        
        RotOscillation.Yaw.Amplitude = 1.0f;
        RotOscillation.Yaw.Frequency = 18.0f;

        RotOscillation.Roll.Amplitude = 0.8f;
        RotOscillation.Roll.Frequency = 18.0f;

        // 위치 - 뒤로 밀림
        LocOscillation.X.Amplitude = 3.0f;
        LocOscillation.X.Frequency = 18.0f;
        
        LocOscillation.Y.Amplitude = 2.0f;
        LocOscillation.Y.Frequency = 18.0f;
        
        LocOscillation.Z.Amplitude = 2.0f;
        LocOscillation.Z.Frequency = 18.0f;

        // FOV - 순간 축소
        FOVOscillation.Amplitude = -2.0f;  // 음수로 잠깐 줌인
        FOVOscillation.Frequency = 18.0f;
    }
};

/**
 * 플레이어 착지 - 중간 충격
 * 높은 곳에서 착지할 때
 */
UCLASS()
class POSISONFROG_API UCPlayerLandShake : public UMatineeCameraShake
{
    GENERATED_BODY()

public:
    UCPlayerLandShake()
    {
        OscillationDuration = 0.2f;
        OscillationBlendInTime = 0.02f;
        OscillationBlendOutTime = 0.08f;

        // 회전 - 아래로 꺾임
        RotOscillation.Pitch.Amplitude = 0.5f;
        RotOscillation.Pitch.Frequency = 20.0f;

        // 위치 - Z축 충격
        LocOscillation.Z.Amplitude = 3.0f;
        LocOscillation.Z.Frequency = 20.0f;
    }
};
