#include "CTankerAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"


void UCTankerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	CacheOwnerReferences();
}

void UCTankerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!ChargeComponent.IsValid())
	{
		CacheOwnerReferences();
		if (!ChargeComponent.IsValid())
		{
			return;
		}
	}
	
	UpdateMovementVariables();
	UpdateChargeStateVariables();
}

void UCTankerAnimInstance::CacheOwnerReferences()
{
	OwnerCharacter = Cast<ACharacter>(TryGetPawnOwner());
	if (!OwnerCharacter.IsValid())
	{
		ChargeComponent.Reset();
		MovementComponent.Reset();
		return;
	}
	
	ChargeComponent = OwnerCharacter->FindComponentByClass<UCTankerChargeComponent>();
	if (!ChargeComponent.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TankerAnimInstance] Missing charge component on %s"), *OwnerCharacter->GetName());
	}
	
	MovementComponent = OwnerCharacter->GetCharacterMovement();
	if (!MovementComponent.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TankerAnimInstance] Missing movement component on %s"), *OwnerCharacter->GetName());
	}
}

void UCTankerAnimInstance::UpdateChargeStateVariables()
{
	if (!ChargeComponent.IsValid())
	{
		ChargeState = EChargeState::Idle;
		bIsIdle = true;
		bIsPreCharge = bIsWindup = bIsCharging = bIsRecovery = bIsOnCooldown = false;
		return;
	}
	
	ChargeState = ChargeComponent->GetState();
	
	bIsIdle = (ChargeState == EChargeState::Idle);
	bIsPreCharge = (ChargeState == EChargeState::PreCharge);
	bIsWindup = (ChargeState == EChargeState::Windup);
	bIsCharging = (ChargeState == EChargeState::Charging);
	bIsRecovery = (ChargeState == EChargeState::Recovery);
	bIsOnCooldown = (ChargeState == EChargeState::Cooldown);
}

void UCTankerAnimInstance::UpdateMovementVariables()
{
	if (!MovementComponent.IsValid())
	{
		MoveSpeed = 0.f;
		return;
	}
	
	const FVector Velocity = MovementComponent->Velocity;
	MoveSpeed = Velocity.Size2D();
}

