#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CSoundManagerSubsystem.generated.h"

class USoundBase;
class UAudioComponent;
class UCSoundDataAsset;

/**
 * 사운드 매니저
 * - BGM 재생/정지/페이드 전담 (우선순위 최상위 유지)
 * - SFX 재생 헬퍼 
 */
UCLASS()
class POSISONFROG_API UCSoundManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ─────────── BGM ───────────
    /** BGM 재생 (페이드인 옵션) */
    UFUNCTION(BlueprintCallable, Category = "Sound|BGM")
    void PlayBGM(USoundBase* Sound, float FadeInDuration = 1.0f, float VolumeMultiplier = 1.0f);

    /** BGM 정지 (페이드아웃 옵션) */
    UFUNCTION(BlueprintCallable, Category = "Sound|BGM")
    void StopBGM(float FadeOutDuration = 1.0f);

    /** BGM 일시정지 */
    UFUNCTION(BlueprintCallable, Category = "Sound|BGM")
    void PauseBGM();

    /** BGM 재개 */
    UFUNCTION(BlueprintCallable, Category = "Sound|BGM")
    void ResumeBGM();

    /** 현재 BGM이 재생 중인지 */
    UFUNCTION(BlueprintPure, Category = "Sound|BGM")
    bool IsBGMPlaying() const;


    // ─────────── SFX ───────────
    /** 2D SFX 재생 (UI 사운드 등) */
    UFUNCTION(BlueprintCallable, Category = "Sound|SFX")
    void PlaySFX2D(USoundBase* Sound, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

    /** 3D SFX 재생 (월드 위치에서) */
    UFUNCTION(BlueprintCallable, Category = "Sound|SFX")
    void PlaySFX3D(USoundBase* Sound, const FVector& Location, 
                    float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);
    
    /** 3D SFX 재생 (액터에 부착) */
    UFUNCTION(BlueprintCallable, Category = "Sound|SFX")
    UAudioComponent* PlaySFX3DAttached(USoundBase* Sound, USceneComponent* AttachToComponent, 
                                        FName AttachPointName = NAME_None, 
                                        FVector Location = FVector::ZeroVector,
                                        float VolumeMultiplier = 1.0f, 
                                        float PitchMultiplier = 1.0f);

    /** 쿨다운을 가진 3D SFX 재생 (동일 사운드 중복 방지) */
    UFUNCTION(BlueprintCallable, Category = "Sound|SFX")
    void PlaySFX3DWithCooldown(USoundBase* Sound, const FVector& Location, 
                                float Cooldown = 0.1f, float VolumeMultiplier = 1.0f, 
                                float PitchMultiplier = 1.0f);


    // ─────────── 사운드 데이터 ───────────
    /** SoundDataAsset 설정 (GameMode에서 호출) */
    UFUNCTION(BlueprintCallable, Category = "Sound")
    void SetSoundDataAsset(UCSoundDataAsset* InDataAsset);

    /** SoundDataAsset 가져오기 */
    UFUNCTION(BlueprintPure, Category = "Sound")
    UCSoundDataAsset* GetSoundDataAsset() const { return SoundDataAsset; }

private:
    // ─────────── BGM 관리 ───────────
    
    // GC(가비지 컬렉터)가 재생 중인 BGM 컴포넌트를 없애고 있었음
    UPROPERTY()
    TObjectPtr<UAudioComponent> BGMAudioComponent = nullptr;

    UPROPERTY()
    TObjectPtr<USoundBase> CurrentBGM = nullptr;

    // BGM pause 상태 추적
    bool bIsBGMPaused = false;

    
    // ─────────── 사운드 데이터 ───────────
    UPROPERTY(Transient)
    TObjectPtr<UCSoundDataAsset> SoundDataAsset = nullptr;

    
    // ─────────── 쿨다운 시스템 ───────────
    // 사운드별 마지막 재생 시간 추적
    UPROPERTY(Transient)
    TMap<USoundBase*, float> SoundCooldowns;
    
    // 쿨다운 체크 헬퍼
    bool CanPlaySound(USoundBase* Sound, float Cooldown);
};