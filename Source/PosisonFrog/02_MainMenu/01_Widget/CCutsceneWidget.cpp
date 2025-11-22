#include "CCutsceneWidget.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Animation/WidgetAnimation.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Engine/Texture2D.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

void UCCutsceneWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 입력 모드 설정 (UI에만 입력 전달)
    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeUIOnly InputMode;
        InputMode.SetWidgetToFocus(TakeWidget());
        PC->SetInputMode(InputMode);
        PC->bShowMouseCursor = true;
    }

    // 초기화
    CurrentPageIndex = 0;
    CurrentFrameIndex = 0;
    bIsPlaying = false;
    bCanAcceptInput = true;
    bIsShaking = false;

    if (bShowDebugInfo)
    {
        UE_LOG(LogTemp, Log, TEXT("[Cutscene] Widget Constructed. Total Pages: %d"), CutscenePages.Num());
    }
}

void UCCutsceneWidget::NativeDestruct()
{
    // 타이머 정리
    if (InputLockTimer.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(InputLockTimer);
    }
    
    if (ShakeTimer.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ShakeTimer);
    }
    
    if (FadeUpdateTimer.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(FadeUpdateTimer);
    }

    // 입력 모드 복원
    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeGameOnly InputMode;
        PC->SetInputMode(InputMode);
        PC->bShowMouseCursor = false;
    }

    Super::NativeDestruct();
}

void UCCutsceneWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
}

FReply UCCutsceneWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (bIsPlaying && bCanAcceptInput)
    {
        ProcessInput();
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UCCutsceneWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bIsPlaying && bCanAcceptInput)
    {
        ProcessInput();
        return FReply::Handled();
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UCCutsceneWidget::ProcessInput()
{
    if (!bCanAcceptInput || !bIsPlaying)
    {
        return;
    }

    // 흔들림 중에는 입력 무시
    if (bIsShaking)
    {
        return;
    }

    // 페이드 중에는 입력 무시
    if (!AreAllFadesComplete())
    {
        return;
    }

    // 입력 일시 잠금
    LockInput(0.3f);

    // 현재 페이지 범위 확인
    if (!CutscenePages.IsValidIndex(CurrentPageIndex))
    {
        EndCutscene();
        return;
    }

    const FCutscenePage& CurrentPage = CutscenePages[CurrentPageIndex];

    // 현재 페이지 내에 다음 컷이 있는지 확인
    if (CurrentFrameIndex + 1 < CurrentPage.Frames.Num())
    {
        // 같은 페이지 내에서 다음 컷으로 진행
        ShowNextFrame();
    }
    else
    {
        // 페이지의 마지막 컷이었으므로 다음 페이지로
        ShowNextPage();
    }
}

void UCCutsceneWidget::StartCutscene()
{
    if (bIsPlaying)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Cutscene] Already playing!"));
        return;
    }

    if (CutscenePages.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[Cutscene] No pages defined!"));
        EndCutscene();
        return;
    }

    bIsPlaying = true;
    CurrentPageIndex = 0;
    CurrentFrameIndex = 0;

    // 첫 번째 페이지의 첫 번째 컷 표시
    if (CutscenePages.IsValidIndex(0) && CutscenePages[0].Frames.Num() > 0)
    {
        const FCutscenePage& FirstPage = CutscenePages[0];
        
        // 첫 번째 프레임 표시
        DisplayFrame(FirstPage.Frames[0]);
        
        // 페이지 레벨 페이드 인이 설정되어 있으면 적용
        if (FirstPage.bFadeInOnPageStart)
        {
            StartPageFadeIn(FirstPage.PageFadeInDuration);
        }
    }

    LogCutsceneProgress();

    // 블루프린트 이벤트 호출
    BP_OnCutsceneStarted();
    BP_OnFrameChanged(CurrentPageIndex, CurrentFrameIndex);

    if (bShowDebugInfo)
    {
        UE_LOG(LogTemp, Log, TEXT("[Cutscene] Started!"));
    }
}

