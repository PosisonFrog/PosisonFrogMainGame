#include "CTutorialPopupWidget.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Texture2D.h"

void UCTutorialPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 버튼 이벤트 바인딩
	if (Button_Continue)
	{
		Button_Continue->OnClicked.AddDynamic(this, &UCTutorialPopupWidget::OnContinueButtonClicked);
	}
}

void UCTutorialPopupWidget::NativeDestruct()
{
	Super::NativeDestruct();

	// 미디어 플레이어 정리
	if (MediaPlayer && MediaPlayer->IsPlaying())
	{
		MediaPlayer->Close();
	}
}

void UCTutorialPopupWidget::SetupTutorial(FName TutorialId)
{
	CurrentTutorialId = TutorialId;

	// 가이드 이미지 설정
	if (TObjectPtr<UTexture2D>* GuideImage = TutorialGuideImages.Find(TutorialId))
	{
		if (Image_TutorialGuide && *GuideImage)
		{
			Image_TutorialGuide->SetBrushFromTexture(*GuideImage);
		}
	}

	// 비디오 설정 및 재생
	if (TObjectPtr<UMediaSource>* VideoSource = TutorialVideoSources.Find(TutorialId))
	{
		if (MediaPlayer && MediaTexture && *VideoSource)
		{
			// Media Texture를 Image에 설정
			if (Image_Video)
			{
				FSlateBrush Brush;
				Brush.SetResourceObject(MediaTexture);
				
				// 16:9 비율 유지를 위한 설정
				FVector2D TextureSize(1920.0f, 1080.0f); // 또는 실제 영상 크기
				Brush.ImageSize = TextureSize;
				Brush.DrawAs = ESlateBrushDrawType::Image;
				Brush.Tiling = ESlateBrushTileType::NoTile;
				
				Image_Video->SetBrush(Brush);
			}

			// 비디오 재생
			MediaPlayer->OpenSource(*VideoSource);
			MediaPlayer->Play();
		}
	}

	// 게임 일시정지 및 UI 모드 설정
	PauseGameAndSetUIMode();
}

void UCTutorialPopupWidget::OnContinueButtonClicked()
{
	// 미디어 플레이어 정지
	if (MediaPlayer && MediaPlayer->IsPlaying())
	{
		MediaPlayer->Close();
	}

	// 게임 재개 및 플레이어 입력 모드로 전환
	ResumeGameAndSetGameMode();

	// 위젯 제거
	RemoveFromParent();
}

void UCTutorialPopupWidget::PauseGameAndSetUIMode()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC)
		return;

	// 게임 일시정지
	UGameplayStatics::SetGamePaused(GetWorld(), true);

	// UI 전용 입력 모드
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);

	// 마우스 커서 표시
	PC->bShowMouseCursor = true;
}

void UCTutorialPopupWidget::ResumeGameAndSetGameMode()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC)
		return;

	// 게임 재개
	UGameplayStatics::SetGamePaused(GetWorld(), false);

	// 게임 전용 입력 모드
	FInputModeGameOnly InputMode;
	PC->SetInputMode(InputMode);

	// 마우스 커서 숨기기
	PC->bShowMouseCursor = false;
}