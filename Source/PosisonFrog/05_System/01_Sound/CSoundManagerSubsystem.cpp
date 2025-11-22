#include "CSoundManagerSubsystem.h"
#include "CSoundDataAsset.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

void UCSoundManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bIsBGMPaused = false;
    UE_LOG(LogTemp, Log, TEXT("[SoundManager] Initialized"));
}

void UCSoundManagerSubsystem::Deinitialize()
{
    StopBGM(0.0f);
    BGMAudioComponent = nullptr;
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
        return;
    }
    
    // 1. 이미 오디오 컴포넌트가 있다면 상태 확인
    if (BGMAudioComponent)
    {
        // [중요] StopBGM 호출 전, 같은 곡인지 먼저 확인
        const bool bIsSameBGM = (CurrentBGM == Sound);
            
        if (bIsSameBGM)
        {
            // 이미 재생 중이고, 일시정지 상태라면 재개
            if (bIsBGMPaused)
            {
                BGMAudioComponent->SetPaused(false);
                bIsBGMPaused = false;
                UE_LOG(LogTemp, Log, TEXT("[SoundManager] Resumed existing BGM: %s"), *Sound->GetName());
            }
            else if (!BGMAudioComponent->IsPlaying())
            {
                // 컴포넌트는 있는데 멈춰있다면 다시 재생
                BGMAudioComponent->Play();
                UE_LOG(LogTemp, Log, TEXT("[SoundManager] Restarted existing BGM playback: %s"), *Sound->GetName());
            }
                    
            // 같은 곡이 잘 나오고 있다면 리턴
            return; 
        }

        // 다른 곡이라면 기존 BGM 정지
        StopBGM(FadeInDuration * 0.5f);
    }
    
    // 2. 새로운 BGM 재생 로직
    BGMAudioComponent = UGameplayStatics::SpawnSound2D(
        World,
        Sound,
        VolumeMultiplier,
        1.0f,           // Pitch
        0.0f,           // StartTime
        nullptr,        // ConcurrencySettings
        true,           // bPersistAcrossLevelTransition
        false           // bAutoDestroy (Subsystem 관리를 위해 false)
    );

    if (BGMAudioComponent)
    {
        CurrentBGM = Sound;
        bIsBGMPaused = false;
        
        // 우선순위를 높여서 다른 SFX 때문에 BGM이 꺼지는 것을 방지
        BGMAudioComponent->Priority = 100.0f;

        // 가상화 설정: 소리가 안 들리는 상황(볼륨 0 등)에서도 백그라운드 재생 유지
        BGMAudioComponent->SetIsVirtualized(true); 
        BGMAudioComponent->SetUISound(true); 

        if (FadeInDuration > 0.0f)
        {
            BGMAudioComponent->FadeIn(FadeInDuration, VolumeMultiplier);
        }
        else
        {
            if(!BGMAudioComponent->IsPlaying())
            {
                BGMAudioComponent->Play();
            }
        }

        UE_LOG(LogTemp, Log, TEXT("[SoundManager] Playing BGM: %s"), *Sound->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[SoundManager] Failed to spawn BGM AudioComponent!"));
    }
}

void UCSoundManagerSubsystem::StopBGM(float FadeOutDuration)
{
    if (!BGMAudioComponent) 
    {
        return;
    }

    if (BGMAudioComponent->IsPlaying())
    {
        if (FadeOutDuration > 0.0f)
        {
            BGMAudioComponent->FadeOut(FadeOutDuration, 0.0f);
        }
        else
        {
            BGMAudioComponent->Stop();
        }
    }

    CurrentBGM = nullptr;
    bIsBGMPaused = false;

    // 포인터 연결 해제 (GC가 알아서 처리하도록 둠)
    BGMAudioComponent = nullptr;

    UE_LOG(LogTemp, Log, TEXT("[SoundManager] Stopped BGM"));
}

void UCSoundManagerSubsystem::PauseBGM()
{
    if (BGMAudioComponent && BGMAudioComponent->IsPlaying())
    {
        BGMAudioComponent->SetPaused(true);
        bIsBGMPaused = true;
        UE_LOG(LogTemp, Log, TEXT("[SoundManager] Paused BGM"));
    }
}

void UCSoundManagerSubsystem::ResumeBGM()
{
    if (BGMAudioComponent && bIsBGMPaused)
    {
        BGMAudioComponent->SetPaused(false);
        bIsBGMPaused = false;
        UE_LOG(LogTemp, Log, TEXT("[SoundManager] Resumed BGM"));
    }
}

bool UCSoundManagerSubsystem::IsBGMPlaying() const
{
    return BGMAudioComponent && BGMAudioComponent->IsPlaying();
}

// ===============================================
// SFX
// ===============================================

void UCSoundManagerSubsystem::PlaySFX2D(USoundBase* Sound, float VolumeMultiplier, float PitchMultiplier)
{
    if (!Sound) return;

    UWorld* World = GetWorld();
    if (!World) return;

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
    if (!Sound) return;

    UWorld* World = GetWorld();
    if (!World) return;

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
    if (!Sound || !AttachToComponent) return nullptr;

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
        true // SFX는 Auto Destroy True
    );

    return AudioComp;
}

// 사운드 중복 출력 체크
void UCSoundManagerSubsystem::PlaySFX3DWithCooldown(
    USoundBase* Sound, 
    const FVector& Location,
    float Cooldown,
    float VolumeMultiplier,
    float PitchMultiplier)
{
    if (!Sound) return;

    if (!CanPlaySound(Sound, Cooldown))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (World)
    {
        SoundCooldowns.Add(Sound, World->GetTimeSeconds());
    }

    PlaySFX3D(Sound, Location, VolumeMultiplier, PitchMultiplier);
}

bool UCSoundManagerSubsystem::CanPlaySound(USoundBase* Sound, float Cooldown)
{
    if (!Sound) return false;

    UWorld* World = GetWorld();
    if (!World) return true;

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