void UCCutsceneWidget::ShowNextFrame()
{
    CurrentFrameIndex++;

    if (!CutscenePages.IsValidIndex(CurrentPageIndex))
    {
        EndCutscene();
        return;
    }

    const FCutscenePage& CurrentPage = CutscenePages[CurrentPageIndex];

    if (!CurrentPage.Frames.IsValidIndex(CurrentFrameIndex))
    {
        // 유효하지 않은 인덱스면 다음 페이지로
        ShowNextPage();
        return;
    }

    // 컷 전환 사운드
    PlaySound(SFX_FrameTransition);

    // 새 컷 표시 (기존 컷 위에 쌓임)
    const FCutsceneFrame& NextFrame = CurrentPage.Frames[CurrentFrameIndex];
    DisplayFrame(NextFrame);

    LogCutsceneProgress();

    // 블루프린트 이벤트 호출
    BP_OnFrameChanged(CurrentPageIndex, CurrentFrameIndex);
}

void UCCutsceneWidget::ShowNextPage()
{
    // 다음 페이지가 있는지 확인
    if (CurrentPageIndex + 1 >= CutscenePages.Num())
    {
        // 마지막 페이지였으므로 컷신 종료
        EndCutscene();
        return;
    }

    // 현재 페이지의 페이드 아웃 설정 확인
    const FCutscenePage& CurrentPage = CutscenePages[CurrentPageIndex];
    if (CurrentPage.bFadeOutOnPageEnd)
    {
        // 페이지 전체 페이드 아웃 시작
        StartPageFadeOut(CurrentPage.PageFadeOutDuration);
        
        // 페이드 아웃이 완료된 후 페이지 전환
        FTimerHandle PageTransitionTimer;
        GetWorld()->GetTimerManager().SetTimer(
            PageTransitionTimer,
            [this]()
            {
                // 현재 페이지 클리어
                ClearCurrentPage();

                // 다음 페이지로 이동
                CurrentPageIndex++;
                CurrentFrameIndex = 0;

                // 페이지 전환 사운드
                PlaySound(SFX_PageTransition);

                // 새 페이지의 첫 번째 컷 표시
                if (CutscenePages.IsValidIndex(CurrentPageIndex))
                {
                    const FCutscenePage& NewPage = CutscenePages[CurrentPageIndex];
                    if (NewPage.Frames.Num() > 0)
                    {
                        DisplayFrame(NewPage.Frames[0]);
                        
                        // 새 페이지 페이드 인이 설정되어 있으면 적용
                        if (NewPage.bFadeInOnPageStart)
                        {
                            StartPageFadeIn(NewPage.PageFadeInDuration);
                        }
                    }
                }

                LogCutsceneProgress();

                // 블루프린트 이벤트 호출
                BP_OnPageChanged(CurrentPageIndex);
                BP_OnFrameChanged(CurrentPageIndex, CurrentFrameIndex);

                if (bShowDebugInfo)
                {
                    UE_LOG(LogTemp, Log, TEXT("[Cutscene] Page changed to: %d"), CurrentPageIndex);
                }
            },
            CurrentPage.PageFadeOutDuration,
            false
        );
    }
    else
    {
        // 페이드 아웃 없이 즉시 전환
        ClearCurrentPage();

        // 다음 페이지로 이동
        CurrentPageIndex++;
        CurrentFrameIndex = 0;

        // 페이지 전환 사운드
        PlaySound(SFX_PageTransition);

        // 새 페이지의 첫 번째 컷 표시
        if (CutscenePages.IsValidIndex(CurrentPageIndex))
        {
            const FCutscenePage& NewPage = CutscenePages[CurrentPageIndex];
            if (NewPage.Frames.Num() > 0)
            {
                DisplayFrame(NewPage.Frames[0]);
                
                // 새 페이지 페이드 인이 설정되어 있으면 적용
                if (NewPage.bFadeInOnPageStart)
                {
                    StartPageFadeIn(NewPage.PageFadeInDuration);
                }
            }
        }

        LogCutsceneProgress();

        // 블루프린트 이벤트 호출
        BP_OnPageChanged(CurrentPageIndex);
        BP_OnFrameChanged(CurrentPageIndex, CurrentFrameIndex);

        if (bShowDebugInfo)
        {
            UE_LOG(LogTemp, Log, TEXT("[Cutscene] Page changed to: %d"), CurrentPageIndex);
        }
    }
}

