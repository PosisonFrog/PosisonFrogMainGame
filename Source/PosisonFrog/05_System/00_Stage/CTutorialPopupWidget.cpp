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

	if (!IsValid(Button_Continue))
	{
		UE_LOG(LogTemp, Error, TEXT("[TutorialPopup] ✗ Button_Continue가 nullptr 또는 유효하지 않음!"));
		UE_LOG(LogTemp, Error, TEXT("[TutorialPopup] WBP_TutorialPopup에 'Button_Continue' 이름의 Button이 있는지 확인하세요!"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[TutorialPopup] ✓ Button_Continue 유효함"));

	if (Button_Continue->OnClicked.IsBound())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TutorialPopup] OnClicked 이미 바인딩되어 있음 - 클리어"));
		Button_Continue->OnClicked.Clear();
	}

	Button_Continue->OnClicked.AddDynamic(this, &UCTutorialPopupWidget::OnContinueButtonClicked);
	UE_LOG(LogTemp, Warning, TEXT("[TutorialPopup] ✓ OnClicked 바인딩 완료!"));

	if (!IsValid(Image_TutorialGuide))
	{
		UE_LOG(LogTemp, Error, TEXT("[TutorialPopup] ✗ Image_TutorialGuide가 nullptr!"));
	}

	if (!IsValid(Image_Video))
	{
		UE_LOG(LogTemp, Error, TEXT("[TutorialPopup] ✗ Image_Video가 nullptr!"));
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
	UE_LOG(LogTemp, Warning, TEXT("[TutorialPopup] SetupTutorial 시작: %s"), *TutorialId.ToString());
	
	CurrentTutorialId = TutorialId;

	if (TObjectPtr<UTexture2D>* GuideImage = TutorialGuideImages.Find(TutorialId))
	{
		if (Image_TutorialGuide && *GuideImage)
		{
			Image_TutorialGuide->SetBrushFromTexture(*GuideImage);
			UE_LOG(LogTemp, Warning, TEXT("[TutorialPopup] ✓ 가이드 이미지 설정 완료"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[TutorialPopup] ✗ Image_TutorialGuide 또는 GuideImage가 nullptr"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[TutorialPopup] ✗ TutorialId '%s'에 해당하는 이미지를 찾을 수 없음"), *TutorialId.ToString());
	}

	// 비디오 설정 및 재생
	if (TObjectPtr<UMediaSource>* VideoSource = TutorialVideoSources.Find(TutorialId))
	{
		if (MediaPlayer && MediaTexture && *VideoSource)
		{
			UE_LOG(LogTemp, Warning, TEXT("[TutorialPopup] ✓ 비디오 소스 발견, 재생 시도"));
			
			// Media Texture를 Image에 설정
			if (Image_Video)
			{
				FSlateBrush Brush;
				Brush.SetResourceObject(MediaTexture);
				
				FVector2D TextureSize(1920.0f, 1080.0f);
				Brush.ImageSize = TextureSize;
				Brush.DrawAs = ESlateBrushDrawType::Image;
				Brush.Tiling = ESlateBrushTileType::NoTile;
				
				Image_Video->SetBrush(Brush);
				UE_LOG(LogTemp, Warning, TEXT("[TutorialPopup] ✓ Video Image 설정 완료"));
			}

			MediaPlayer->OpenSource(*VideoSource);
			MediaPlayer->Play();
			UE_LOG(LogTemp, Warning, TEXT("[TutorialPopup] ✓ 비디오 재생 시작"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[TutorialPopup] ✗ MediaPlayer, MediaTexture, VideoSource 중 하나가 nullptr"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[TutorialPopup] ✗ TutorialId '%s'에 해당하는 비디오를 찾을 수 없음"), *TutorialId.ToString());
	}

	PauseGameAndSetUIMode();
	UE_LOG(LogTemp, Warning, TEXT("[TutorialPopup] ✓ SetupTutorial 완료!"));
}

void UCTutorialPopupWidget::OnContinueButtonClicked()
{
	if (MediaPlayer && MediaPlayer->IsPlaying())
	{
		MediaPlayer->Close();
	}

	ResumeGameAndSetGameMode();

	RemoveFromParent();
}

void UCTutorialPopupWidget::PauseGameAndSetUIMode()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC)
		return;

	UGameplayStatics::SetGamePaused(GetWorld(), true);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);

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