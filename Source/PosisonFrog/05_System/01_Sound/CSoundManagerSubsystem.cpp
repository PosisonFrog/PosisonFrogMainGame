#include "CSoundManagerSubsystem.h"
#include "CSoundDataAsset.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

void UCSoundManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // AudioComponent는 PlayBGM에서 SpawnSound2D로 생성

    bIsBGMPaused = false;

    UE_LOG(LogTemp, Log, TEXT("[SoundManager] Initialized"));
    
}

void UCSoundManagerSubsystem::Deinitialize()
{
    StopBGM(0.0f);

    // AudioComponent 정리
    BGMAudioComponent.Reset();

    Super::Deinitialize();

    UE_LOG(LogTemp, Log, TEXT("[SoundManager] Deinitialized"));
}

// ===============================================
// BGM
// ===============================================

void UCSoundManagerSubsystem::PlayBGM(USoundBase* Sound, float FadeInDuration, float VolumeMultiplier)
{
    if (!Sound)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SoundManager] PlayBGM failed - Invalid sound"));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("[SoundManager] PlayBGM failed - No World"));
        return;
    }
    
    
    
    if (BGMAudioComponent.IsValid())
    {
        StopBGM(FadeInDuration * 0.5f);
        const bool bIsSameBGM = CurrentBGM.IsValid() && CurrentBGM.Get() == Sound;
            
        if (bIsSameBGM)
        {
            // 동일한 BGM을 다시 요청한 경우, 재생을 유지하거나 일시정지 상태라면 재개만 한다.
            if (bIsBGMPaused)
            {
                BGMAudioComponent->SetPaused(false);
                bIsBGMPaused = false;
                UE_LOG(LogTemp, Log, TEXT("[SoundManager] Resumed existing BGM: %s"), *Sound->GetName());
            }
            else if (!BGMAudioComponent->IsPlaying())
            {
                BGMAudioComponent->Play();
                UE_LOG(LogTemp, Log, TEXT("[SoundManager] Restarted existing BGM playback: %s"), *Sound->GetName());
            }
                    
            // 동일한 사운드면 페이드인/리스타트 없이 그대로 유지
            return;
        }
            
        if (BGMAudioComponent->IsPlaying())
        {
            StopBGM(FadeInDuration * 0.5f);
        }
    }
    
    BGMAudioComponent = UGameplayStatics::SpawnSound2D(
        World,
        Sound,
        VolumeMultiplier,
        1.0f,           // Pitch
        0.0f,           // StartTime
        nullptr,        // ConcurrencySettings
        true,           // bPersistAcrossLevelTransition
        false           // bAutoDestroy (false로 해서 우리가 관리)
    );

    if (BGMAudioComponent.IsValid())
    {
        CurrentBGM = Sound;
        bIsBGMPaused = false;
        
        if (FadeInDuration > 0.0f)
        {
            BGMAudioComponent->FadeIn(FadeInDuration, VolumeMultiplier);
        }

        UE_LOG(LogTemp, Log, TEXT("[SoundManager] Playing BGM: %s, IsPlaying: %d"), 
               *Sound->GetName(), BGMAudioComponent->IsPlaying());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[SoundManager] Failed to spawn BGM AudioComponent!"));
    }
}

void UCSoundManagerSubsystem::StopBGM(float FadeOutDuration)
{
    if (!BGMAudioComponent.IsValid() || !BGMAudioComponent->IsPlaying())
    {
        return;
    }

    if (FadeOutDuration > 0.0f)
    {
        BGMAudioComponent->FadeOut(FadeOutDuration, 0.0f);
    }
    else
    {
        BGMAudioComponent->Stop();
    }

    CurrentBGM.Reset();
    bIsBGMPaused = false;

    UE_LOG(LogTemp, Log, TEXT("[SoundManager] Stopped BGM"));
}

void UCSoundManagerSubsystem::PauseBGM()
{
    if (BGMAudioComponent.IsValid() && BGMAudioComponent->IsPlaying())
    {
        BGMAudioComponent->SetPaused(true);
        bIsBGMPaused = true;
        UE_LOG(LogTemp, Log, TEXT("[SoundManager] Paused BGM"));
    }
}