void UCCutsceneWidget::ClearCurrentPage()
{
    // 프레임별 페이드 아웃이 설정된 경우 처리
    bool bAnyFadeOut = false;
    
    if (CutscenePages.IsValidIndex(CurrentPageIndex))
    {
        const FCutscenePage& CurrentPage = CutscenePages[CurrentPageIndex];
        
        for (int32 i = 0; i < ActiveFrameImages.Num() && i < CurrentPage.Frames.Num(); ++i)
        {
            const FCutsceneFrame& Frame = CurrentPage.Frames[i];
            
            if (Frame.bFadeOutOnHide && ActiveFrameImages[i])
            {
                StartImageFadeOut(ActiveFrameImages[i], Frame.FadeOutDuration);
                bAnyFadeOut = true;
            }
        }
    }
    
    // 페이드 아웃이 있으면 완료 후 제거, 없으면 즉시 제거
    if (!bAnyFadeOut)
    {
        for (UImage* FrameImage : ActiveFrameImages)
        {
            if (FrameImage && CutsceneCanvas)
            {
                CutsceneCanvas->RemoveChild(FrameImage);
            }
        }

        ActiveFrameImages.Empty();

        if (bShowDebugInfo)
        {
            UE_LOG(LogTemp, Log, TEXT("[Cutscene] Cleared current page"));
        }
    }
}

void UCCutsceneWidget::DisplayFrame(const FCutsceneFrame& Frame)
{
    if (!CutsceneCanvas)
    {
        UE_LOG(LogTemp, Error, TEXT("[Cutscene] CutsceneCanvas is null!"));
        return;
    }

    // 새 이미지 위젯 생성
    UImage* NewFrameImage = NewObject<UImage>(this, UImage::StaticClass());
    if (!NewFrameImage)
    {
        UE_LOG(LogTemp, Error, TEXT("[Cutscene] Failed to create Image widget!"));
        return;
    }

    // 캔버스에 추가
    UCanvasPanelSlot* CanvasSlot = CutsceneCanvas->AddChildToCanvas(NewFrameImage);
    if (!CanvasSlot)
    {
        UE_LOG(LogTemp, Error, TEXT("[Cutscene] Failed to add Image to Canvas!"));
        return;
    }

    // 레이아웃 설정
    if (Frame.bUseFullScreen)
    {
        // 풀 스크린 모드
        CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        CanvasSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
        CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
    }
    else
    {
        // 커스텀 레이아웃 - Size는 텍스처 로딩 후 설정
        CanvasSlot->SetAnchors(Frame.Anchors);
        CanvasSlot->SetPosition(Frame.Position);
        CanvasSlot->SetAlignment(Frame.Alignment);
    }
    
    // Z-Order 설정
    CanvasSlot->SetZOrder(Frame.ZOrder);

    // 이미지 로딩 및 설정
    if (!Frame.FrameImage.IsNull())
    {
        // 비동기 로딩
        FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
        TSharedPtr<FStreamableHandle> Handle = Streamable.RequestAsyncLoad(
            Frame.FrameImage.ToSoftObjectPath(),
            [NewFrameImage, CanvasSlot, Frame]()
            {
                if (IsValid(NewFrameImage) && IsValid(CanvasSlot))
                {
                    if (UTexture2D* LoadedTexture = Frame.FrameImage.Get())
                    {
                        NewFrameImage->SetBrushFromTexture(LoadedTexture);
                        
                        // 텍스처 원본 크기 가져오기 + Scale 적용
                        if (!Frame.bUseFullScreen)
                        {
                            FVector2D TextureSize = FVector2D(LoadedTexture->GetSizeX(), LoadedTexture->GetSizeY());
                            FVector2D ScaledSize = TextureSize * Frame.Scale;
                            CanvasSlot->SetSize(ScaledSize);
                        }
                    }
                }
            }
        );
    }

    // ActiveFrameImages에 추가
    ActiveFrameImages.Add(NewFrameImage);

    // 프레임별 페이드 인 적용
    if (Frame.bFadeInOnShow)
    {
        StartImageFadeIn(NewFrameImage, Frame.FadeInDuration);
    }
    else
    {
        // 페이드 인이 없으면 완전 불투명으로 시작
        NewFrameImage->SetRenderOpacity(1.0f);
    }

    // 흔들림 효과 적용
    if (Frame.bShakeOnTransition)
    {
        PlayShakeEffect(NewFrameImage, Frame.ShakeIntensity, Frame.ShakeDuration);
    }

    if (Frame.FrameSound)
    {
        // 프레임에 설정된 볼륨으로 재생
        PlayFrameEffectSound(Frame.FrameSound, Frame.FrameSoundVolume);
        
        if (bShowDebugInfo)
        {
            UE_LOG(LogTemp, Log, TEXT("[Cutscene] Playing frame sound: %s (Vol: %.1f)"), *Frame.FrameSound->GetName(), Frame.FrameSoundVolume);
        }
    }

    if (bShowDebugInfo)
    {
        UE_LOG(LogTemp, Log, TEXT("[Cutscene] Displayed frame. Total active frames: %d"), ActiveFrameImages.Num());
    }
}

