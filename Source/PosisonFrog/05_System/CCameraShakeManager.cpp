// CCameraShakeManager.cpp
#include "CCameraShakeManager.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/CameraShake.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "99_Util/CLog.h"

UCCameraShakeManager* UCCameraShakeManager::Instance = nullptr;

UCCameraShakeManager* UCCameraShakeManager::GetInstance(UWorld* World)
{
    if (!Instance && World)
    {
        Instance = NewObject<UCCameraShakeManager>();
        Instance->AddToRoot(); // GC 방지
    }
    return Instance;
}

void UCCameraShakeManager::PlayPlayerAttackShake(
    UWorld* World,
    TSubclassOf<UCameraShakeBase> ShakeClass,
    FVector Location,
    float Scale)
{
    if (!World || !ShakeClass)
        return;

    if (!bPlayerShakesEnabled)
        return;

    if (!CanPlayPlayerShake(ShakeClass))
        return;

    // 플레이어 컨트롤러에서 직접 재생 (위치 무관)
    if (APlayerController* PC = World->GetFirstPlayerController())
    {
        if (PC->PlayerCameraManager)
        {
            PC->PlayerCameraManager->StartCameraShake(ShakeClass, Scale);
            UpdatePlayerShakeTime(ShakeClass, World->GetTimeSeconds());
        }
    }
}

void UCCameraShakeManager::PlayEnemyAttackShake(
    UWorld* World,
    TSubclassOf<UMatineeCameraShake> ShakeClass,
    FVector Location,
    float InnerRadius,
    float OuterRadius,
    float Scale)
{
    if (!World || !ShakeClass)
        return;

    if (!bEnemyShakesEnabled)
        return;

    if (!CanPlayEnemyShake(ShakeClass))
        return;

    // 위치 기반 감쇠로 재생
    UGameplayStatics::PlayWorldCameraShake(
        World,
        ShakeClass,
        Location,
        InnerRadius,
        OuterRadius,
        1.0f,  // Falloff
        true   // Orient towards epicenter
    );

    UpdateEnemyShakeTime(ShakeClass, World->GetTimeSeconds());
}

void UCCameraShakeManager::StopAllShakes(UWorld* World)
{
    if (!World)
        return;

    if (APlayerController* PC = World->GetFirstPlayerController())
    {
        if (PC->PlayerCameraManager)
        {
            PC->PlayerCameraManager->StopAllCameraShakes(true);
        }
    }
}

void UCCameraShakeManager::StopPlayerShakes(UWorld* World)
{
    // 현재 언리얼 API로는 특정 셰이크만 멈추기 어려움
    // 필요시 커스텀 구현
    if (!World)
        return;

    PlayerShakeLastTimes.Empty();
}

void UCCameraShakeManager::StopEnemyShakes(UWorld* World)
{
    if (!World)
        return;

    EnemyShakeLastTimes.Empty();
}

bool UCCameraShakeManager::CanPlayPlayerShake(TSubclassOf<UCameraShakeBase> ShakeClass)
{
    if (!ShakeClass)
        return false;

    float* LastTime = PlayerShakeLastTimes.Find(ShakeClass);
    if (!LastTime)
        return true;

    // 쿨다운 체크는 매니저 레벨에서만 (World는 여기서 접근 불가)
    // 호출하는 쪽에서 World를 전달받아야 함
    return true;
}

bool UCCameraShakeManager::CanPlayEnemyShake(TSubclassOf<UMatineeCameraShake> ShakeClass)
{
    if (!ShakeClass)
        return false;

    float* LastTime = EnemyShakeLastTimes.Find(ShakeClass);
    if (!LastTime)
        return true;

    return true;
}

void UCCameraShakeManager::UpdatePlayerShakeTime(TSubclassOf<UCameraShakeBase> ShakeClass, float CurrentTime)
{
    if (!ShakeClass)
        return;

    PlayerShakeLastTimes.Add(ShakeClass, CurrentTime);
}

void UCCameraShakeManager::UpdateEnemyShakeTime(TSubclassOf<UMatineeCameraShake> ShakeClass, float CurrentTime)
{
    if (!ShakeClass)
        return;

    EnemyShakeLastTimes.Add(ShakeClass, CurrentTime);
}

void UCCameraShakeManager::LogDebugInfo() const
{
    CLog::Log(FString::Printf(TEXT("[CameraShakeManager] Player Shakes Enabled: %s"), 
        bPlayerShakesEnabled ? TEXT("Yes") : TEXT("No")));
    CLog::Log(FString::Printf(TEXT("[CameraShakeManager] Enemy Shakes Enabled: %s"), 
        bEnemyShakesEnabled ? TEXT("Yes") : TEXT("No")));
    CLog::Log(FString::Printf(TEXT("[CameraShakeManager] Player Shake Count: %d"), 
        PlayerShakeLastTimes.Num()));
    CLog::Log(FString::Printf(TEXT("[CameraShakeManager] Enemy Shake Count: %d"), 
        EnemyShakeLastTimes.Num()));
}