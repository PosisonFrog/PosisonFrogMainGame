#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "TransparentCameraComponent.generated.h"

class UCameraComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POSISONFROG_API UTransparentCameraComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UTransparentCameraComponent();

	void SetCameraComponent(UCameraComponent* InCameraComponent);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// 시야를 가리는 액터들의 PrimitiveComponent에 Custom Depth를 활성화하는 함수
	void UpdateObstructingActors();

public:
	// 라인 트레이스가 감지할 콜리전 채널
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace Settings")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

private:
	UPROPERTY()
	TObjectPtr<UCameraComponent> CameraComponent = nullptr;

	UPROPERTY()
	TObjectPtr<ACharacter> OwnerCharacter = nullptr;
    
	// 이전 프레임에서 시야를 가리고 있던 컴포넌트들의 목록
	UPROPERTY()
	TSet<TObjectPtr<UPrimitiveComponent>> ObstructingComponents_LastFrame;
};