void UCCutsceneWidget::PlayShakeEffect(UImage* TargetImage, float Intensity, float Duration)
{
    if (!TargetImage || Intensity <= 0.0f || Duration <= 0.0f)
    {
        if (bShowDebugInfo)
        {
            UE_LOG(LogTemp, Warning, TEXT("[Cutscene] Invalid shake parameters!"));
        }
        return;
    }

    bIsShaking = true;
    ShakeElapsedTime = 0.0f;
    ShakeTotalDuration = Duration;
    ShakeCurrentIntensity = Intensity;
    
    // 흔들릴 이미지 저장
    ShakingImage = TargetImage;

    // 이미지의 RenderTransform 초기화
    ShakeOriginalPosition = TargetImage->GetRenderTransform().Translation;

    // 타이머 시작 (60fps 기준, 약 0.016초마다 업데이트)
    GetWorld()->GetTimerManager().SetTimer(
        ShakeTimer,
        this,
        &UCCutsceneWidget::ShakeUpdate,
        0.016f,
        true  // 반복
    );

    if (bShowDebugInfo)
    {
        UE_LOG(LogTemp, Log, TEXT("[Cutscene] Shake effect started on image: Intensity=%.2f, Duration=%.2f"), Intensity, Duration);
    }
}

void UCCutsceneWidget::PlayFrameEffectSound(USoundBase* Sound, float Volume, float Pitch)
{
    if (Sound)
    {
        UGameplayStatics::PlaySound2D(this, Sound, Volume, Pitch);
    }
}

void UCCutsceneWidget::ShakeUpdate()
{
    // 흔들릴 이미지가 유효한지 확인
    if (!ShakingImage.IsValid() || !bIsShaking)
    {
        OnShakeFinished();
        return;
    }

    ShakeElapsedTime += 0.016f;  // 타이머 간격

    // 진행률 계산 (0.0 ~ 1.0)
    float Progress = FMath::Clamp(ShakeElapsedTime / ShakeTotalDuration, 0.0f, 1.0f);

    // 시간이 지남에 따라 감쇠 (1.0 → 0.0)
    float DecayFactor = 1.0f - Progress;

    // 랜덤 오프셋 생성 (-1.0 ~ 1.0)
    float RandomX = FMath::FRandRange(-1.0f, 1.0f);
    float RandomY = FMath::FRandRange(-1.0f, 1.0f);

    // Intensity와 감쇠 적용
    float OffsetX = RandomX * ShakeCurrentIntensity * DecayFactor * 10.0f;  // 10배 스케일
    float OffsetY = RandomY * ShakeCurrentIntensity * DecayFactor * 10.0f;

    // 이미지의 RenderTransform 적용
    UImage* ImagePtr = ShakingImage.Get();
    if (ImagePtr)
    {
        FWidgetTransform Transform = ImagePtr->GetRenderTransform();
        Transform.Translation = ShakeOriginalPosition + FVector2D(OffsetX, OffsetY);
        ImagePtr->SetRenderTransform(Transform);
    }

    // 완료 확인
    if (Progress >= 1.0f)
    {
        OnShakeFinished();
    }
}

void UCCutsceneWidget::OnShakeFinished()
{
    if (!bIsShaking)
    {
        return;
    }

    bIsShaking = false;

    // 타이머 정리
    if (ShakeTimer.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ShakeTimer);
    }

    // 이미지 원래 위치로 복원
    if (ShakingImage.IsValid())
    {
        UImage* ImagePtr = ShakingImage.Get();
        FWidgetTransform Transform = ImagePtr->GetRenderTransform();
        Transform.Translation = ShakeOriginalPosition;
        ImagePtr->SetRenderTransform(Transform);
    }
    
    // 참조 해제
    ShakingImage.Reset();

    if (bShowDebugInfo)
    {
        UE_LOG(LogTemp, Log, TEXT("[Cutscene] Shake finished"));
    }
}

