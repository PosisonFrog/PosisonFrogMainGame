// Fill out your copyright notice in the Description page of Project Settings.


#include "CPlayerCharacter.h"
#include "00_Character/02_Component/CDashComponent.h"
#include "00_Character/02_Component/CEnhancedInputComponent.h"
#include "00_Character/02_Component/CGameplayTags.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Global.h"



ACPlayerCharacter::ACPlayerCharacter()
{
	DashComponent = CreateDefaultSubobject<UCDashComponent>(TEXT("DashComponent"));
	check(DashComponent);  // 생성 후 즉시 검증

	// 카메라 설정
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetRootComponent());
 
	PlayerCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	PlayerCamera->SetupAttachment(SpringArm);

	
	// 메쉬 및 스프링암 위치/회전 설정
	GetMesh()->SetRelativeLocation(FVector(0, 0, -90));
	GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));
	SpringArm->SetRelativeRotation(FRotator(0, 0, 0));
	SpringArm->SetRelativeLocation(FVector(-18, 63, 49));

	// 캐릭터 무브먼트 설정
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;


	
}
void ACPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	// 초기 설정
	DefaultFOV = PlayerCamera->FieldOfView;
	GetCharacterMovement()->MaxWalkSpeed = WalkingSpeed;

	// 컴포넌트가 없으면 지연 생성 시도
	if (!DashComponent)
	{
		CLog::Log("대시 컴포넌트 없음 - 지연 생성 시도");
		DashComponent = NewObject<UCDashComponent>(this, UCDashComponent::StaticClass(), TEXT("DashComponent"));
		DashComponent->RegisterComponent();
	}
	SpringArm->bEnableCameraLag = true;

}
void ACPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ACPlayerCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UCEnhancedInputComponent* CEnhancedInputComponent = Cast<UCEnhancedInputComponent>(PlayerInputComponent);
	check(CEnhancedInputComponent);
	// 기본 이동 및 시야 입력
	CEnhancedInputComponent->BindActionByTag(InputConfig, CGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ACPlayerCharacter::Input_Move);
	CEnhancedInputComponent->BindActionByTag(InputConfig, CGameplayTags::InputTag_Look, ETriggerEvent::Triggered, this, &ACPlayerCharacter::Input_Look);
	CEnhancedInputComponent->BindActionByTag(InputConfig, CGameplayTags::InputTag_Dash, ETriggerEvent::Started, this, &ACPlayerCharacter::DashStart);

}

void ACPlayerCharacter::Input_Move(const FInputActionValue& InputActionValue)
{
	// 입력 벡터 추출
	FVector2D MovementVector = InputActionValue.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ACPlayerCharacter::Input_Look(const FInputActionValue& InputActionValue)
{
	FVector2D LookAxisVector = InputActionValue.Get<FVector2D>();
 
	if (Controller != nullptr)
	{
		AddControllerYawInput(LookAxisVector.X / 5);
		AddControllerPitchInput(LookAxisVector.Y / -5);
	}
}



FVector ACPlayerCharacter::GetPawnViewLocation() const
{
	return Super::GetPawnViewLocation();
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


