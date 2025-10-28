#include "CTankerAnimInstance.h"

#include "99_Util/CLog.h"
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
	if (bIsIdle == true)
		CLog::Log(TEXT("TankerAniminst = IDleState"));
	bIsPreCharge = (ChargeState == EChargeState::PreCharge);
	if (bIsPreCharge == true)
		CLog::Log(TEXT("TankerAniminst = PreChargeState"));
	bIsWindup = (ChargeState == EChargeState::Windup);
	if (bIsWindup == true)
		CLog::Log(TEXT("TankerAniminst = WindupState"));
	bIsCharging = (ChargeState == EChargeState::Charging);
	if (bIsCharging == true)
		CLog::Log(TEXT("TankerAniminst = ChargeState"));
	bIsRecovery = (ChargeState == EChargeState::Recovery);
	if (bIsRecovery == true)
		CLog::Log(TEXT("TankerAniminst = RecoveryState"));
	bIsOnCooldown = (ChargeState == EChargeState::Cooldown);
	if (bIsOnCooldown == true)
		CLog::Log(TEXT("TankerAniminst = CooldownState"));
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