void UCCutsceneWidget::StartImageFadeIn(UImage* TargetImage, float Duration)
{
    if (!TargetImage || Duration <= 0.0f)
    {
        return;
    }

    // 이미지를 투명하게 시작
    TargetImage->SetRenderOpacity(0.0f);

    // 페이드 상태 추가
    FImageFadeState NewFadeState;
    NewFadeState.TargetImage = TargetImage;
    NewFadeState.ElapsedTime = 0.0f;
    NewFadeState.TotalDuration = Duration;
    NewFadeState.StartOpacity = 0.0f;
    NewFadeState.TargetOpacity = 1.0f;
    NewFadeState.bIsComplete = false;

    ActiveFadeStates.Add(NewFadeState);

    // 페이드 업데이트 타이머가 실행 중이 아니면 시작
    if (!FadeUpdateTimer.IsValid())
    {
        GetWorld()->GetTimerManager().SetTimer(
            FadeUpdateTimer,
            this,
            &UCCutsceneWidget::UpdateAllFades,
            0.016f,  // 60fps
            true     // 반복
        );
    }

    if (bShowDebugInfo)
    {
        UE_LOG(LogTemp, Log, TEXT("[Cutscene] Started fade in for image (Duration: %.2f)"), Duration);
    }
}

void UCCutsceneWidget::StartImageFadeOut(UImage* TargetImage, float Duration)
{
    if (!TargetImage || Duration <= 0.0f)
    {
        return;
    }

    // 페이드 상태 추가
    FImageFadeState NewFadeState;
    NewFadeState.TargetImage = TargetImage;
    NewFadeState.ElapsedTime = 0.0f;
    NewFadeState.TotalDuration = Duration;
    NewFadeState.StartOpacity = TargetImage->GetRenderOpacity();
    NewFadeState.TargetOpacity = 0.0f;
    NewFadeState.bIsComplete = false;

    ActiveFadeStates.Add(NewFadeState);

    // 페이드 업데이트 타이머가 실행 중이 아니면 시작
    if (!FadeUpdateTimer.IsValid())
    {
        GetWorld()->GetTimerManager().SetTimer(
            FadeUpdateTimer,
            this,
            &UCCutsceneWidget::UpdateAllFades,
            0.016f,  // 60fps
            true     // 반복
        );
    }

    if (bShowDebugInfo)
    {
        UE_LOG(LogTemp, Log, TEXT("[Cutscene] Started fade out for image (Duration: %.2f)"), Duration);
    }
}

void UCCutsceneWidget::StartPageFadeIn(float Duration)
{
    if (Duration <= 0.0f)
    {
        return;
    }

    // 모든 활성 이미지에 페이드 인 적용
    for (UImage* Image : ActiveFrameImages)
    {
        if (Image)
        {
            StartImageFadeIn(Image, Duration);
        }
    }

    if (bShowDebugInfo)
    {
        UE_LOG(LogTemp, Log, TEXT("[Cutscene] Started page fade in (Duration: %.2f, Images: %d)"), Duration, ActiveFrameImages.Num());
    }
}

void UCCutsceneWidget::StartPageFadeOut(float Duration)
{
    if (Duration <= 0.0f)
    {
        return;
    }

    // 모든 활성 이미지에 페이드 아웃 적용
    for (UImage* Image : ActiveFrameImages)
    {
        if (Image)
        {
            StartImageFadeOut(Image, Duration);
        }
    }

    if (bShowDebugInfo)
    {
        UE_LOG(LogTemp, Log, TEXT("[Cutscene] Started page fade out (Duration: %.2f, Images: %d)"), Duration, ActiveFrameImages.Num());
    }
}

