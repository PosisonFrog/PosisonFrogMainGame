// Fill out your copyright notice in the Description page of Project Settings.


#include "CSkillBorderCopyWidget.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "CSimpleSkillBorderRenderer.h"

void UCSkillBorderCopyWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UCSkillBorderCopyWidget::NativeDestruct()
{
	FlushRenderingCommands();
	OutputRT = nullptr;
	Super::NativeDestruct();
}

void UCSkillBorderCopyWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if (IsDesignTime())
	{
		return;
	}

	if (!bHasInitializedInGame) // (이전 답변의 bool 플래그는 그대로 유지합니다)
	{
		bHasInitializedInGame = true;

		EnsureRT();
		DispatchOnce();
		ApplyBrush();
	}
}

void UCSkillBorderCopyWidget::SetSourceTexture(UTexture2D* InTex)
{
	SourceTexture = InTex;
	
	if (!IsDesignTime() && bHasInitializedInGame)
	{
		EnsureRT();
		DispatchOnce();
		ApplyBrush();
	}
}

void UCSkillBorderCopyWidget::EnsureRT()
{
	int32 W = OutputSize.X, H = OutputSize.Y;
	if (W <= 0 || H <= 0)
	{
		if (SourceTexture) { W = SourceTexture->GetSizeX(); H = SourceTexture->GetSizeY(); }
		else { W = H = 512; }
	}

	if (OutputRT && OutputRT->SizeX == W && OutputRT->SizeY == H)
		return;

	OutputRT = NewObject<UTextureRenderTarget2D>(this, TEXT("SkillBorder_RT"));
	OutputRT->RenderTargetFormat = RTF_RGBA16f;     // UAV 가능
	
#if ENGINE_MAJOR_VERSION >= 5
	OutputRT->bCanCreateUAV = true;
#endif
	
	OutputRT->bAutoGenerateMips = false;
	OutputRT->ClearColor = FLinearColor::Transparent;
	OutputRT->InitCustomFormat(W, H, PF_FloatRGBA, /*bForceLinearGamma=*/false);
	OutputRT->UpdateResourceImmediate(true);
}

void UCSkillBorderCopyWidget::DispatchOnce()
{
	if (!SourceTexture || !OutputRT) return;
	// 플러그인 렌더러: 게임 스레드 래퍼 호출 (내부에서 렌더 스레드 실행)
	FSimpleSkillBorderRenderer::CopyNow(SourceTexture, OutputRT);
}

void UCSkillBorderCopyWidget::ApplyBrush()
{
	if (PreviewImage && OutputRT)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(OutputRT);
		Brush.ImageSize = FVector2D(OutputRT->SizeX, OutputRT->SizeY);
		PreviewImage->SetBrush(Brush);
	}
}