#include "00_Character/02_Component/00_PlayerComponent/CUltimateBuffComponent.h"
#include "CPlayerMovementBuffComponent.h"
#include "00_Character/02_Component/00_PlayerComponent/CPlayerHealthComponent.h"
#include "00_Character/00_Player/CPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PostProcessComponent.h" // 추가

UCUltimateBuffComponent::UCUltimateBuffComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCUltimateBuffComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerChar = Cast<ACPlayerCharacter>(GetOwner());
	if (IsValid(OwnerChar))
	{
		MovementComponent = OwnerChar->GetCharacterMovement();
		MovementBuffComponent = OwnerChar->FindComponentByClass<UCPlayerMovementBuffComponent>();
			
		if (MovementComponent)
			BaseMaxWalkSpeed = MovementComponent->MaxWalkSpeed;

		HealthComponent = OwnerChar->FindComponentByClass<UCPlayerHealthComponent>();
		if (HealthComponent)
			BaseMaxHealth = HealthComponent->GetMaxHealth();

		// ========== PostProcessComponent 찾기 또는 생성 ==========
		PostProcessComponent = OwnerChar->FindComponentByClass<UPostProcessComponent>();
		
		if (!PostProcessComponent)
		{
			// 없으면 런타임에 생성
			PostProcessComponent = NewObject<UPostProcessComponent>(OwnerChar, TEXT("GlitchPostProcess"));
			if (PostProcessComponent)
			{
				PostProcessComponent->RegisterComponent();
				PostProcessComponent->AttachToComponent(OwnerChar->GetRootComponent(), 
					FAttachmentTransformRules::KeepRelativeTransform);
				
				UE_LOG(LogTemp, Log, TEXT("[ULT] PostProcessComponent created dynamically"));
			}
		}

		// PostProcess 설정
		if (PostProcessComponent)
		{
			PostProcessComponent->bUnbound = true; // 전체 화면 영향
			PostProcessComponent->BlendRadius = PostProcessBlendRadius;
			PostProcessComponent->Priority = 1; // 우선순위 설정
			
			// 머티리얼 추가
			if (GlitchMaterial)
			{
				PostProcessComponent->Settings.WeightedBlendables.Array.Empty();
				PostProcessComponent->Settings.WeightedBlendables.Array.Add(
					FWeightedBlendable(1.0f, GlitchMaterial)
				);
				UE_LOG(LogTemp, Log, TEXT("[ULT] Glitch Material assigned to PostProcess"));
			}
			
			// 초기 비활성화
			PostProcessComponent->bEnabled = false;
			UE_LOG(LogTemp, Log, TEXT("[ULT] PostProcessComponent initialized (disabled)"));
		}
		// ======================================================
	}
}

void UCUltimateBuffComponent::ActivateUltimate()
{
	ApplyAll();
	EnablePostProcess();
}

void UCUltimateBuffComponent::DeactivateUltimate()
{
	RestoreAll();
	DisablePostProcess();
}

void UCUltimateBuffComponent::ApplyAll()
{
	bIsActive = true;
	
	// 이동 속도
	if (MovementBuffComponent && BaseMaxWalkSpeed > 0.0f)
	{
		float NewBaseSpeed = BaseMaxWalkSpeed * MoveSpeedMul;
		MovementBuffComponent->SetBaseMaxWalkSpeed(NewBaseSpeed);
		UE_LOG(LogTemp, Log, TEXT("[ULT][On] MoveSpeed %.1f -> %.1f (x%.2f)"), 
			BaseMaxWalkSpeed, NewBaseSpeed, MoveSpeedMul);
	}

	OutgoingDamageMul = DamageMul;
	UE_LOG(LogTemp, Log, TEXT("[ULT][On] OutgoingDamageMul=%.2f"), OutgoingDamageMul);
	
	// 최대 HP
	if (HealthComponent)
	{
		const float NewMax = FMath::Max(1.0f, BaseMaxHealth * MaxHpMul);
		HealthComponent->SetMaxHealth(NewMax);

		if (bHealOnActivate)
		{
			float HealAmount = NewMax * HealPercent;
			HealthComponent->Healing(HealAmount);
			UE_LOG(LogTemp, Log, TEXT("[ULT][On] MaxHP %.1f -> %.1f, Heal=%.1f"), 
				BaseMaxHealth, NewMax, HealAmount);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[ULT][On] MaxHP %.1f -> %.1f"), BaseMaxHealth, NewMax);
		}
	}

	// 방어력
	switch (DefenseMode)
	{
	case EUltDefenseMode::PercentReduction:
		IncomingDamageScale = 1.0f - FMath::Clamp(DamageReduction01, 0.0f, 1.0f);
		break;
	case EUltDefenseMode::ArmorStat:
		IncomingDamageScale = FMath::Max(MinDamageScale, 1.0f / (1.0f + ArmorValue / ArmorK));
		break;
	default:
		break;
	}
	UE_LOG(LogTemp, Log, TEXT("[ULT][On] IncomingScale %.2f (Mode=%d)"), 
		IncomingDamageScale, DefenseMode);
}

void UCUltimateBuffComponent::RestoreAll()
{
	bIsActive = false;
	
	if (MovementBuffComponent && BaseMaxWalkSpeed > 0.0f)
	{
		MovementBuffComponent->SetBaseMaxWalkSpeed(BaseMaxWalkSpeed);
		UE_LOG(LogTemp, Log, TEXT("[ULT][Off] MoveSpeed restore -> %.1f"), BaseMaxWalkSpeed);
	}

	OutgoingDamageMul = DefaultDamageMultiplier;
	UE_LOG(LogTemp, Log, TEXT("[ULT][Off] OutDamageMul -> %.2f"), OutgoingDamageMul);

	if (HealthComponent && BaseMaxHealth > 0.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("[ULT][Off] MaxHp -> %.1f"), BaseMaxHealth);
		HealthComponent->SetMaxHealth(BaseMaxHealth);
	}

	IncomingDamageScale = 1.0f;
	UE_LOG(LogTemp, Log, TEXT("[ULT][Off] IncomingDamageScale -> %.2f"), IncomingDamageScale);
}

void UCUltimateBuffComponent::EnablePostProcess()
{
	if (PostProcessComponent)
	{
		PostProcessComponent->bEnabled = true;
		UE_LOG(LogTemp, Log, TEXT("[ULT][PostProcess] Glitch Effect Enabled"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ULT][PostProcess] PostProcessComponent is NULL!"));
	}
}

void UCUltimateBuffComponent::DisablePostProcess()
{
	if (PostProcessComponent)
	{
		PostProcessComponent->bEnabled = false;
		UE_LOG(LogTemp, Log, TEXT("[ULT][PostProcess] Glitch Effect Disabled"));
	}
}