void UCCutsceneWidget::UpdateAllFades()
{
    if (ActiveFadeStates.Num() == 0)
    {
        // 모든 페이드가 완료되었으므로 타이머 정리
        if (FadeUpdateTimer.IsValid())
        {
            GetWorld()->GetTimerManager().ClearTimer(FadeUpdateTimer);
            FadeUpdateTimer.Invalidate();
        }
        return;
    }

    // 모든 활성 페이드 업데이트
    for (int32 i = ActiveFadeStates.Num() - 1; i >= 0; --i)
    {
        FImageFadeState& FadeState = ActiveFadeStates[i];

        // 이미지가 유효한지 확인
        if (!FadeState.TargetImage.IsValid())
        {
            ActiveFadeStates.RemoveAt(i);
            continue;
        }

        // 진행 시간 증가
        FadeState.ElapsedTime += 0.016f;

        // 진행률 계산 (0.0 ~ 1.0)
        float Progress = FMath::Clamp(FadeState.ElapsedTime / FadeState.TotalDuration, 0.0f, 1.0f);

        // 선형 보간
        float CurrentOpacity = FMath::Lerp(FadeState.StartOpacity, FadeState.TargetOpacity, Progress);

        // Opacity 적용
        UImage* ImagePtr = FadeState.TargetImage.Get();
        if (ImagePtr)
        {
            ImagePtr->SetRenderOpacity(CurrentOpacity);
        }

        // 완료 확인
        if (Progress >= 1.0f)
        {
            FadeState.bIsComplete = true;
            
            // 페이드 아웃이 완료되고 투명해진 이미지는 캔버스에서 제거
            if (FadeState.TargetOpacity == 0.0f && ImagePtr && CutsceneCanvas)
            {
                CutsceneCanvas->RemoveChild(ImagePtr);
                ActiveFrameImages.Remove(ImagePtr);
            }
            
            ActiveFadeStates.RemoveAt(i);
        }
    }
}

bool UCCutsceneWidget::AreAllFadesComplete() const
{
    return ActiveFadeStates.Num() == 0;
}

void UCCutsceneWidget::EndCutscene()
{
    if (!bIsPlaying)
    {
        return;
    }

    bIsPlaying = false;
    bCanAcceptInput = false;

    // 종료 사운드
    PlaySound(SFX_CutsceneEnd);

    // 블루프린트 이벤트 호출
    BP_OnCutsceneEnded();

    if (bShowDebugInfo)
    {
        UE_LOG(LogTemp, Log, TEXT("[Cutscene] Ended"));
    }

    // 델리게이트 브로드캐스트 (외부에서 컷신 종료 알림 받음)
    OnCutsceneFinished.Broadcast();

    // 레벨 전환 (딜레이 후)
    if (bAutoTravelOnEnd)
    {
        FTimerHandle TravelTimer;
        GetWorld()->GetTimerManager().SetTimer(
            TravelTimer,
            this,
            &UCCutsceneWidget::TravelToNextLevel,
            TravelDelay,
            false
        );
    }
}

void UCCutsceneWidget::TravelToNextLevel()
{
    if (!NextLevelName.IsNone())
    {
        UGameplayStatics::OpenLevel(this, NextLevelName);
    }
    else if (!NextLevelAsset.IsNull())
    {
        UGameplayStatics::OpenLevel(this, FName(*NextLevelAsset.GetAssetName()));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Cutscene] No next level specified!"));
    }
}

void UCCutsceneWidget::LockInput(float Duration)
{
    bCanAcceptInput = false;

    // 기존 타이머 클리어
    if (InputLockTimer.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(InputLockTimer);
    }

    // 새 타이머 설정
    GetWorld()->GetTimerManager().SetTimer(
        InputLockTimer,
        this,
        &UCCutsceneWidget::UnlockInput,
        Duration,
        false
    );
}

void UCCutsceneWidget::UnlockInput()
{
    bCanAcceptInput = true;
}

void UCCutsceneWidget::PlaySound(USoundBase* Sound)
{
    if (Sound)
    {
        UGameplayStatics::PlaySound2D(this, Sound);
    }
}

void UCCutsceneWidget::InitializeAndStart()
{
    // 외부에서 호출하여 컷신을 시작
    StartCutscene();
}

void UCCutsceneWidget::SkipCutscene()
{
    // 즉시 종료 (테스트용)
    if (bIsPlaying)
    {
        UE_LOG(LogTemp, Log, TEXT("[Cutscene] Skipped by user"));
        EndCutscene();
    }
}

void UCCutsceneWidget::LogCutsceneProgress()
{
    if (!bShowDebugInfo)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[Cutscene] Progress: Page %d/%d, Frame %d/%d"),
        CurrentPageIndex + 1,
        CutscenePages.Num(),
        CurrentFrameIndex + 1,
        CutscenePages.IsValidIndex(CurrentPageIndex) ? CutscenePages[CurrentPageIndex].Frames.Num() : 0
    );
}