#include "CTankerAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"


void UCTankerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	// Owner 가져오기
	// Owner 캐릭터 가져오기
	OwnerCharacter = Cast<ACharacter>(TryGetPawnOwner());
    
	if (OwnerCharacter)
	{
		// ChargeComponent 캐싱
		ChargeComp = OwnerCharacter->FindComponentByClass<UCTankerChargeComponent>();
        
		if (!ChargeComp)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] CTankerChargeComponent not found on %s"), 
				*OwnerCharacter->GetName());
		}

		// MovementComponent 캐싱
		MovementComp = OwnerCharacter->GetCharacterMovement();
        
		if (!MovementComp)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AnimInstance] CharacterMovementComponent not found on %s"), 
				*OwnerCharacter->GetName());
		}
	}
}

void UCTankerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 컴포넌트 유효성 체크
	if (!ChargeComp || !ChargeComp->IsValidLowLevel())
	{
		return;
	}

	if (MovementComp && MovementComp -> IsValidLowLevel())
	{
		// 속도 계산 (수평 속도만)
		FVector Velocity = MovementComp->Velocity;
		MoveSpeed = Velocity.Size2D(); 
	}

	// Enum State 가져오기
	ChargeState = ChargeComp->GetState();

	// Bool 변수 업데이트 (State Machine에서 사용)
	bIsIdle        = (ChargeState == EChargeState::Idle);
	bIsPreCharge   = (ChargeState == EChargeState::PreCharge);
	bIsWindup      = (ChargeState == EChargeState::Windup);
	bIsCharging    = (ChargeState == EChargeState::Charging);
	bIsRecovery    = (ChargeState == EChargeState::Recovery);
	bIsOnCooldown  = (ChargeState == EChargeState::Cooldown);
}
