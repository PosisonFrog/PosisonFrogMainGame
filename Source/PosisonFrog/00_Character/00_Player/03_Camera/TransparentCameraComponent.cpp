#include "TransparentCameraComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"

#include "Global.h"

UTransparentCameraComponent::UTransparentCameraComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UTransparentCameraComponent::SetCameraComponent(UCameraComponent* InCameraComponent)
{
    CameraComponent = InCameraComponent;
}

void UTransparentCameraComponent::BeginPlay()
{
    Super::BeginPlay();
    OwnerCharacter = Cast<ACharacter>(GetOwner());
}

void UTransparentCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UpdateObstructingActors();
}

void UTransparentCameraComponent::UpdateObstructingActors()
{
    if (!OwnerCharacter || !CameraComponent)
    {
        return;
    }

    // 현재 프레임에서 감지된 컴포넌트들을 저장할 Set
    TSet<TObjectPtr<UPrimitiveComponent>> ObstructingComponents_CurrentFrame;

    const FVector StartLocation = CameraComponent->GetComponentLocation();
    const FVector EndLocation = OwnerCharacter->GetActorLocation();

    TArray<FHitResult> HitResults;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter);

    GetWorld()->LineTraceMultiByChannel(HitResults, StartLocation, EndLocation, TraceChannel, Params);
    
    for (const FHitResult& Hit : HitResults)
    {
        UPrimitiveComponent* HitComponent = Hit.GetComponent();
        if (IsValid(HitComponent) && HitComponent->IsVisible())
        {
            // 1. 현재 프레임에 감지된 컴포넌트 목록에 추가
            ObstructingComponents_CurrentFrame.Add(HitComponent);
            CLog::Print(FString::Printf(TEXT("Obstructing: %s"), *HitComponent->GetName()), 0.f, 0.1f, FColor::Green);
            // 2. Custom Depth 렌더링을 켠다 (태그 지정)
            HitComponent->SetRenderCustomDepth(true);
        }
    }

    // 3. 이전 프레임에는 있었지만 현재 프레임에는 없는 컴포넌트를 찾는다 (더 이상 시야를 가리지 않음)
    TSet<TObjectPtr<UPrimitiveComponent>> ComponentsToRestore = ObstructingComponents_LastFrame.Difference(ObstructingComponents_CurrentFrame);

    for (UPrimitiveComponent* Comp : ComponentsToRestore)
    {
        if (IsValid(Comp))
        {
            // Custom Depth 렌더링을 끈다 (태그 제거)
            Comp->SetRenderCustomDepth(false);
        }
    }

    // 4. 현재 프레임의 목록을 다음 프레임을 위해 저장
    ObstructingComponents_LastFrame = ObstructingComponents_CurrentFrame;
}