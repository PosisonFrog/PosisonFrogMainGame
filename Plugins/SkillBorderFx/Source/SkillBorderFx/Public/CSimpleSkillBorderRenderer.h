#pragma once

#include "RHICommandList.h"

class FTextureResource;
class FTextureRenderTargetResource;
class UTexture2D;
class UTextureRenderTarget2D;

class SKILLBORDERFX_API FSimpleSkillBorderRenderer
{
public:
	// 게임 스레드 래퍼: UObject 받아서 렌더 스레드로 넘김
	static void CopyNow(class UTexture2D* Source, class UTextureRenderTarget2D* Dest);

	// 렌더 스레드 전용: 리소스로만 처리 (UObject 접근 금지)
	static void Copy_RT(FRHICommandListImmediate& RHICmdList, FTextureResource* SrcRes, FTextureRenderTargetResource* DstRes);
};