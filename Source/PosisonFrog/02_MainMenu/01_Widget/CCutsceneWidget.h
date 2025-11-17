#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CCutsceneWidget.generated.h"

// ── 전방 선언 ──
class UImage;
class UCanvasPanel;
class UWidgetAnimation;
class USoundBase;
class UWorld;

/**
 * 개별 이미지 페이드 상태 추적 구조체
 */
USTRUCT()
struct FImageFadeState
{
    GENERATED_BODY()

    // 페이드 중인 이미지 위젯
    TWeakObjectPtr<UImage> TargetImage;
    
    // 페이드 진행 시간
    float ElapsedTime = 0.0f;
    
    // 페이드 총 지속 시간
    float TotalDuration = 0.0f;
    
    // 시작 투명도
    float StartOpacity = 0.0f;
    
    // 목표 투명도
    float TargetOpacity = 1.0f;
    
    // 페이드 완료 여부
    bool bIsComplete = false;
};

/**
 * 컷신 프레임 구조체
 * 각 컷의 이미지와 전환 효과를 정의
 */
USTRUCT(BlueprintType)
struct FCutsceneFrame
{
    GENERATED_BODY()

    // 컷 이미지 (WBP에서 설정)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene")
    TSoftObjectPtr<UTexture2D> FrameImage;

    // ─────────────────────────────
    // 레이아웃 설정
    // ─────────────────────────────
    
    // 풀스크린 모드 사용 (true면 아래 설정 무시하고 전체 화면)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene|Layout")
    bool bUseFullScreen = true;
    
    // 앵커 설정 (0,0 = 좌상단, 1,1 = 우하단)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene|Layout", meta = (EditCondition = "!bUseFullScreen"))
    FAnchors Anchors = FAnchors(0.5f, 0.5f, 0.5f, 0.5f);  // 중앙 앵커
    
    // 위치 (앵커 기준 오프셋)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene|Layout", meta = (EditCondition = "!bUseFullScreen"))
    FVector2D Position = FVector2D(0.0f, 0.0f);
    
    // 크기 비율 (0.0 ~ 2.0, 1.0 = 100%, 이미지 원본 크기 기준)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene|Layout", meta = (EditCondition = "!bUseFullScreen", ClampMin = "0.01", ClampMax = "2.0", UIMin = "0.01", UIMax = "2.0"))
    float Scale = 1.0f;
    
    // 정렬 (0,0 = 좌상단, 0.5,0.5 = 중앙, 1,1 = 우하단)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene|Layout", meta = (EditCondition = "!bUseFullScreen"))
    FVector2D Alignment = FVector2D(0.5f, 0.5f);
    
    // Z-Order (낮을수록 뒤에, 높을수록 앞에)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene|Layout")
    int32 ZOrder = 0;

    // ─────────────────────────────
    // 페이드 효과
    // ─────────────────────────────
    
    // 이 컷이 표시될 때 페이드 인 적용 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene|Fade")
    bool bFadeInOnShow = true;

    // 페이드 인 지속 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene|Fade", meta = (EditCondition = "bFadeInOnShow", ClampMin = "0.0", ClampMax = "3.0"))
    float FadeInDuration = 0.3f;

    // 이 컷이 제거될 때 페이드 아웃 적용 여부 (페이지 전환 시)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene|Fade")
    bool bFadeOutOnHide = false;

    // 페이드 아웃 지속 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene|Fade", meta = (EditCondition = "bFadeOutOnHide", ClampMin = "0.0", ClampMax = "3.0"))
    float FadeOutDuration = 0.3f;

    // ─────────────────────────────
    // 흔들림 효과
    // ─────────────────────────────
    
    // 이 컷으로 전환될 때 흔들림 효과 적용 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene|Effects")
    bool bShakeOnTransition = false;

    // 흔들림 강도 (0.0 ~ 2.0 권장)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene|Effects", meta = (EditCondition = "bShakeOnTransition", ClampMin = "0.0", ClampMax = "5.0"))
    float ShakeIntensity = 1.0f;

    // 흔들림 지속 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene|Effects", meta = (EditCondition = "bShakeOnTransition", ClampMin = "0.1", ClampMax = "2.0"))
    float ShakeDuration = 0.3f;
};

/**
 * 컷신 페이지 구조체
 * 한 페이지에 여러 컷(프레임)을 포함
 */
USTRUCT(BlueprintType)
struct FCutscenePage
{
    GENERATED_BODY()

