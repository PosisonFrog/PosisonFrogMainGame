// Copyright Epic Games, Inc. All Rights Reserved.

#include "CPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"


#include "00_Character/02_Component/CDashComponent.h"
#include "00_Character/02_Component/CEnhancedInputComponent.h"
#include "00_Character/02_Component/CGameplayTags.h"
#include "00_Character/02_Component/CWeaponComponent.h"

#include "Global.h"


ACPlayerCharacter::ACPlayerCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	DashComponent = CreateDefaultSubobject<UCDashComponent>(TEXT("DashComponent"));
	check(DashComponent);  // 생성 후 즉시 검증

	WeaponComponent = CreateDefaultSubobject<UCWeaponComponent>(TEXT("WeaponComponent"));
	check(WeaponComponent);  // 생성 후 즉시 검증
		
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true; 
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); 

	//가속 및 정지
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 400.0f; 
	SpringArm->bUsePawnControlRotation = true; 

	PlayerCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	PlayerCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName); 
	PlayerCamera->bUsePawnControlRotation = false; 
}

void ACPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->MaxWalkSpeed = WalkingSpeed;

	// 컴포넌트가 없으면 지연 생성 시도
	if (!DashComponent)
	{
		CLog::Log("대시 컴포넌트 없음 - 지연 생성 시도");
		DashComponent = NewObject<UCDashComponent>(this, UCDashComponent::StaticClass(), TEXT("DashComponent"));
		DashComponent->RegisterComponent();
	}

	if (!WeaponComponent)
	{
		CLog::Log("무기 컴포넌트 없음 - 지연 생성 시도");
		WeaponComponent = NewObject<UCWeaponComponent>(this, UCWeaponComponent::StaticClass(), TEXT("WeaponComponent"));
		WeaponComponent->RegisterComponent();
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void ACPlayerCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	UCEnhancedInputComponent* CEnhancedInputComponent = Cast<UCEnhancedInputComponent>(PlayerInputComponent);
	check(CEnhancedInputComponent);
	// 기본 이동 및 시야 입력
	CEnhancedInputComponent->BindActionByTag(InputConfig, CGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ACPlayerCharacter::Move);
	CEnhancedInputComponent->BindActionByTag(InputConfig, CGameplayTags::InputTag_Look, ETriggerEvent::Triggered, this, &ACPlayerCharacter::Look);
	CEnhancedInputComponent->BindActionByTag(InputConfig, CGameplayTags::InputTag_Dash, ETriggerEvent::Started, this, &ACPlayerCharacter::DashStart);
	CEnhancedInputComponent->BindActionByTag(InputConfig, CGameplayTags::InputTag_Attack, ETriggerEvent::Started, this, &ACPlayerCharacter::Attack);
}

void ACPlayerCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ACPlayerCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ACPlayerCharacter::DashStart()
{
	if (IsValid(DashComponent))  // IsValid 함수로 더 안전하게 확인
	{
		CLog::Log("대시 시작 - 컴포넌트 사용 가능");
		DashComponent->StartDash();
	}
	else
	{
		CLog::Log("오류: DashComponent를 찾을 수 없습니다!");
    
		// 컴포넌트를 찾아 참조 설정 시도
		/*UCDashComponent* FoundComponent = FindComponentByClass<UCDashComponent>();
		if (IsValid(FoundComponent))
		{
			CLog::Log("컴포넌트를 찾아서 참조 설정 시도");
			DashComponent = FoundComponent;
			DashComponent->StartDash();
		}*/
	}
}

void ACPlayerCharacter::Attack()
{
	if (IsValid(WeaponComponent))
	{
		CLog::Log("공격 시작 - 컴포넌트 사용 가능");
		WeaponComponent->DoAttack();
	}
}