void UCSoundManagerSubsystem::ResumeBGM()
{
    if (BGMAudioComponent.IsValid() && bIsBGMPaused)
    {
        BGMAudioComponent->SetPaused(false);
        bIsBGMPaused = false;
        UE_LOG(LogTemp, Log, TEXT("[SoundManager] Resumed BGM"));
    }
}

bool UCSoundManagerSubsystem::IsBGMPlaying() const
{
    return BGMAudioComponent.IsValid() && BGMAudioComponent->IsPlaying();
}

// ===============================================
// SFX
// ===============================================

void UCSoundManagerSubsystem::PlaySFX2D(USoundBase* Sound, float VolumeMultiplier, float PitchMultiplier)
{
    if (!Sound)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // 세팅 UI의 SoundMix가 자동으로 적용됨
    UGameplayStatics::PlaySound2D(
        World,
        Sound,
        VolumeMultiplier,
        PitchMultiplier,
        0.0f,
        nullptr,
        nullptr,
        false
    );
}

void UCSoundManagerSubsystem::PlaySFX3D(USoundBase* Sound, const FVector& Location, 
                                         float VolumeMultiplier, float PitchMultiplier)
{
    if (!Sound)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // 세팅 UI의 SoundMix가 자동으로 적용됨
    UGameplayStatics::PlaySoundAtLocation(
        World,
        Sound,
        Location,
        FRotator::ZeroRotator,
        VolumeMultiplier,
        PitchMultiplier,
        0.0f,
        nullptr,
        nullptr,
        nullptr
    );
}

UAudioComponent* UCSoundManagerSubsystem::PlaySFX3DAttached(
    USoundBase* Sound, 
    USceneComponent* AttachToComponent,
    FName AttachPointName,
    FVector Location,
    float VolumeMultiplier,
    float PitchMultiplier)
{
    if (!Sound || !AttachToComponent)
    {
        return nullptr;
    }

    // 세팅 UI의 SoundMix가 자동으로 적용됨
    UAudioComponent* AudioComp = UGameplayStatics::SpawnSoundAttached(
        Sound,
        AttachToComponent,
        AttachPointName,
        Location,
        FRotator::ZeroRotator,
        EAttachLocation::KeepRelativeOffset,
        false,
        VolumeMultiplier,
        PitchMultiplier,
        0.0f,
        nullptr,
        nullptr,
        false
    );

    return AudioComp;
}



// 사운드 중복 출력 체크(히트시)
void UCSoundManagerSubsystem::PlaySFX3DWithCooldown(
    USoundBase* Sound, 
    const FVector& Location,
    float Cooldown,
    float VolumeMultiplier,
    float PitchMultiplier)
{
    if (!Sound)
        return;

    if (!CanPlaySound(Sound, Cooldown))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[SoundManager] Sound on cooldown: %s"), *Sound->GetName());
        return;
    }

    // 마지막 재생 시간 기록
    UWorld* World = GetWorld();
    if (World)
    {
        SoundCooldowns.Add(Sound, World->GetTimeSeconds());
    }

    PlaySFX3D(Sound, Location, VolumeMultiplier, PitchMultiplier);
}

bool UCSoundManagerSubsystem::CanPlaySound(USoundBase* Sound, float Cooldown)
{
    if (!Sound)
        return false;

    UWorld* World = GetWorld();
    if (!World)
        return true;

    const float CurrentTime = World->GetTimeSeconds();
    
    if (!SoundCooldowns.Contains(Sound))
        return true;

    const float LastPlayTime = SoundCooldowns[Sound];
    const float TimeSinceLastPlay = CurrentTime - LastPlayTime;

    return TimeSinceLastPlay >= Cooldown;
}

// ===============================================
// 사운드 데이터
// ===============================================

void UCSoundManagerSubsystem::SetSoundDataAsset(UCSoundDataAsset* InDataAsset)
{
    SoundDataAsset = InDataAsset;
    UE_LOG(LogTemp, Log, TEXT("[SoundManager] SoundDataAsset set: %s"), 
           InDataAsset ? *InDataAsset->GetName() : TEXT("None"));
}