    // 이 페이지에 포함된 컷들 (2~4개 권장)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene")
    TArray<FCutsceneFrame> Frames;

    // ─────────────────────────────
    // 페이지 레벨 페이드 설정
    // ─────────────────────────────
    
    // 이 페이지로 전환될 때 페이지 전체 페이드 인 적용 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene|PageFade")
    bool bFadeInOnPageStart = false;

    // 페이지 페이드 인 지속 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene|PageFade", meta = (EditCondition = "bFadeInOnPageStart", ClampMin = "0.0", ClampMax = "3.0"))
    float PageFadeInDuration = 0.5f;

    // 이 페이지에서 나갈 때 페이지 전체 페이드 아웃 적용 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene|PageFade")
    bool bFadeOutOnPageEnd = false;

    // 페이지 페이드 아웃 지속 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene|PageFade", meta = (EditCondition = "bFadeOutOnPageEnd", ClampMin = "0.0", ClampMax = "3.0"))
    float PageFadeOutDuration = 0.5f;
};

// 컷신 종료 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCutsceneFinishedDelegate);

/**
 * 컷신 위젯 클래스
 * 페이지 기반 이미지 컷신 시스템
 */
UCLASS()
class POSISONFROG_API UCCutsceneWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // ─────────────────────────────
    // UI 바인딩
    // ─────────────────────────────
protected:
    // 컷신 이미지를 표시할 루트 캔버스
    UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "Cutscene")
    TObjectPtr<UCanvasPanel> CutsceneCanvas;

    // 현재 표시 중인 컷 이미지들을 담을 컨테이너
    // (한 페이지에 여러 컷이 쌓이므로 동적 생성)
    UPROPERTY()
    TArray<TObjectPtr<UImage>> ActiveFrameImages;

    // ─────────────────────────────
    // 컷신 데이터 (WBP에서 설정)
    // ─────────────────────────────
protected:
    // 전체 컷신 페이지 배열
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutscene", meta = (AllowPrivateAccess = "true"))
    TArray<FCutscenePage> CutscenePages;

    // 컷 전환 사운드
    UPROPERTY(EditDefaultsOnly, Category = "Cutscene|Audio")
    USoundBase* SFX_FrameTransition = nullptr;

    // 페이지 전환 사운드 (선택)
    UPROPERTY(EditDefaultsOnly, Category = "Cutscene|Audio")
    USoundBase* SFX_PageTransition = nullptr;

    // 컷신 종료 사운드 (선택)
    UPROPERTY(EditDefaultsOnly, Category = "Cutscene|Audio")
    USoundBase* SFX_CutsceneEnd = nullptr;

    // ─────────────────────────────
    // 애니메이션 (WBP에서 생성) - 사용하지 않음
    // ─────────────────────────────
protected:
    // 페이드 인 애니메이션 (선택 - 코드로 구현됨)
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    UWidgetAnimation* Anim_FadeIn = nullptr;

    // 페이드 아웃 애니메이션 (선택 - 코드로 구현됨)
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    UWidgetAnimation* Anim_FadeOut = nullptr;

    // ─────────────────────────────
    // 진행 상태 관리
    // ─────────────────────────────
protected:
    // 현재 페이지 인덱스
    int32 CurrentPageIndex = 0;

    // 현재 페이지 내 컷 인덱스
    int32 CurrentFrameIndex = 0;

    // 컷신 진행 중 여부
    bool bIsPlaying = false;

    // 입력 대기 중 여부 (애니메이션 중에는 입력 무시)
    bool bCanAcceptInput = true;

    // 흔들림 애니메이션 재생 중 여부
    bool bIsShaking = false;
    
    // 흔들림 효과 타이머 및 상태
    FTimerHandle ShakeTimer;
    float ShakeElapsedTime = 0.0f;
    float ShakeTotalDuration = 0.0f;
    float ShakeCurrentIntensity = 0.0f;
    FVector2D ShakeOriginalPosition = FVector2D::ZeroVector;
    
    // 현재 흔들리는 이미지 위젯 (약한 참조)
    TWeakObjectPtr<UImage> ShakingImage;
    
    // 개별 이미지 페이드 상태 관리
    TArray<FImageFadeState> ActiveFadeStates;
    FTimerHandle FadeUpdateTimer;

    // ─────────────────────────────
    // 입력 처리
    // ─────────────────────────────
