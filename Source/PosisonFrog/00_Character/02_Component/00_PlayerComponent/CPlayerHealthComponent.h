// CHealthComponent.h

#pragma once

#include "CoreMinimal.h"
#include "00_Character/02_Component/CBaseHealthComponent.h"
#include "CPlayerHealthComponent.generated.h"

class UCPlayerStatAssetData;

/**
 * 플레이어 체력 컴포넌트
 * - 버프 시스템 연동 (IBuffable 인터페이스)
 * - UI 갱신은 ACPlayerCharacter에서 처리
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class POSISONFROG_API UCPlayerHealthComponent : public UCBaseHealthComponent
{
	GENERATED_BODY()

public:
	// 버프 시스템 통합
	virtual float Damage(float InAmount) override;

	// 플레이어 전용 사망 처리
	virtual void OnDeathInternal() override;
};

