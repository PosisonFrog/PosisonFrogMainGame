#include "CSimpleSkillBorderRenderer.h"

#include "TextureResource.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RHIStaticStates.h"
#include "CSimpleSkillBorderCS.h"
#include "RHI.h"
#include "RHIResources.h"

void FSimpleSkillBorderRenderer::CopyNow(UTexture2D* Source, UTextureRenderTarget2D* Dest)
{
    if (!Source || !Dest) return;

    // 게임 스레드에서 안전하게 리소스 포인터 확보
    FTextureResource* SrcRes = Source->GetResource();
    FTextureRenderTargetResource* DstRes = Dest->GameThread_GetRenderTargetResource();
    if (!SrcRes || !DstRes) return;

    ENQUEUE_RENDER_COMMAND(SkillBorder_Copy)(
        [SrcRes, DstRes](FRHICommandListImmediate& RHICmdList)
        {
            FSimpleSkillBorderRenderer::Copy_RT(RHICmdList, SrcRes, DstRes);
        }
    );
}

void FSimpleSkillBorderRenderer::Copy_RT(FRHICommandListImmediate& RHICmdList, FTextureResource* SrcRes, FTextureRenderTargetResource* DstRes)
{
    check(IsInRenderingThread());
    if (!SrcRes || !DstRes) return;

    FTextureRHIRef SrcRHI = SrcRes->TextureRHI ? SrcRes->TextureRHI->GetTexture2D() : nullptr;
    FTextureRHIRef DstRHI = DstRes->GetRenderTargetTexture();
    if (!SrcRHI || !DstRHI) return;

    FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("SkillBorder_Copy"));

    // RDG 외부 등록 (읽기용/쓰기용)
    FRDGTextureRef SourceRDG = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(SrcRHI, TEXT("SkillBorder_SourceRDG")));
    FRDGTextureRef OutputRDG = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(DstRHI, TEXT("SkillBorder_OutputRDG")));

    const bool bOutputHasUAV = EnumHasAnyFlags(OutputRDG->Desc.Flags, TexCreate_UAV);
    FRDGTextureRef ComputeTargetRDG = OutputRDG;

    // 출력이 UAV 불가라면 임시 텍스처를 만들어 컴퓨트 → 최종 복사
    if (!bOutputHasUAV)
    {
        FRDGTextureDesc UAVDesc = OutputRDG->Desc;
        UAVDesc.Flags |= TexCreate_UAV;
        ComputeTargetRDG = GraphBuilder.CreateTexture(UAVDesc, TEXT("SkillBorder_ComputeUAV"));
    }

    /*// 중간 입력(소스와 완전 호환 Desc라 복사 안전)
    FRDGTextureDesc IntermediateDesc = SourceRDG->Desc;
    IntermediateDesc.Flags &= ~TexCreate_UAV;
    FRDGTextureRef IntermediateRDG = GraphBuilder.CreateTexture(IntermediateDesc, TEXT("SkillBorder_Intermediate"));

    // 소스 → 중간 복사 (호환)
    AddCopyTexturePass(GraphBuilder, SourceRDG, IntermediateRDG);*/

    const FIntPoint OutSize = OutputRDG->Desc.Extent;
    
    // 컴퓨트 디스패치
    TShaderMapRef<CSimpleSkillBorderCS> CS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
    if (CS.IsValid())
    {
        auto* Params = GraphBuilder.AllocParameters<CSimpleSkillBorderCS::FParameters>();
        Params->InputTexture  = SourceRDG;
        Params->InputSampler  = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        Params->OutputTexture = GraphBuilder.CreateUAV(ComputeTargetRDG);
        Params->TextureSize = FVector2f((float)OutSize.X, (float)OutSize.Y);

        const FIntVector GroupCount(
            FMath::DivideAndRoundUp(OutSize.X, 8),
            FMath::DivideAndRoundUp(OutSize.Y, 8),
            1);

        FComputeShaderUtils::AddPass(GraphBuilder,
            RDG_EVENT_NAME("SkillBorder_CS"),
            CS, Params, GroupCount);
    }

    // 임시 → 실제 출력 복사
    if (ComputeTargetRDG != OutputRDG)
    {
        AddCopyTexturePass(GraphBuilder, ComputeTargetRDG, OutputRDG);
    }

    GraphBuilder.Execute();
}