protected:
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    // 입력 처리 (다음 컷/페이지로 진행)
    UFUNCTION()
    void ProcessInput();

    // 입력 잠금/해제
    void LockInput(float Duration = 0.5f);
    void UnlockInput();

    FTimerHandle InputLockTimer;

    // ─────────────────────────────
    // 컷신 제어
    // ─────────────────────────────
protected:
    // 컷신 시작
    UFUNCTION(BlueprintCallable, Category = "Cutscene")
    void StartCutscene();

    // 다음 컷으로 이동
    void ShowNextFrame();

    // 다음 페이지로 이동
    void ShowNextPage();

    // 현재 페이지의 모든 컷 클리어
    void ClearCurrentPage();

    // 특정 컷 표시
    void DisplayFrame(const FCutsceneFrame& Frame);

    // 화면 흔들림 효과 재생 (특정 이미지 위젯)
    void PlayShakeEffect(UImage* TargetImage, float Intensity, float Duration);

    // 화면 흔들림 종료 콜백
    UFUNCTION()
    void OnShakeFinished();
    
    // 화면 흔들림 업데이트 (타이머 콜백)
    void ShakeUpdate();
    
    // 개별 이미지 페이드 인 효과 시작
    void StartImageFadeIn(UImage* TargetImage, float Duration);
    
    // 개별 이미지 페이드 아웃 효과 시작
    void StartImageFadeOut(UImage* TargetImage, float Duration);
    
    // 페이지 전체 페이드 인 (모든 활성 이미지)
    void StartPageFadeIn(float Duration);
    
    // 페이지 전체 페이드 아웃 (모든 활성 이미지)
    void StartPageFadeOut(float Duration);
    
    // 페이드 업데이트 (모든 활성 페이드 처리)
    void UpdateAllFades();
    
    // 페이드 완료 확인
    bool AreAllFadesComplete() const;

    // 컷신 종료
    void EndCutscene();

    // ─────────────────────────────
    // 오디오
    // ─────────────────────────────
protected:
    void PlaySound(USoundBase* Sound);

    // ─────────────────────────────
    // 레벨 전환
    // ─────────────────────────────
protected:
    // 컷신 종료 후 이동할 레벨
    UPROPERTY(EditDefaultsOnly, Category = "Navigation")
    FName NextLevelName = TEXT("PlayLevel");

    // Soft Reference로 레벨 지정 (선택)
    UPROPERTY(EditDefaultsOnly, Category = "Navigation")
    TSoftObjectPtr<UWorld> NextLevelAsset;

    // 컷신 종료 후 자동으로 레벨 이동할지 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation", meta = (AllowPrivateAccess = "true"))
    bool bAutoTravelOnEnd = true;

    // 레벨 전환 전 대기 시간 (페이드 아웃 등을 위해)
    UPROPERTY(EditDefaultsOnly, Category = "Navigation", meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float TravelDelay = 1.0f;

    // 레벨 이동 실행
    void TravelToNextLevel();

    // ─────────────────────────────
    // 블루프린트 이벤트
    // ─────────────────────────────
protected:
    // 컷신 시작 시 호출 (BP에서 추가 로직 가능)
    UFUNCTION(BlueprintImplementableEvent, Category = "Cutscene")
    void BP_OnCutsceneStarted();

    // 컷 전환 시 호출 (BP에서 추가 효과 가능)
    UFUNCTION(BlueprintImplementableEvent, Category = "Cutscene")
    void BP_OnFrameChanged(int32 PageIndex, int32 FrameIndex);

    // 페이지 전환 시 호출
    UFUNCTION(BlueprintImplementableEvent, Category = "Cutscene")
    void BP_OnPageChanged(int32 NewPageIndex);

    // 컷신 종료 시 호출 (BP에서 추가 로직 가능)
    UFUNCTION(BlueprintImplementableEvent, Category = "Cutscene")
    void BP_OnCutsceneEnded();

    // ─────────────────────────────
    // 디버그/유틸리티
    // ─────────────────────────────
protected:
    // 진행 상황 출력 (디버그용)
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bShowDebugInfo = false;

    void LogCutsceneProgress();

public:
    // 컷신 종료 시 호출되는 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Cutscene|Events")
    FOnCutsceneFinishedDelegate OnCutsceneFinished;
    
    // 외부에서 컷신 시작을 트리거할 수 있도록
    UFUNCTION(BlueprintCallable, Category = "Cutscene")
    void InitializeAndStart();

    // 컷신 스킵 (테스트용)
    UFUNCTION(BlueprintCallable, Category = "Cutscene")
    void SkipCutscene();
};