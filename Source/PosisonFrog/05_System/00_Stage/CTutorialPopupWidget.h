#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CTutorialPopupWidget.generated.h"

class UImage;
class UButton;
class UMediaPlayer;
class UMediaSource;
class UMediaTexture;
class UTexture2D;

UCLASS()
class POSISONFROG_API UCTutorialPopupWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// 튜토리얼 데이터로 초기화
	void SetupTutorial(FName TutorialId);

protected:
	// 버튼 클릭 이벤트
	UFUNCTION()
	void OnContinueButtonClicked();

	// 게임 일시정지/재개
	void PauseGameAndSetUIMode();
	void ResumeGameAndSetGameMode();

public:
	// ──────────── Widget Binding ────────────
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_TutorialGuide;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Video;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Continue;

	// ──────────── Tutorial Data ────────────
	UPROPERTY(EditAnywhere, Category = "Tutorial")
	TMap<FName, TObjectPtr<UTexture2D>> TutorialGuideImages;

	UPROPERTY(EditAnywhere, Category = "Tutorial")
	TMap<FName, TObjectPtr<UMediaSource>> TutorialVideoSources;

	// Media Player와 Texture를 함께 설정
	UPROPERTY(EditAnywhere, Category = "Tutorial|Media")
	TObjectPtr<UMediaPlayer> MediaPlayer;

	UPROPERTY(EditAnywhere, Category = "Tutorial|Media")
	TObjectPtr<UMediaTexture> MediaTexture;

private:
	FName CurrentTutorialId;
};